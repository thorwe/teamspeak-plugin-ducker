#include "mod_ducker_global.h"

#include "core/ts_functions.h"
#include "core/ts_helpers_qt.h"

#include <QtCore/QSettings>

#include <utility>

using namespace com::teamspeak::pluginsdk;
using namespace thorwe;

Ducker_Global::Ducker_Global(Plugin_Base &plugin)
    : m_talkers(plugin.talkers())
{
    setObjectName(QStringLiteral("GlobalDucker"));
    m_isPrintEnabled = false;
    setParent(&plugin);
}

float Ducker_Global::getValue() const
{
    return m_value;
}

void Ducker_Global::setActive(bool val)
{
    if (val == m_active)
        return;

    m_active = val;
    // Log(QString("setActive: %1").arg((value)?"true":"false"),LogLevel_DEBUG);
    m_vols.do_for_each([val](DspVolumeDucker *volume) { volume->set_gain_adjustment(val); });
}

bool Ducker_Global::onInfoDataChanged(
uint64 connection_id, uint64 id, PluginItemType type, uint64 mine, QTextStream &data)
{
    if (!running())
        return false;

    auto dirty = false;
    if (type == PLUGIN_CLIENT)
    {
        auto plugin = qobject_cast<Plugin_Base *>(parent());
        const auto &plugin_id = plugin->id();
        funcs::set_plugin_menu_enabled(plugin_id, m_ContextMenuToggleMusicBot, (id != mine) ? 1 : 0);

        if ((id != mine) && isClientMusicBot(connection_id, static_cast<anyID>(id)))
        {
            data << this->objectName() << ":";
            dirty = true;

            data << "Global Ducking Target (MusicBot)";
        }
    }
    return dirty;
}

void Ducker_Global::onContextMenuEvent(uint64 connection_id,
                                       PluginMenuType type,
                                       int menu_item_id,
                                       uint64 selected_item_id)
{
    Q_UNUSED(type);
    if (menu_item_id == m_ContextMenuToggleMusicBot)
        ToggleMusicBot(connection_id, static_cast<anyID>(selected_item_id));
}

void Ducker_Global::onConnectStatusChanged(uint64 connection_id, int new_status, unsigned int error_number)
{
    m_vols.onConnectStatusChanged(connection_id, new_status, error_number);
}

void Ducker_Global::AddMusicBot(uint64 connection_id, anyID client_id)
{
    const auto [error_talking, talking] =
    funcs::get_client_property_as_int(connection_id, client_id, CLIENT_FLAG_TALKING);
    if (ts_errc::ok != error_talking)
        return;

    const auto [error_my_id, my_id] = funcs::get_client_id(connection_id);
    if (ts_errc::ok != error_my_id)
    {
        Error("AddMusicBot", connection_id, error_my_id);
        return;
    }

    const auto [error_my_channel_id, my_channel_id] = funcs::get_channel_of_client(connection_id, my_id);
    if (ts_errc::ok != error_my_channel_id)
    {
        Error("(AddMusicBot) Error getting Client Channel Id", connection_id, error_my_channel_id);
        return;
    }

    const auto [error_target_channel_id, target_channel_id] =
    funcs::get_channel_of_client(connection_id, client_id);
    if (ts_errc::ok != error_target_channel_id)
    {
        Error("(AddMusicBot) Error getting Client Channel Id", connection_id, error_target_channel_id);
        return;
    }

    if (target_channel_id != my_channel_id)
        return;

    auto *vol = AddMusicBotVolume(connection_id, client_id);
    if (vol)
    {
        if ((STATUS_TALKING == talking) && (!isActive()))
            setActive(true);

        vol->set_processing(STATUS_TALKING == talking);
    }
}

void Ducker_Global::RemoveMusicBot(uint64 connection_id, anyID client_id)
{
    auto [error, result] =
    funcs::get_client_property_as_string(connection_id, client_id, CLIENT_UNIQUE_IDENTIFIER);
    if (ts_errc::ok != error)
        return;

    m_duck_targets.remove(std::move(result));
    m_vols.delete_item(connection_id, client_id);
}

void Ducker_Global::ToggleMusicBot(uint64 connection_id, anyID client_id)
{
    auto [error_client_uid, client_uid] =
    funcs::get_client_property_as_string(connection_id, client_id, CLIENT_UNIQUE_IDENTIFIER);
    if (ts_errc::ok != error_client_uid)
    {
        Error("(ToggleMusicBot)", connection_id, error_client_uid);
        return;
    }

    if (m_duck_targets.contains(client_uid))
    {
        m_duck_targets.remove(client_uid);
        UpdateActive();
        m_vols.delete_item(connection_id, client_id);
    }
    else
    {
        auto [error_display_name, display_name] = funcs::get_client_display_name(connection_id, client_id);
        if (ts_errc::ok != error_display_name)
        {
            Error("(ToggleMusicBot) Error getting client display name", connection_id, error_display_name);
            return;
        }
        m_duck_targets.insert(std::move(client_uid), std::move(display_name));
        UpdateActive();
        AddMusicBotVolume(connection_id, client_id);
    }
    SaveDuckTargets();
}

bool Ducker_Global::isClientMusicBot(uint64 connection_id, anyID client_id)
{
    const auto [error_client_uid, client_uid] =
    funcs::get_client_property_as_string(connection_id, client_id, CLIENT_UNIQUE_IDENTIFIER);
    if (ts_errc::ok != error_client_uid)
    {
        Error("(isClientMusicBot)", connection_id, error_client_uid);
        return false;
    }

    return (m_duck_targets.contains(client_uid));
}

bool Ducker_Global::isClientMusicBotRt(uint64 connection_id, anyID client_id)
{
    return m_vols.contains(connection_id, client_id);
}

void Ducker_Global::onClientMoveEvent(uint64 connection_id,
                                      anyID client_id,
                                      uint64 old_channel_id,
                                      uint64 new_channel_id,
                                      int visibility,
                                      anyID my_id)
{
    Q_UNUSED(visibility);

    if (client_id == my_id)  // I have switched channels
    {
        m_vols.delete_items(connection_id);

        const auto [error_channel_client_ids, channel_client_ids] =
        funcs::get_channel_client_ids(connection_id, new_channel_id);
        if (ts_errc::ok != error_channel_client_ids)
            Error("(onClientMoveEvent): Error getting Channel Client List", connection_id,
                  error_channel_client_ids);
        else
        {
            for (const auto channel_client_id : channel_client_ids)
            {
                if (channel_client_id == my_id)
                    continue;

                if (isClientMusicBot(connection_id, channel_client_id))
                    AddMusicBotVolume(connection_id, channel_client_id);
            }
        }
    }
    else  // Someone else has...
    {
        const auto [error_my_channel_id, my_channel_id] = funcs::get_channel_of_client(connection_id, my_id);
        if (ts_errc::ok != error_my_channel_id)
            Error("(onClientMoveEvent) Error getting Client Channel Id", connection_id, error_my_channel_id);
        else
        {
            if (my_channel_id == old_channel_id)  // left
            {
                m_vols.delete_item(connection_id, client_id);
            }
            else if (my_channel_id == new_channel_id)  // joined
            {
                if (isClientMusicBot(connection_id, client_id))
                    AddMusicBotVolume(connection_id, client_id);
            }
        }
    }
}

//! Routes the arguments of the event to the corresponding volume object
/*!
 * \brief Ducker_Global::onEditPlaybackVoiceDataEvent pre-processing voice event
 * \param serverConnectionHandlerID the connection id of the server
 * \param clientID the client-side runtime-id of the sender
 * \param samples the sample array to manipulate
 * \param sampleCount amount of samples in the array
 * \param channels currently always 1 on TS3; unused
 * \return true, if the ducker has processed / the client is a music bot
 */
bool Ducker_Global::onEditPlaybackVoiceDataEvent(
uint64 connection_id, anyID client_id, short *samples, int frame_count, int channels)
{
    if (!(running()))
        return false;

    auto samples_ = gsl::span<int16_t>{samples, static_cast<size_t>(frame_count * channels)};
    return m_vols.do_for(
    [samples_, channels](DspVolumeDucker *volume) -> bool {
        if (!volume)
            return false;

        volume->process(samples_, channels);
        return (volume->processing() && volume->gain_adjustment());
    },
    connection_id, client_id);
}

void Ducker_Global::onRunningStateChanged(bool value)
{
    if (m_ContextMenuToggleMusicBot == -1)
    {
        auto plugin = qobject_cast<Plugin_Base *>(parent());
        auto &context_menu = plugin->context_menu();
        m_ContextMenuToggleMusicBot =
        context_menu.Register(this, PLUGIN_MENU_TYPE_CLIENT, "Toggle Global Ducking Target", "duck_16.png");
        connect(&context_menu, &TSContextMenu::FireContextMenuEvent, this, &Ducker_Global::onContextMenuEvent,
                Qt::AutoConnection);
    }

    if (value)
    {
        connect(&m_talkers, &Talkers::ConnectStatusChanged, this, &Ducker_Global::onConnectStatusChanged,
                Qt::UniqueConnection);

        const auto [error_connection_ids, connection_ids] = funcs::get_server_connection_handler_ids();
        if (ts_errc::ok == error_connection_ids)
        {
            for (const auto connection_id : connection_ids)
            {
                {
                    const auto [error_connection_status, connection_status] =
                    funcs::get_connection_status(connection_id);
                    if (ts_errc::ok != error_connection_status)
                        continue;

                    if (STATUS_CONNECTION_ESTABLISHED != connection_status)
                        continue;
                }

                const auto [error_my_id, my_id] = funcs::get_client_id(connection_id);
                if (ts_errc::ok != error_my_id)
                {
                    Error("onRunningStateChanged", connection_id, error_my_id);
                    continue;
                }

                const auto [error_my_channel_id, my_channel_id] =
                funcs::get_channel_of_client(connection_id, my_id);
                if (ts_errc::ok != error_my_channel_id)
                    Error("(onRunningStateChanged) Could not get channel of client.", connection_id,
                          error_my_channel_id);
                else
                    onClientMoveEvent(connection_id, my_id, 0, my_channel_id, RETAIN_VISIBILITY, my_id);
            }

            UpdateActive();
        }
    }
    else
    {
        disconnect(&m_talkers, &Talkers::ConnectStatusChanged, this, &Ducker_Global::onConnectStatusChanged);
        setActive(false);
        m_vols.delete_items();
    }
    auto plugin = qobject_cast<Plugin_Base *>(parent());
    auto &info_data = plugin->info_data();
    info_data.Register(this, value, 1);
    Log(QString("enabled: %1").arg((value) ? "true" : "false"));
}

void Ducker_Global::setValue(float val)
{
    if (val == m_value)
        return;

    m_value = val;
    Log(QString("setValue: %1").arg(m_value));
    m_vols.do_for_each([val](DspVolumeDucker *volume) { volume->set_gain_desired(val); });
}

void Ducker_Global::onTalkStatusChanged(
uint64 connection_id, int status, bool is_received_whisper, anyID client_id, bool is_me)
{
    Q_UNUSED(is_received_whisper);

    if (is_me || !running())
        return;

    if (m_duck_targets.isEmpty())
        return;

    // Compute if this change causes a ducking change
    if ((!isActive()) && (status == STATUS_TALKING))
    {
        if (!isClientMusicBotRt(connection_id, client_id))
            setActive(true);
    }
    else if (isActive() && (status == STATUS_NOT_TALKING))
        UpdateActive();

    if ((status == STATUS_TALKING) || (status == STATUS_NOT_TALKING))
    {
        m_vols.do_for(
        [status](DspVolumeDucker *volume) {
            if (!volume)
                return;

            volume->set_processing(status == STATUS_TALKING);
        },
        connection_id, client_id);
    }
}

DspVolumeDucker *Ducker_Global::AddMusicBotVolume(uint64 connection_id, anyID client_id)
{
    auto result = m_vols.add_volume(connection_id, client_id);
    auto *vol = result.first;
    if (vol)
    {
        vol->set_gain_desired(m_value);
        vol->set_gain_adjustment(m_active);
    }
    return vol;
}

void Ducker_Global::SaveDuckTargets()
{
    QSettings cfg(TSHelpers::GetPath(teamspeak::plugin::Path::PluginIni), QSettings::IniFormat);
    cfg.beginGroup(QStringLiteral("ducker_global"));
    const auto kOldSize = cfg.beginReadArray(QStringLiteral("targets"));
    cfg.endArray();
    cfg.beginWriteArray(QStringLiteral("targets"), m_duck_targets.size());
    QMapIterator<std::string, std::string> i(m_duck_targets);
    int count = 0;
    while (i.hasNext())
    {
        i.next();
        cfg.setArrayIndex(count);
        cfg.setValue(QStringLiteral("uid"), QString::fromStdString(i.key()));
        cfg.setValue(QStringLiteral("name"), QString::fromStdString(i.value()));
        ++count;
    }
    if (kOldSize > m_duck_targets.size())
    {
        for (auto j = m_duck_targets.size(); j < kOldSize; ++j)
        {
            cfg.setArrayIndex(j);
            cfg.remove(QStringLiteral("uid"));
            cfg.remove(QStringLiteral("name"));
        }
    }
    cfg.endArray();
    cfg.endGroup();
}

void Ducker_Global::UpdateActive()
{
    auto is_active = false;

    const auto &talkers = m_talkers.GetTalkerMap();
    for (auto it = talkers.cbegin(); it != talkers.cend(); ++it)
    {
        if (!isClientMusicBotRt(it.key(), it.value()))
        {
            is_active = true;
            break;
        }
    }

    if (!is_active)
    {
        const auto &whisperers = m_talkers.GetWhisperMap();
        for (auto it = whisperers.cbegin(); it != whisperers.cend(); ++it)
        {
            if (!isClientMusicBotRt(it.key(), it.value()))
            {
                is_active = true;
                break;
            }
        }
    }

    setActive(is_active);
}

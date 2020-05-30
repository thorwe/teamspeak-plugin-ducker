#include "mod_ducker_global.h"

#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "ts3_functions.h"

#include "plugin.h"

#include <QtCore/QSettings>
#include "core/ts_helpers_qt.h"

using namespace thorwe;

Ducker_Global::Ducker_Global(Plugin_Base& plugin)
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
    //Log(QString("setActive: %1").arg((value)?"true":"false"),LogLevel_DEBUG);
    m_vols.do_for_each([val](DspVolumeDucker* volume)
    {
        volume->set_gain_adjustment(val);
    });
}

bool Ducker_Global::onInfoDataChanged(uint64 connection_id, uint64 id, PluginItemType type, uint64 mine, QTextStream &data)
{
    if (!isRunning())
        return false;

    auto dirty = false;
    if (type == PLUGIN_CLIENT)
    {
		auto plugin = qobject_cast<Plugin_Base*>(parent());
		const auto kPluginId = plugin->id();
        ts3Functions.setPluginMenuEnabled(kPluginId.c_str(), m_ContextMenuToggleMusicBot, (id != mine) ? 1 : 0);

        if ((id != mine) && isClientMusicBot(connection_id, static_cast<anyID>(id)))
        {
            data << this->objectName() << ":";
            dirty = true;

            data << "Global Ducking Target (MusicBot)";
        }
    }
    return dirty;
}

void Ducker_Global::onContextMenuEvent(uint64 connection_id, PluginMenuType type, int menu_item_id, uint64 selected_item_id)
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
    int talking;
    if (ts3Functions.getClientVariableAsInt(connection_id, client_id, CLIENT_FLAG_TALKING, &talking) == ERROR_ok)
    {
        unsigned int error;

        // Get My Id on this handler
        anyID my_id;
        if ((error = ts3Functions.getClientID(connection_id, &my_id)) != ERROR_ok)
        {
            Error("AddMusicBot", connection_id, error);
            return;
        }

        // Get My channel on this handler
        uint64 my_channel_id;
        if ((error = ts3Functions.getChannelOfClient(connection_id, my_id, &my_channel_id)) != ERROR_ok)
            Error("(AddMusicBot) Error getting Client Channel Id", connection_id, error);
        else
        {
            uint64 channel_id;
            if ((error = ts3Functions.getChannelOfClient(connection_id, client_id, &channel_id)) != ERROR_ok)
                Error("(AddMusicBot) Error getting Client Channel Id", connection_id, error);
            else
            {
                if (channel_id != my_channel_id)
                    return;

                auto* vol = AddMusicBotVolume(connection_id, client_id);
                if (vol)
                {
                    if ((talking == STATUS_TALKING) && (!isActive()))
                        setActive(true);

                    vol->set_processing(talking == STATUS_TALKING);
                }
            }
        }
    }
}

void Ducker_Global::RemoveMusicBot(uint64 connection_id, anyID client_id)
{
    QString uid;
    if (TSHelpers::GetClientUID(connection_id, client_id, uid) != ERROR_ok)
        return;

    m_duck_targets.remove(uid);
    m_vols.delete_item(connection_id, client_id);
}

void Ducker_Global::ToggleMusicBot(uint64 connection_id, anyID client_id)
{
    //Log(QString("(ToggleMusicBot) %1").arg(clientID),serverConnectionHandlerID,LogLevel_DEBUG);
    unsigned int error;

    QString uid;
    if (TSHelpers::GetClientUID(connection_id, client_id, uid) != ERROR_ok)
    {
        Error("(ToggleMusicBot)");
        return;
    }

    if (m_duck_targets.contains(uid))
    {
        m_duck_targets.remove(uid);
        UpdateActive();
        m_vols.delete_item(connection_id, client_id);
    }
    else
    {
        char name[512];
        if((error = ts3Functions.getClientDisplayName(connection_id, client_id, name, 512)) != ERROR_ok)
        {
            Error("(ToggleMusicBot) Error getting client display name", connection_id, error);
            return;
        }
        m_duck_targets.insert(uid, name);
        UpdateActive();
        AddMusicBotVolume(connection_id, client_id);
    }
    SaveDuckTargets();
}

bool Ducker_Global::isClientMusicBot(uint64 connection_id, anyID client_id)
{
    QString uid;
    if (TSHelpers::GetClientUID(connection_id, client_id, uid) != ERROR_ok)
        return false;

    return (m_duck_targets.contains(uid));
}

bool Ducker_Global::isClientMusicBotRt(uint64 connection_id, anyID client_id)
{
    return m_vols.contains(connection_id, client_id);
}

void Ducker_Global::onClientMoveEvent(uint64 connection_id, anyID client_id, uint64 old_channel_id, uint64 new_channel_id, int visibility, anyID my_id)
{
    Q_UNUSED(visibility);

    unsigned int error;

    if (client_id == my_id)                   // I have switched channels
    {
        m_vols.delete_items(connection_id);

        // Get Channel Client List
        anyID* clients;
        if((error = ts3Functions.getChannelClientList(connection_id, new_channel_id, &clients)) != ERROR_ok)
            Error("(onClientMoveEvent): Error getting Channel Client List", connection_id, error);
        else
        {
            // for every GDT client insert volume
            for (int i = 0; clients[i]; i++)
            {
                if (clients[i] == my_id)
                    continue;

                if (isClientMusicBot(connection_id, clients[i]))
                    AddMusicBotVolume(connection_id, clients[i]);
            }
        }
    }
    else                                    // Someone else has...
    {
        // Get My channel on this handler
        uint64 channelID;
        if ((error = ts3Functions.getChannelOfClient(connection_id, my_id, &channelID)) != ERROR_ok)
            Error("(onClientMoveEvent) Error getting Client Channel Id", connection_id, error);
        else
        {
            if (channelID == old_channel_id)      // left
            {
                m_vols.delete_item(connection_id, client_id);
            }
            else if (channelID == new_channel_id) // joined
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
bool Ducker_Global::onEditPlaybackVoiceDataEvent(uint64 connection_id, anyID client_id, short *samples, int frame_count, int channels)
{
    if (!(isRunning()))
        return false;

    auto samples_ = gsl::span<int16_t>{samples, static_cast<size_t>(frame_count * channels)};
    return m_vols.do_for([samples_, channels](DspVolumeDucker* volume) -> bool
    {
        volume->process(samples_, channels);
        return (volume->processing() && volume->gain_adjustment());
    }, connection_id, client_id);
}

void Ducker_Global::onRunningStateChanged(bool value)
{
    if (m_ContextMenuToggleMusicBot == -1)
    {
		auto plugin = qobject_cast<Plugin_Base*>(parent());
		auto& context_menu = plugin->context_menu();
        m_ContextMenuToggleMusicBot = context_menu.Register(this, PLUGIN_MENU_TYPE_CLIENT, "Toggle Global Ducking Target", "duck_16.png");
        connect(&context_menu, &TSContextMenu::FireContextMenuEvent, this, &Ducker_Global::onContextMenuEvent, Qt::AutoConnection);
    }

    if (value)
    {
        connect(&m_talkers, &Talkers::ConnectStatusChanged, this, &Ducker_Global::onConnectStatusChanged, Qt::UniqueConnection);

        uint64* servers;
        if (ts3Functions.getServerConnectionHandlerList(&servers) == ERROR_ok)
        {
            for (auto server = servers; *server; ++server)
            {
                {
                    int status;
                    if (ts3Functions.getConnectionStatus(*server, &status) != ERROR_ok)
                        continue;

                    if (status != STATUS_CONNECTION_ESTABLISHED)
                        continue;
                }

                unsigned int error;

                // Get My Id on this handler
                anyID my_id;
                if ((error = ts3Functions.getClientID(*server, &my_id)) != ERROR_ok)
                {
                    Error("onRunningStateChanged",*server,error);
                    continue;
                }

                // Get My channel on this handler
                uint64 channel_id;
                if((error = ts3Functions.getChannelOfClient(*server, my_id, &channel_id)) != ERROR_ok)
                    Error("(onRunningStateChanged) Could not get channel of client.", *server, error);
                else
                    onClientMoveEvent(*server, my_id, 0, channel_id, RETAIN_VISIBILITY, my_id);

            }
            ts3Functions.freeMemory(servers);

            UpdateActive();
        }
    }
    else
    {
        disconnect(&m_talkers, &Talkers::ConnectStatusChanged, this, &Ducker_Global::onConnectStatusChanged);
        setActive(false);
        m_vols.delete_items();
    }
	auto plugin = qobject_cast<Plugin_Base*>(parent());
	auto& info_data = plugin->info_data();
    info_data.Register(this, value, 1);
    Log(QString("enabled: %1").arg((value) ? "true" : "false"));
}

void Ducker_Global::setValue(float val)
{
    if (val == m_value)
        return;

    m_value = val;
    Log(QString("setValue: %1").arg(m_value));
    m_vols.do_for_each([val](DspVolumeDucker* volume)
    {
        volume->set_gain_desired(val);
    });
}

void Ducker_Global::onTalkStatusChanged(uint64 connection_id, int status, bool is_received_whisper, anyID client_id, bool is_me)
{
    Q_UNUSED(is_received_whisper);

    if (is_me || !isRunning())
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
        m_vols.do_for([status](DspVolumeDucker* volume)
        {
            volume->set_processing(status == STATUS_TALKING);
        }, connection_id, client_id);
    }
}

DspVolumeDucker* Ducker_Global::AddMusicBotVolume(uint64 connection_id, anyID client_id)
{
    auto result = m_vols.add_volume(connection_id, client_id);
    auto* vol = result.first;
    if (vol)
    {
        vol->set_gain_desired(m_value);
        vol->set_gain_adjustment(m_active);
    }
    return vol;
}

void Ducker_Global::SaveDuckTargets()
{
    QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
    cfg.beginGroup(QStringLiteral("ducker_global"));
    const auto kOldSize = cfg.beginReadArray(QStringLiteral("targets"));
    cfg.endArray();
    cfg.beginWriteArray(QStringLiteral("targets"), m_duck_targets.size());
    QMapIterator<QString,QString> i(m_duck_targets);
    int count = 0;
    while (i.hasNext())
    {
        i.next();
        cfg.setArrayIndex(count);
        cfg.setValue(QStringLiteral("uid"),i.key());
        cfg.setValue(QStringLiteral("name"),i.value());
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

    const auto& talkers = m_talkers.GetTalkerMap();
    for (auto it = talkers.cbegin(); it != talkers.cend(); ++it)
    {
        if (!isClientMusicBotRt(it.key(),it.value()))
        {
            is_active = true;
            break;
        }
    }

    if (!is_active)
    {
        const auto& whisperers = m_talkers.GetWhisperMap();
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

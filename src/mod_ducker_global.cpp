#include "mod_ducker_global.h"

#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "ts3_functions.h"

#include "plugin.h"

#include <QtCore/QSettings>
#include "core/ts_helpers_qt.h"

Ducker_Global::Ducker_Global(Plugin_Base& plugin)
	: m_talkers(plugin.talkers())
{
	setObjectName(QStringLiteral("GlobalDucker"));
    m_isPrintEnabled = false;
    setParent(&plugin);

    vols = new Volumes(this, Volumes::Volume_Type::DUCKER);
}

float Ducker_Global::getValue() const
{
    return m_value;
}

void Ducker_Global::setActive(bool value)
{
    if (value == m_active)
        return;

    m_active = value;
    //Log(QString("setActive: %1").arg((value)?"true":"false"),LogLevel_DEBUG);
    emit activeSet(m_active);
}

bool Ducker_Global::onInfoDataChanged(uint64 server_connection_handler_id, uint64 id, PluginItemType type, uint64 mine, QTextStream &data)
{
    if (!isRunning())
        return false;

    auto dirty = false;
    if (type == PLUGIN_CLIENT)
    {
		auto plugin = qobject_cast<Plugin_Base*>(parent());
		const auto kPluginId = plugin->id();
        ts3Functions.setPluginMenuEnabled(kPluginId.c_str(), m_ContextMenuToggleMusicBot, (id != mine) ? 1 : 0);

        if ((id != mine) && isClientMusicBot(server_connection_handler_id, static_cast<anyID>(id)))
        {
            data << this->objectName() << ":";
            dirty = true;

            data << "Global Ducking Target (MusicBot)";
        }
    }
    return dirty;
}

void Ducker_Global::onContextMenuEvent(uint64 server_connection_handler_id, PluginMenuType type, int menu_item_id, uint64 selected_item_id)
{
    Q_UNUSED(type);
    if (menu_item_id == m_ContextMenuToggleMusicBot)
        ToggleMusicBot(server_connection_handler_id, static_cast<anyID>(selected_item_id));
}

void Ducker_Global::AddMusicBot(uint64 server_connection_handler_id, anyID client_id)
{
    int talking;
    if (ts3Functions.getClientVariableAsInt(server_connection_handler_id, client_id, CLIENT_FLAG_TALKING, &talking) == ERROR_ok)
    {
        unsigned int error;

        // Get My Id on this handler
        anyID my_id;
        if ((error = ts3Functions.getClientID(server_connection_handler_id, &my_id)) != ERROR_ok)
        {
            Error("AddMusicBot", server_connection_handler_id, error);
            return;
        }

        // Get My channel on this handler
        uint64 my_channel_id;
        if ((error = ts3Functions.getChannelOfClient(server_connection_handler_id, my_id, &my_channel_id)) != ERROR_ok)
            Error("(AddMusicBot) Error getting Client Channel Id", server_connection_handler_id, error);
        else
        {
            uint64 channel_id;
            if ((error = ts3Functions.getChannelOfClient(server_connection_handler_id, client_id, &channel_id)) != ERROR_ok)
                Error("(AddMusicBot) Error getting Client Channel Id", server_connection_handler_id, error);
            else
            {
                if (channel_id != my_channel_id)
                    return;

                auto vol = qobject_cast<DspVolumeDucker*>(AddMusicBotVolume(server_connection_handler_id, client_id));
                if (vol)
                {
                    if ((talking == STATUS_TALKING) && (!isActive()))
                        setActive(true);

                    vol->setProcessing(talking == STATUS_TALKING);
                }
            }
        }
    }
}

void Ducker_Global::RemoveMusicBot(uint64 server_connection_handler_id, anyID client_id)
{
    QString uid;
    if (TSHelpers::GetClientUID(server_connection_handler_id, client_id, uid) != ERROR_ok)
        return;

    m_duck_targets.remove(uid);
    vols->RemoveVolume(server_connection_handler_id, client_id);
}

void Ducker_Global::ToggleMusicBot(uint64 server_connection_handler_id, anyID client_id)
{
    //Log(QString("(ToggleMusicBot) %1").arg(clientID),serverConnectionHandlerID,LogLevel_DEBUG);
    unsigned int error;

    QString uid;
    if (TSHelpers::GetClientUID(server_connection_handler_id, client_id, uid) != ERROR_ok)
    {
        Error("(ToggleMusicBot)");
        return;
    }

    if (m_duck_targets.contains(uid))
    {
        m_duck_targets.remove(uid);
        UpdateActive();
        vols->RemoveVolume(server_connection_handler_id, client_id);
    }
    else
    {
        char name[512];
        if((error = ts3Functions.getClientDisplayName(server_connection_handler_id, client_id, name, 512)) != ERROR_ok)
        {
            Error("(ToggleMusicBot) Error getting client display name", server_connection_handler_id, error);
            return;
        }
        m_duck_targets.insert(uid, name);
        UpdateActive();
        AddMusicBotVolume(server_connection_handler_id, client_id);
    }
    SaveDuckTargets();
}

bool Ducker_Global::isClientMusicBot(uint64 server_connection_handler_id, anyID client_id)
{
    QString uid;
    if (TSHelpers::GetClientUID(server_connection_handler_id, client_id, uid) != ERROR_ok)
        return false;

    return (m_duck_targets.contains(uid));
}

bool Ducker_Global::isClientMusicBotRt(uint64 server_connection_handler_id, anyID client_id)
{
    return vols->ContainsVolume(server_connection_handler_id, client_id);
}

void Ducker_Global::onClientMoveEvent(uint64 server_connection_handler_id, anyID client_id, uint64 old_channel_id, uint64 new_channel_id, int visibility, anyID my_id)
{
    Q_UNUSED(visibility);

    unsigned int error;

    if (client_id == my_id)                   // I have switched channels
    {
        vols->RemoveVolumes(server_connection_handler_id);

        // Get Channel Client List
        anyID* clients;
        if((error = ts3Functions.getChannelClientList(server_connection_handler_id, new_channel_id, &clients)) != ERROR_ok)
            Error("(onClientMoveEvent): Error getting Channel Client List", server_connection_handler_id, error);
        else
        {
            // for every GDT client insert volume
            for (int i = 0; clients[i]; i++)
            {
                if (clients[i] == my_id)
                    continue;

                if (isClientMusicBot(server_connection_handler_id, clients[i]))
                    AddMusicBotVolume(server_connection_handler_id, clients[i]);
            }
        }
    }
    else                                    // Someone else has...
    {
        // Get My channel on this handler
        uint64 channelID;
        if ((error = ts3Functions.getChannelOfClient(server_connection_handler_id, my_id, &channelID)) != ERROR_ok)
            Error("(onClientMoveEvent) Error getting Client Channel Id", server_connection_handler_id, error);
        else
        {
            if (channelID == old_channel_id)      // left
            {
                vols->RemoveVolume(server_connection_handler_id, client_id);
            }
            else if (channelID == new_channel_id) // joined
            {
                if (isClientMusicBot(server_connection_handler_id, client_id))
                    AddMusicBotVolume(server_connection_handler_id, client_id);
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
bool Ducker_Global::onEditPlaybackVoiceDataEvent(uint64 server_connection_handler_id, anyID client_id, short *samples, int frame_count, int channels)
{
    if (!(isRunning()))
        return false;

    auto vol = qobject_cast<DspVolumeDucker*>(vols->GetVolume(server_connection_handler_id, client_id));
    if (vol == 0)
        return false;

    vol->process(samples, frame_count, channels);
    return (vol->isProcessing() && vol->getGainAdjustment());
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
        connect(&m_talkers, &Talkers::ConnectStatusChanged, vols, &Volumes::onConnectStatusChanged, Qt::UniqueConnection);

        uint64* servers;
        if (ts3Functions.getServerConnectionHandlerList(&servers) == ERROR_ok)
        {
            for (auto server = servers; *server != (uint64)NULL; ++server)
            {
                int status;
                if (ts3Functions.getConnectionStatus(*server, &status) != ERROR_ok)
                    continue;

                if (status != STATUS_CONNECTION_ESTABLISHED)
                    continue;

                unsigned int error;

                // Get My Id on this handler
                anyID my_id;
                if ((error = ts3Functions.getClientID(*server,&my_id)) != ERROR_ok)
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
        disconnect(&m_talkers, &Talkers::ConnectStatusChanged, vols, &Volumes::onConnectStatusChanged);
        setActive(false);
        vols->RemoveVolumes();
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
    emit valueSet(m_value);
}

void Ducker_Global::onTalkStatusChanged(uint64 server_connection_handler_id, int status, bool is_received_whisper, anyID client_id, bool is_me)
{
    Q_UNUSED(is_received_whisper);

    if (is_me || !isRunning())
        return;

    if (m_duck_targets.isEmpty())
        return;

    // Compute if this change causes a ducking change
    if ((!isActive()) && (status == STATUS_TALKING))
    {
        if (!isClientMusicBotRt(server_connection_handler_id, client_id))
            setActive(true);
    }
    else if (isActive() && (status == STATUS_NOT_TALKING))
        UpdateActive();

    if ((status == STATUS_TALKING) || (status == STATUS_NOT_TALKING))
    {
        auto vol = qobject_cast<DspVolumeDucker*>(vols->GetVolume(server_connection_handler_id, client_id));
        if (vol)
            vol->setProcessing(status == STATUS_TALKING);
    }
}

DspVolumeDucker* Ducker_Global::AddMusicBotVolume(uint64 server_connection_handler_id, anyID client_id)
{
    auto vol = qobject_cast<DspVolumeDucker*>(vols->AddVolume(server_connection_handler_id, client_id));
    if (vol)
    {
        vol->setGainDesired(m_value);
        connect(this, &Ducker_Global::valueSet, vol, &DspVolumeDucker::setGainDesired, Qt::DirectConnection);
        vol->setGainAdjustment(m_active);
        connect(this, &Ducker_Global::activeSet, vol, &DspVolumeDucker::setGainAdjustment, Qt::DirectConnection);
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

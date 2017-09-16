#include "mod_ducker_channel.h"

#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "ts3_functions.h"

#include "plugin.h"
#include "core/plugin_base.h"
#include "core/ts_helpers_qt.h"

Ducker_Channel::Ducker_Channel(Plugin_Base& plugin)
	: m_talkers(plugin.talkers())
{
	setObjectName(QStringLiteral("ChannelDucker"));
    m_isPrintEnabled = false;
    setParent(&plugin);

    vols = new Volumes(this, Volumes::Volume_Type::DUCKER);
}

float Ducker_Channel::getValue() const
{
    return m_value;
}

bool Ducker_Channel::isTargetOtherTabs() const
{
    return m_isTargetOtherTabs;
}

bool Ducker_Channel::isDuckPrioritySpeakers() const
{
    return m_duck_priority_speakers;
}

// User Setting interaction

void Ducker_Channel::onRunningStateChanged(bool value)
{
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
                if((error = ts3Functions.getClientID(*server, &my_id)) != ERROR_ok)
                {
                    Error("onRunningStateChanged", *server, error);
                    continue;
                }

                // Get My channel on this handler
                uint64 channel_id;
                if((error = ts3Functions.getChannelOfClient(*server, my_id, &channel_id)) != ERROR_ok)
                    Error("(onRunningStateChanged) Could not get channel of client.",*server,error);
                else
                    onClientMoveEvent(*server, my_id, 0, channel_id, RETAIN_VISIBILITY, my_id);

            }
            ts3Functions.freeMemory(servers);

            //UpdateActive();   //?
            m_talkers.DumpTalkStatusChanges(this, STATUS_TALKING); // Should work just fine?
        }
    }
    else
    {
        disconnect(&m_talkers, &Talkers::ConnectStatusChanged, vols, &Volumes::onConnectStatusChanged);
        setActive(false);
        vols->RemoveVolumes();
    }
    Log(QString("enabled: %1").arg((value)?"true":"false"));
}

//! Set the amount of the volume reduction in dezibel(dB)
/*!
 * \brief Ducker_Channel::setValue
 * \param newValue reduction amount in dezibel (float)
 */
void Ducker_Channel::setValue(float val)
{
    if (val == m_value)
        return;

    m_value = val;
    Log(QString("setValue: %1").arg(m_value));
    emit valueSet(m_value);
}

void Ducker_Channel::setDuckingReverse(bool val)
{
    if (val == m_isTargetOtherTabs)
        return;

    m_isTargetOtherTabs = val;
    if (isRunning())  //since everything needs to be re-evaluated might as well toggle off/on
    {
        onRunningStateChanged(false); //setEnabled would trigger signal
        onRunningStateChanged(true);
    }
    Log(QString("isTargetOtherTabs: %1").arg((m_isTargetOtherTabs)?"true":"false"));
}

void Ducker_Channel::setDuckPrioritySpeakers(bool val)
{
    if (val == m_duck_priority_speakers)
        return;

    if (isRunning())  //since everything needs to be re-evaluated might as well toggle off/on
    {
        onRunningStateChanged(false); //setEnabled would trigger signal
        onRunningStateChanged(true);
    }
    m_duck_priority_speakers = val;
}

//void Ducker_Channel::saveSettings()
//{
//    QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
//    cfg.setValue("ducking_enabled", isEnabled());
//    cfg.setValue("ducking_value", getValue());
//    cfg.setValue("ducking_reverse", isTargetOtherTabs());
//}

void Ducker_Channel::setHomeId(uint64 server_connection_handler_id)
{
    if (server_connection_handler_id == m_homeId)
        return;

    uint64 oldHomeId = m_homeId;
    m_homeId = server_connection_handler_id;
    if (m_homeId == 0)
        return;

    UpdateActive();
    // Dump talk changes for new and old home id
//    talkers->DumpTalkStatusChanges(this,STATUS_TALKING); //ToDo: ServerConnectionHandler specific dump
    // don't need no whisper or self talk here
    auto map = m_talkers.GetTalkerMap();
    if (map.contains(oldHomeId))
    {
        auto list = map.values(oldHomeId);
        for (int i = 0; i<list.size(); ++i)
        {
            auto vol = qobject_cast<DspVolumeDucker*>(vols->GetVolume(oldHomeId,list[i]));
            if (vol)
                vol->setDuckBlocked(!m_isTargetOtherTabs);
        }
    }

    if (map.contains(m_homeId))
    {
        auto list = map.values(m_homeId);
        for (int i = 0; i<list.size(); ++i)
        {
            auto vol = qobject_cast<DspVolumeDucker*>(vols->GetVolume(m_homeId,list[i]));
            if (vol)
                vol->setDuckBlocked(m_isTargetOtherTabs);
        }
    }

    Log(QString("setHomeId: %1").arg(m_homeId));
}

void Ducker_Channel::setActive(bool value)
{
    if (value == m_isActive)
        return;

    m_isActive = value;
//    Log(QString("setActive: %1").arg((value)?"true":"false"),LogLevel_DEBUG);
    emit activeSet(m_isActive);
}

// ts event handlers

//! Have a volume object for everyone in the clients channel
void Ducker_Channel::onClientMoveEvent(uint64 server_connection_handler_id, anyID client_id, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID myID)
{
    Q_UNUSED(visibility);

    unsigned int error;

    if (client_id == myID)                   // I have switched channels
    {
//        if (visibility == ENTER_VISIBILITY)  // those are direct from connect status change, filter those out to only use from setHomeId (and normal ones)
//            return;

//        if ((oldChannelID == 0)  && (visibility == RETAIN_VISIBILITY)) // -> setHomeId clientMove
//            return;

//        Log("Refreshing volumes",LogLevel_DEBUG);
        vols->RemoveVolumes(server_connection_handler_id);

        // Get Channel Client List
        anyID* clients;
        if ((error = ts3Functions.getChannelClientList(server_connection_handler_id, newChannelID, &clients)) != ERROR_ok)
            Error("(onClientMoveEvent) Error getting Channel Client List", server_connection_handler_id, error);
        else
        {
            // for every client insert volume
            for (int i = 0; clients[i]; i++)
            {
                if (clients[i] == myID)
                    continue;

                AddDuckerVolume(server_connection_handler_id,clients[i]);
            }
        }
    }
    else                                    // Someone else has...
    {
        // Get My channel on this handler
        uint64 channelID;
        if((error = ts3Functions.getChannelOfClient(server_connection_handler_id,myID,&channelID)) != ERROR_ok)
            Error(QString("(onClientMoveEvent) Error getting my Client Channel Id (client_id: %1)").arg(client_id), server_connection_handler_id, error);
        else
        {
            if (channelID == oldChannelID)      // left
                vols->RemoveVolume(server_connection_handler_id,client_id);
            else if (channelID == newChannelID) // joined
                AddDuckerVolume(server_connection_handler_id,client_id);
        }
    }
}

bool Ducker_Channel::onTalkStatusChanged(uint64 server_connection_handler_id, int status, bool is_received_whisper, anyID client_id, bool is_me)
{
    if (!isRunning())
        return false;

    if (is_me && !m_isTargetOtherTabs)
        return false;

    // Compute if this change causes a ducking change
    if ((!isActive()) && (status == STATUS_TALKING))
    {
        if ((is_received_whisper) || (!m_isTargetOtherTabs && (server_connection_handler_id != m_homeId)) || (m_isTargetOtherTabs && (server_connection_handler_id == m_homeId)))
            setActive(true);
    }
    else if (isActive() && (status == STATUS_NOT_TALKING))
        UpdateActive();

    if (is_me)
        return false;

    if (((status == STATUS_TALKING) || (status == STATUS_NOT_TALKING)))
    {
        auto vol = qobject_cast<DspVolumeDucker*>(vols->GetVolume(server_connection_handler_id, client_id));
        if (!vol)
            return false;

        const auto is_trigger = (m_isTargetOtherTabs && (server_connection_handler_id == m_homeId)) || (!m_isTargetOtherTabs && (server_connection_handler_id != m_homeId));

        //non ideal i guess
        unsigned int error;
        int is_priority_speaker_duck = 0;
        if ((!m_duck_priority_speakers) && (status == STATUS_TALKING) && ((error = ts3Functions.getClientVariableAsInt(server_connection_handler_id, client_id, CLIENT_IS_PRIORITY_SPEAKER, &is_priority_speaker_duck)) != ERROR_ok))
            Error("(onTalkStatusChangeEvent)", server_connection_handler_id, error);

        vol->setDuckBlocked(is_trigger || ((is_received_whisper || (is_priority_speaker_duck)) && (status == STATUS_TALKING)));
        vol->setProcessing(status == STATUS_TALKING);
        return ((status == STATUS_TALKING) && !(is_received_whisper || is_priority_speaker_duck));
    }
    return false;
}

//! Routes the arguments of the event to the corresponding volume object
/*!
 * \brief Ducker_Channel::onEditPlaybackVoiceDataEvent pre-processing voice event
 * \param server_connection_handler_id the connection id of the server
 * \param client_id the client-side runtime-id of the sender
 * \param samples the sample array to manipulate
 * \param sampleCount amount of samples in the array
 * \param channels amount of channels
 */
void Ducker_Channel::onEditPlaybackVoiceDataEvent(uint64 server_connection_handler_id, anyID client_id, short *samples, int frame_count, int channels)
{
    if (!(isRunning()))
        return;

    if (((!m_isTargetOtherTabs) && (server_connection_handler_id != m_homeId)) || ((m_isTargetOtherTabs) && (server_connection_handler_id == m_homeId)))
        return;

    auto vol = qobject_cast<DspVolumeDucker*>(vols->GetVolume(server_connection_handler_id, client_id));
    if (vol == 0)
        return;

    //sampleCount = sampleCount * channels;
    vol->process(samples, frame_count, channels);
}

//! Create and add a Volume object to the ServerChannelVolumes map
/*!
 * \brief Ducker_Channel::AddDuckerVolume Helper function
 * \param server_connection_handler_id the connection id of the server
 * \param client_id the client id
 * \return a volume object
 */
DspVolumeDucker* Ducker_Channel::AddDuckerVolume(uint64 server_connection_handler_id, anyID client_id)
{
    auto vol = qobject_cast<DspVolumeDucker*>(vols->AddVolume(server_connection_handler_id,client_id));
    if (vol != 0)
    {
        vol->setGainDesired(m_value);
        connect(this, &Ducker_Channel::valueSet, vol, &DspVolumeDucker::setGainDesired, Qt::DirectConnection);
        vol->setGainAdjustment(m_isActive);
        connect(this, &Ducker_Channel::activeSet, vol, &DspVolumeDucker::setGainAdjustment, Qt::DirectConnection);
    }
//    Log(QString("Ducker: Added %1 to ServerChannelVolumes.").arg(client_id),server_connection_handler_id,LogLevel_DEBUG);
    return vol;
}

void Ducker_Channel::UpdateActive()
{
    bool is_active = false;

    if (m_isTargetOtherTabs && (m_talkers.isMeTalking() != 0))
        is_active = true;
    else if (!(m_talkers.GetWhisperMap().isEmpty()))
        is_active=true;
    else
    {
        if (m_isTargetOtherTabs)
        {
            if (m_talkers.GetTalkerMap().contains(m_homeId))
                is_active=true;
        }
        else
        {
            if (m_talkers.GetTalkerMap().count() != m_talkers.GetTalkerMap().count(m_homeId))
                is_active = true;
        }
    }
    setActive(is_active);
}

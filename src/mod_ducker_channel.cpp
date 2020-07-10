#include "mod_ducker_channel.h"

#include "core/plugin_base.h"
#include "core/ts_functions.h"
#include "core/ts_helpers_qt.h"

#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"

#include <gsl/span>

using namespace com::teamspeak::pluginsdk;
using namespace thorwe;

Ducker_Channel::Ducker_Channel(Plugin_Base &plugin)
    : m_talkers(plugin.talkers())
{
    setObjectName(QStringLiteral("ChannelDucker"));
    setParent(&plugin);
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
        const auto [error_connection_ids, connection_ids] = funcs::get_server_connection_handler_ids();
        if (ts_errc::ok == error_connection_ids)
        {
            for (const auto connection_id : connection_ids)
            {
                const auto [error_connection_status, connection_status] =
                funcs::get_connection_status(connection_id);
                if (ts_errc::ok != error_connection_status)
                    continue;

                if (STATUS_CONNECTION_ESTABLISHED != connection_status)
                    continue;

                const auto [error_my_id, my_id] = funcs::get_client_id(connection_id);
                if (ts_errc::ok != error_my_id)
                {
                    Error("onRunningStateChanged", connection_id, error_my_id);
                    continue;
                }

                const auto [error_my_channel_id, my_channel_id] =
                funcs::get_channel_of_client(connection_id, my_id);
                if (ts_errc::ok != error_my_channel_id)
                {
                    Error("(onRunningStateChanged) Could not get channel of client.", connection_id,
                          error_my_channel_id);
                }
                else
                    onClientMoveEvent(connection_id, my_id, 0, my_channel_id, RETAIN_VISIBILITY, my_id);
            }
            m_talkers.DumpTalkStatusChanges(this, STATUS_TALKING);  // Should work just fine?
        }
    }
    else
    {
        setActive(false);
        m_vols.delete_items();
    }
    Log(QString("enabled: %1").arg((value) ? "true" : "false"));
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
    m_vols.do_for_each([val](DspVolumeDucker *volume) -> void { volume->set_gain_desired(val); });
}

void Ducker_Channel::setDuckingReverse(bool val)
{
    if (val == m_isTargetOtherTabs)
        return;

    m_isTargetOtherTabs = val;
    if (running())  // since everything needs to be re-evaluated might as well toggle off/on
    {
        onRunningStateChanged(false);  // setEnabled would trigger signal
        onRunningStateChanged(true);
    }
    Log(QString("isTargetOtherTabs: %1").arg((m_isTargetOtherTabs) ? "true" : "false"));
}

void Ducker_Channel::setDuckPrioritySpeakers(bool val)
{
    if (val == m_duck_priority_speakers)
        return;

    if (running())  // since everything needs to be re-evaluated might as well toggle off/on
    {
        onRunningStateChanged(false);  // setEnabled would trigger signal
        onRunningStateChanged(true);
    }
    m_duck_priority_speakers = val;
}

// void Ducker_Channel::saveSettings()
//{
//    QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
//    cfg.setValue("ducking_enabled", isEnabled());
//    cfg.setValue("ducking_value", getValue());
//    cfg.setValue("ducking_reverse", isTargetOtherTabs());
//}

void Ducker_Channel::setHomeId(uint64 connection_id)
{
    if (connection_id == m_homeId)
        return;

    uint64 oldHomeId = m_homeId;
    m_homeId = connection_id;
    if (m_homeId == 0)
        return;

    UpdateActive();
    // Dump talk changes for new and old home id
    //    talkers->DumpTalkStatusChanges(this,STATUS_TALKING); //ToDo: ServerConnectionHandler specific dump
    // don't need no whisper or self talk here
    {
        const auto set_blocked = !m_isTargetOtherTabs;
        m_vols.do_for_each(
        [&set_blocked](DspVolumeDucker *volume) {
            if (volume)
                volume->set_duck_blocked(set_blocked);
        },
        oldHomeId);
    }

    {
        const auto set_blocked = m_isTargetOtherTabs;
        m_vols.do_for_each(
        [&set_blocked](DspVolumeDucker *volume) {
            if (volume)
                volume->set_duck_blocked(set_blocked);
        },
        m_homeId);
    }

    Log(QString("setHomeId: %1").arg(m_homeId));
}

void Ducker_Channel::setActive(bool val)
{
    if (val == m_isActive)
        return;

    m_isActive = val;
    m_vols.do_for_each([val](DspVolumeDucker *volume) { volume->set_gain_adjustment(val); });
}

// ts event handlers

//! Have a volume object for everyone in the clients channel
void Ducker_Channel::onClientMoveEvent(uint64 connection_id,
                                       anyID client_id,
                                       uint64 old_channel_id,
                                       uint64 new_channel_id,
                                       int visibility,
                                       anyID my_id)
{
    Q_UNUSED(visibility);

    if (client_id == my_id)  // I have switched channels
    {
        //        if (visibility == ENTER_VISIBILITY)  // those are direct from connect status change, filter
        //        those out to only use from setHomeId (and normal ones)
        //            return;

        //        if ((oldChannelID == 0)  && (visibility == RETAIN_VISIBILITY)) // -> setHomeId clientMove
        //            return;

        //        Log("Refreshing volumes",LogLevel_DEBUG);
        m_vols.delete_items(connection_id);

        const auto [error_channel_client_ids, channel_client_ids] =
        funcs::get_channel_client_ids(connection_id, new_channel_id);
        if (ts_errc::ok != error_channel_client_ids)
            Error("(onClientMoveEvent) Error getting Channel Client List", connection_id,
                  error_channel_client_ids);
        else
        {
            // for every client insert volume
            for (const auto client_id : channel_client_ids)
            {
                if (client_id == my_id)
                    continue;

                AddDuckerVolume(connection_id, client_id);
            }
        }
    }
    else  // Someone else has...
    {
        const auto [error_my_channel_id, my_channel_id] = funcs::get_channel_of_client(connection_id, my_id);
        if (ts_errc::ok != error_my_channel_id)
            Error(
            QString("(onClientMoveEvent) Error getting my Client Channel Id (client_id: %1)").arg(client_id),
            connection_id, error_my_channel_id);
        else
        {
            if (my_channel_id == old_channel_id)  // left
                m_vols.delete_item(connection_id, client_id);
            else if (my_channel_id == new_channel_id)  // joined
                AddDuckerVolume(connection_id, client_id);
        }
    }
}

bool Ducker_Channel::onTalkStatusChanged(
uint64 connection_id, int status, bool is_received_whisper, anyID client_id, bool is_me)
{
    if (!running())
        return false;

    if (is_me && !m_isTargetOtherTabs)
        return false;

    // Compute if this change causes a ducking change
    if ((!isActive()) && (status == STATUS_TALKING))
    {
        if ((is_received_whisper) || (!m_isTargetOtherTabs && (connection_id != m_homeId)) ||
            (m_isTargetOtherTabs && (connection_id == m_homeId)))
            setActive(true);
    }
    else if (isActive() && (status == STATUS_NOT_TALKING))
        UpdateActive();

    if (is_me)
        return false;

    if (((status == STATUS_TALKING) || (status == STATUS_NOT_TALKING)))
    {
        const auto is_trigger = (m_isTargetOtherTabs && (connection_id == m_homeId)) ||
                                (!m_isTargetOtherTabs && (connection_id != m_homeId));

        // non ideal i guess
        int is_priority_speaker_duck = 0;
        if ((!m_duck_priority_speakers) && (STATUS_TALKING == status))
        {
            const auto [error_is_prio, is_prio] =
            funcs::get_client_property_as_int(connection_id, client_id, CLIENT_IS_PRIORITY_SPEAKER);
            if (ts_errc::ok != error_is_prio)
                Error("(onTalkStatusChangeEvent)", connection_id, error_is_prio);
            else
                is_priority_speaker_duck = is_prio;
        }

        m_vols.do_for(
        [is_trigger, is_received_whisper, is_priority_speaker_duck, status](DspVolumeDucker *volume) {
            if (!volume)
                return;

            volume->set_duck_blocked(is_trigger || ((is_received_whisper || (is_priority_speaker_duck)) &&
                                                    (status == STATUS_TALKING)));
            volume->set_processing(status == STATUS_TALKING);
        },
        connection_id, client_id);

        return ((status == STATUS_TALKING) && !(is_received_whisper || is_priority_speaker_duck));
    }
    return false;
}

//! Routes the arguments of the event to the corresponding volume object
/*!
 * \brief Ducker_Channel::onEditPlaybackVoiceDataEvent pre-processing voice event
 * \param connection_id the connection id of the server
 * \param client_id the client-side runtime-id of the sender
 * \param samples the sample array to manipulate
 * \param sampleCount amount of samples in the array
 * \param channels amount of channels
 */
void Ducker_Channel::onEditPlaybackVoiceDataEvent(
uint64 connection_id, anyID client_id, short *samples, int frame_count, int channels)
{
    if (!(running()))
        return;

    if (((!m_isTargetOtherTabs) && (connection_id != m_homeId)) ||
        ((m_isTargetOtherTabs) && (connection_id == m_homeId)))
        return;

    auto samples_ = gsl::span<int16_t>{samples, static_cast<size_t>(frame_count * channels)};
    m_vols.do_for(
    [samples_, channels](DspVolumeDucker *volume) {
        if (!volume)
            return;

        volume->process(samples_, channels);
    },
    connection_id, client_id);
}

//! Create and add a Volume object to the ServerChannelVolumes map
/*!
 * \brief Ducker_Channel::AddDuckerVolume Helper function
 * \param connection_id the connection id of the server
 * \param client_id the client id
 * \return a volume object
 */
DspVolumeDucker *Ducker_Channel::AddDuckerVolume(uint64 connection_id, anyID client_id)
{
    auto result = m_vols.add_volume(connection_id, client_id);
    auto *vol = result.first;
    if (vol)
    {
        vol->set_gain_desired(m_value);
        vol->set_gain_adjustment(m_isActive);
    }
    return vol;
}

void Ducker_Channel::UpdateActive()
{
    bool is_active = false;

    if (m_isTargetOtherTabs && (m_talkers.isMeTalking() != 0))
        is_active = true;
    else if (!(m_talkers.get_infos(Talkers::Talker_Type::Whisperers).empty()))
        is_active = true;
    else
    {
        if (m_isTargetOtherTabs)
        {
            if (!m_talkers.get_infos(Talkers::Talker_Type::Talkers, m_homeId).empty())
                is_active = true;
        }
        else
        {
            if (m_talkers.get_infos(Talkers::Talker_Type::Talkers).size() !=
                m_talkers.get_infos(Talkers::Talker_Type::Talkers, m_homeId).size())
                is_active = true;
        }
    }
    setActive(is_active);
}

void Ducker_Channel::onConnectStatusChanged(uint64 connection_id, int new_status, unsigned int error_number)
{
    m_vols.onConnectStatusChanged(connection_id, new_status, error_number);
}

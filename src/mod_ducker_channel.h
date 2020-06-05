#pragma once

#include <QtCore/QObject>
#include "teamspeak/public_definitions.h"
#include "core/module_qt.h"
#include "volume/volumes.h"
#include "volume/dsp_volume_ducker.h"
#include "core/talkers.h"

#include <memory>

class Plugin_Base;

class Ducker_Channel : public Module_Qt, public TalkInterface
{
    Q_OBJECT
    Q_INTERFACES(TalkInterface)
    Q_PROPERTY(bool isActive
               READ isActive
               WRITE setActive
               NOTIFY activeSet)
    Q_PROPERTY(float value
               READ getValue
               WRITE setValue
               NOTIFY valueSet)
    Q_PROPERTY(uint64 homeId
               READ homeId
               WRITE setHomeId)
    Q_PROPERTY(bool targetOtherTabs
               READ isTargetOtherTabs)
    Q_PROPERTY(bool duckPrioritySpeakers
               READ isDuckPrioritySpeakers)

public:
    explicit Ducker_Channel(Plugin_Base& plugin);

    float getValue() const;
    bool isTargetOtherTabs() const;
    bool isDuckPrioritySpeakers() const;

    // events forwarded from plugin.cpp
    void onClientMoveEvent(uint64 connection_id, anyID client_id, uint64 old_channel_id, uint64 new_channel_id, int visibility, anyID my_id);
    void onEditPlaybackVoiceDataEvent(uint64 server_connection_handler_id, anyID client_id, short* samples, int frame_count, int channels);

    void setHomeId(uint64 server_connection_handler_id);
    uint64 homeId() { return m_homeId; }
    bool isActive() { return m_isActive; }
    void setActive(bool); // for testing command, move to private later

    bool onTalkStatusChanged(uint64 server_connection_handler_id, int status, bool is_received_whisper, anyID client_id, bool is_me);

private:
    bool m_isTargetOtherTabs = false;
    bool m_isActive = false;
    float value() { return m_value; }
    float m_value = 0.0f;
    uint64 m_homeId = 0;
    bool m_duck_priority_speakers = false;

	Talkers& m_talkers;
    thorwe::volume::Volumes<DspVolumeDucker> m_vols;

    DspVolumeDucker* AddDuckerVolume(uint64 connection_id, anyID client_id);
    void UpdateActive();

signals:
    void valueSet(float);
    void activeSet(bool);

public slots:
    void setValue(float);
    void setDuckingReverse(bool);
    void setDuckPrioritySpeakers(bool);

    void onConnectStatusChanged(uint64 connection_id, int new_status, unsigned int error_number);

protected:
    void onRunningStateChanged(bool);
};

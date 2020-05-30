#pragma once

#include <QtCore/QObject>
#include <QtCore/QTextStream>
#include "teamspeak/public_definitions.h"
#include "core/module.h"
#include "volume/volumes.h"
#include "core/talkers.h"
#include "core/ts_infodata_qt.h"
#include "core/ts_context_menu_qt.h"
#include "volume/dsp_volume_ducker.h"

#include "core/plugin_base.h"

#include <memory>

class Ducker_Global : public Module, public InfoDataInterface, public ContextMenuInterface
{
    Q_OBJECT
    Q_INTERFACES(InfoDataInterface ContextMenuInterface)

public:
    Ducker_Global(Plugin_Base& plugin);

    float getValue() const;

    void AddMusicBot(uint64 connection_id, anyID client_id);
    void RemoveMusicBot(uint64 connection_id, anyID client_id);
    void ToggleMusicBot(uint64 connection_id, anyID client_id);

    // events forwarded from plugin.cpp
    void onClientMoveEvent(uint64 connection_id, anyID client_id, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID myID);
    bool onEditPlaybackVoiceDataEvent(uint64 connection_id, anyID client_id, short* samples, int sampleCount, int channels);
    bool isClientMusicBot(uint64 connection_id, anyID client_id);
    bool isClientMusicBotRt(uint64 connection_id, anyID client_id);
    void onTalkStatusChanged(uint64 connection_id, int status, bool isReceivedWhisper, anyID client_id, bool isMe);

    bool isActive() { return m_active; }
    void setActive(bool); // for testing command, move to private later

    QMap<QString,QString> m_duck_targets;

    bool onInfoDataChanged(uint64 connection_id, uint64 id, PluginItemType type, uint64 mine, QTextStream &data);

public slots:
    void setValue(float newValue);
    void onContextMenuEvent(uint64 connection_id, PluginMenuType type, int menuItemID, uint64 selectedItemID);

    void onConnectStatusChanged(uint64 connection_id, int new_status, unsigned int error_number);

protected:
    void onRunningStateChanged(bool value);

private:
    bool m_active = false;

    float value() const { return m_value; }
    float m_value = 0.0f;

    Talkers& m_talkers;
    thorwe::volume::Volumes<DspVolumeDucker> m_vols;

    DspVolumeDucker* AddMusicBotVolume(uint64 connection_id, anyID client_id);

    void SaveDuckTargets();
    void UpdateActive();

    int m_ContextMenuToggleMusicBot = -1;
};

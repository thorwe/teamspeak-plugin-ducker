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

class Ducker_Global : public Module, public InfoDataInterface, public ContextMenuInterface
{
    Q_OBJECT
    Q_INTERFACES(InfoDataInterface ContextMenuInterface)
    Q_PROPERTY(bool isActive
               READ isActive
               WRITE setActive
               NOTIFY activeSet)
    Q_PROPERTY(float value
               READ getValue
               WRITE setValue
               NOTIFY valueSet)

public:
    Ducker_Global(Plugin_Base& plugin);

    float getValue() const;

    void AddMusicBot(uint64 serverConnectionHandlerID, anyID clientID);
    void RemoveMusicBot(uint64 serverConnectionHandlerID, anyID clientID);
    void ToggleMusicBot(uint64 serverConnectionHandlerID, anyID clientID);

    // events forwarded from plugin.cpp
    void onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID myID);
    bool onEditPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels);
    bool isClientMusicBot(uint64 serverConnectionHandlerID, anyID clientID);
    bool isClientMusicBotRt(uint64 serverConnectionHandlerID, anyID clientID);
    void onTalkStatusChanged(uint64 serverConnectionHandlerID, int status, bool isReceivedWhisper, anyID clientID, bool isMe);

    bool isActive() { return m_active; }
    void setActive(bool); // for testing command, move to private later

    QMap<QString,QString> m_duck_targets;

    bool onInfoDataChanged(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, uint64 mine, QTextStream &data);


signals:
    void valueSet(float);
    void activeSet(bool);

public slots:
    void setValue(float newValue);
    void onContextMenuEvent(uint64 serverConnectionHandlerID, PluginMenuType type, int menuItemID, uint64 selectedItemID);

protected:
    void onRunningStateChanged(bool value);

private:
    bool m_active = false;

    float value() { return m_value; }
    float m_value = 0.0f;

    Talkers& m_talkers;
    Volumes* vols;

    DspVolumeDucker* AddMusicBotVolume(uint64 serverConnectionHandlerID, anyID clientID);

    void SaveDuckTargets();
    void UpdateActive();

    int m_ContextMenuToggleMusicBot = -1;
};

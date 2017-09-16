#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include "core/ts_context_menu_qt.h"
#include "config_ducking_client.h"
#include "config_ducking_tabs.h"
#include "mod_ducker_global.h"
#include "mod_ducker_channel.h"

class Plugin_Base;

class SettingsDuck : public QObject, public ContextMenuInterface
{
    Q_OBJECT
    Q_INTERFACES(ContextMenuInterface)

public:
	explicit SettingsDuck(Plugin_Base* plugin);

    void Init(Ducker_Global *ducker_G, Ducker_Channel *ducker_C);

signals:
    void globalDuckerEnabledSet(bool);
    void globalDuckerValueChanged(float);
    void channelDuckerEnabledSet(bool);
    void channelDuckerValueChanged(float);
    void channelDuckerReverseSet(bool);
    void channelDuckerDuckPSEnabledSet(bool);

    void settingsSave();

public slots:
    void onContextMenuEvent(uint64 server_connectionHandler_id, PluginMenuType type, int menu_item_id, uint64 selected_item_id);
    void onMenusInitialized();

private slots:
    void saveSettings(int);

private:
    int m_context_menu_ui_client = -1;
    int m_context_menu_ui_tabs = -1;
    QPointer<Config_Ducking_Client> m_config_client;
    QPointer<Config_Ducking_Tabs> m_config_tabs;

    QPointer<Ducker_Global> mP_ducker_G;
    QPointer<Ducker_Channel> mP_ducker_C;
};

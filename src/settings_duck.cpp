#include "settings_duck.h"

#include <QtCore/QSettings>

#include "core/plugin_base.h"
#include "core/ts_helpers_qt.h"
#include "core/ts_logging_qt.h"

SettingsDuck::SettingsDuck(Plugin_Base* plugin)
	: QObject(plugin)
{
    this->setObjectName("SettingsDuck");
}

void SettingsDuck::Init(Ducker_Global* ducker_G, Ducker_Channel* ducker_C)
{
    if(m_context_menu_ui_client == -1)
    {
		auto plugin = qobject_cast<Plugin_Base*>(parent());
		auto& context_menu = plugin->context_menu();
        m_context_menu_ui_client = context_menu.Register(this, PLUGIN_MENU_TYPE_GLOBAL, "Clients", "");
        m_context_menu_ui_tabs = context_menu.Register(this, PLUGIN_MENU_TYPE_GLOBAL, "Server Tabs", "");
        connect(&context_menu, &TSContextMenu::MenusInitialized, this, &SettingsDuck::onMenusInitialized, Qt::AutoConnection);
        connect(&context_menu, &TSContextMenu::FireContextMenuEvent, this, &SettingsDuck::onContextMenuEvent, Qt::AutoConnection);
    }

    connect(this, &SettingsDuck::globalDuckerEnabledSet, ducker_G, &Ducker_Global::setEnabled);
    connect(this, &SettingsDuck::globalDuckerValueChanged, ducker_G, &Ducker_Global::setValue);

    connect(this, &SettingsDuck::channelDuckerEnabledSet, ducker_C, &Ducker_Channel::setEnabled);
    connect(this, &SettingsDuck::channelDuckerValueChanged, ducker_C, &Ducker_Channel::setValue);
    connect(this, &SettingsDuck::channelDuckerReverseSet, ducker_C, &Ducker_Channel::setDuckingReverse);
    connect(this, &SettingsDuck::channelDuckerDuckPSEnabledSet, ducker_C, &Ducker_Channel::setDuckPrioritySpeakers);

    QSettings cfg(TSHelpers::GetPath(teamspeak::plugin::Path::PluginIni), QSettings::IniFormat);

    cfg.beginGroup("ducker_global");
    int size = cfg.beginReadArray("targets");
    for (int i = 0; i < size; ++i)
    {
        cfg.setArrayIndex(i);
        ducker_G->m_duck_targets.insert(cfg.value("uid").toString().toStdString(),
                                        cfg.value("name").toString().toStdString());
    }
    cfg.endArray();
    ducker_G->setValue(cfg.value("value",-23.0f).toFloat());
    ducker_G->setEnabled(cfg.value("enabled",true).toBool());
    cfg.endGroup();

    ducker_C->setEnabled(cfg.value("ducking_enabled",true).toBool());
    ducker_C->setValue(cfg.value("ducking_value",-23.0F).toFloat());
    ducker_C->setDuckingReverse(cfg.value("ducking_reverse",false).toBool());
    ducker_C->setDuckPrioritySpeakers(cfg.value("ducking_PS",false).toBool());

    mP_ducker_G = ducker_G;
    mP_ducker_C = ducker_C;
}

void SettingsDuck::onContextMenuEvent(uint64 server_connection_handler_id, PluginMenuType type, int menu_item_id, uint64 selected_item_id)
{
    Q_UNUSED(server_connection_handler_id);
    Q_UNUSED(selected_item_id);

    if (type == PLUGIN_MENU_TYPE_GLOBAL)
    {
        if (menu_item_id == m_context_menu_ui_client)
        {
            if (m_config_client)
                m_config_client.data()->activateWindow();
            else
            {
                auto p_config = new Config_Ducking_Client(TSHelpers::GetMainWindow());  //has delete on close attribute

                QSettings cfg(TSHelpers::GetPath(teamspeak::plugin::Path::PluginIni), QSettings::IniFormat);
                cfg.beginGroup("ducker_global");
                p_config->UpdateGlobalDuckerEnabled(cfg.value("enabled",true).toBool());
                p_config->UpdateGlobalDuckerValue(cfg.value("value",-23.0F).toFloat());
                cfg.endGroup();

                this->connect(p_config, &Config_Ducking_Client::globalDuckerEnabledSet, this, &SettingsDuck::globalDuckerEnabledSet);
                this->connect(p_config, &Config_Ducking_Client::globalDuckerValueChanged, this, &SettingsDuck::globalDuckerValueChanged);

                connect(p_config, &Config_Ducking_Client::finished, this, &SettingsDuck::saveSettings);
                p_config->show();
                m_config_client = p_config;
            }
        }
        else if (menu_item_id == m_context_menu_ui_tabs)
        {
            if (m_config_client)
                m_config_client.data()->activateWindow();
            else
            {
                auto p_config = new Config_Ducking_Tabs(TSHelpers::GetMainWindow());  //has delete on close attribute
                QSettings cfg(TSHelpers::GetPath(teamspeak::plugin::Path::PluginIni), QSettings::IniFormat);
                p_config->UpdateChannelDuckerEnabled(cfg.value("ducking_enabled", true).toBool());
                p_config->UpdateChannelDuckerValue(cfg.value("ducking_value", -23.0).toFloat());
                p_config->UpdateChannelDuckerReverse((cfg.value("ducking_reverse", false).toBool()) ? 1 : 0);
                p_config->UpdateChannelDuckerDuckPSEnabled(cfg.value("ducking_PS", false).toBool());

                this->connect(p_config, &Config_Ducking_Tabs::channelDuckerEnabledSet, this, &SettingsDuck::channelDuckerEnabledSet);
                this->connect(p_config, &Config_Ducking_Tabs::channelDuckerValueChanged, this, &SettingsDuck::channelDuckerValueChanged);
                this->connect(p_config, &Config_Ducking_Tabs::channelDuckerReverseSet, this, &SettingsDuck::channelDuckerReverseSet);
                this->connect(p_config, &Config_Ducking_Tabs::channelDuckerDuckPSEnabledSet, this, &SettingsDuck::channelDuckerDuckPSEnabledSet);

                connect(p_config, &Config_Ducking_Tabs::finished, this, &SettingsDuck::saveSettings);
                p_config->show();
                m_config_tabs = p_config;
            }
        }
    }
}

void SettingsDuck::onMenusInitialized()
{
    if(m_context_menu_ui_client == -1)
        TSLogging::Error(QString("%1: Client Menu wasn't registered.").arg(this->objectName()));

    if(m_context_menu_ui_tabs == -1)
        TSLogging::Error(QString("%1: Tab Menu wasn't registered.").arg(this->objectName()));
}

void SettingsDuck::saveSettings(int r)
{
    Q_UNUSED(r);
    QSettings cfg(TSHelpers::GetPath(teamspeak::plugin::Path::PluginIni), QSettings::IniFormat);
    if (mP_ducker_G)
    {
        cfg.beginGroup("ducker_global");
        cfg.setValue("enabled", mP_ducker_G.data()->enabled());
        cfg.setValue("value", mP_ducker_G.data()->getValue());
        cfg.endGroup();
    }
    if (mP_ducker_C)
    {
        cfg.setValue("ducking_enabled", mP_ducker_C.data()->enabled());
        cfg.setValue("ducking_value", mP_ducker_C.data()->getValue());
        cfg.setValue("ducking_reverse", mP_ducker_C.data()->isTargetOtherTabs());
        cfg.setValue("ducking_PS", mP_ducker_C.data()->isDuckPrioritySpeakers());
    }

    emit settingsSave();
}

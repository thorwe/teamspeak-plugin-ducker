#pragma once

#include "core/plugin_base.h"

class Ducker_Global;
class Ducker_Channel;
class SettingsDuck;

class Plugin : public Plugin_Base
{
	Q_OBJECT

public:
	Plugin(const char* plugin_id, QObject *parent = nullptr);

	static const char* kPluginName;
	static const char* kPluginVersion;
    static const int kPluginApiVersion = 23;
	static const char* kPluginAuthor;
	static const char* kPluginDescription;

	int initialize() override;

	/* optional */
	static const int kPluginOffersConfigure = PLUGIN_OFFERS_NO_CONFIGURE;

	void on_current_server_connection_changed(uint64 sch_id) override;
    void on_connect_status_changed(uint64 sch_id, int new_status, unsigned int error_number) override;

    void on_client_move(uint64 sch_id, anyID client_id, uint64 old_channel_id, uint64 new_channel_id, int visibility, anyID my_id, const char* move_message) override;
	void on_client_move_timeout(uint64 sch_id, anyID client_id, uint64 old_channel_id, anyID my_id, const char* timeout_message) override;
	void on_client_move_moved(uint64 sch_id, anyID client_id, uint64 old_channel_id, uint64 new_channel_id, int visibility, anyID my_id, anyID mover_id, const char* mover_name, const char* mover_unique_id, const char* move_message) override;

	void on_talk_status_changed(uint64 sch_id, int status, int is_received_whisper, anyID client_id, bool is_me) override;
	void on_playback_pre_process(uint64 sch_id, anyID client_id, short* samples, int frame_count, int channels) override;

private:
	Ducker_Global* ducker_g = nullptr;
	Ducker_Channel* ducker_c = nullptr;
	SettingsDuck* m_settings_duck = nullptr;
};

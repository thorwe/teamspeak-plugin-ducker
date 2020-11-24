#include "plugin_qt.h"

#include "teamspeak/clientlib_publicdefinitions.h"

#include "mod_ducker_channel.h"
#include "mod_ducker_global.h"
#include "settings_duck.h"

const char* Plugin::kPluginName = "Ducker";
const char* Plugin::kPluginVersion = "1.0.1";
const char* Plugin::kPluginAuthor = "Thorsten Weinz";
const char* Plugin::kPluginDescription = "In ducking, the level of one audio signal is reduced by the presence of another signal.\n\nFeatures:\nReduce the volume of a specific client (e.g. a music bot) when someone else starts to talk\nReduce the volume of clients on a server when someone on another connected server starts to talk.\n";

Plugin::Plugin(const char* plugin_id, QObject *parent)
	: Plugin_Base(plugin_id, parent)
	, ducker_g(new Ducker_Global(*this))
	, ducker_c(new Ducker_Channel(*this))
	, m_settings_duck(new SettingsDuck(this))
{}

/* required functions */

auto Plugin::initialize() -> int
{
	context_menu().setMainIcon("duck_16.png");
	translator().update();
	m_settings_duck->Init(ducker_g, ducker_c);
	return 0;
}

/* optional functions */

void Plugin::on_current_server_connection_changed(uint64 sch_id)
{
    ducker_c->setHomeId(sch_id);
}

void Plugin::on_connect_status_changed(uint64 sch_id, int new_status, unsigned int error_number)
{
    ducker_g->onConnectStatusChanged(sch_id, new_status, error_number);
    ducker_c->onConnectStatusChanged(sch_id, new_status, error_number);
}

void Plugin::on_client_move(uint64 sch_id, anyID client_id, uint64 old_channel_id, uint64 new_channel_id, int visibility, anyID my_id, const char * move_message)
{
	Q_UNUSED(move_message);
	ducker_g->onClientMoveEvent(sch_id, client_id, old_channel_id, new_channel_id, visibility, my_id);
	ducker_c->onClientMoveEvent(sch_id, client_id, old_channel_id, new_channel_id, visibility, my_id);
}

void Plugin::on_client_move_timeout(uint64 sch_id, anyID client_id, uint64 old_channel_id, anyID my_id, const char * timeout_message)
{
	Q_UNUSED(timeout_message);
	ducker_g->onClientMoveEvent(sch_id, client_id, old_channel_id, 0, LEAVE_VISIBILITY, my_id);
	ducker_c->onClientMoveEvent(sch_id, client_id, old_channel_id, 0, LEAVE_VISIBILITY, my_id);
}

void Plugin::on_client_move_moved(uint64 sch_id, anyID client_id, uint64 old_channel_id, uint64 new_channel_id, int visibility, anyID my_id, anyID mover_id, const char * mover_name, const char * mover_unique_id, const char * move_message)
{
	Q_UNUSED(mover_id);
	Q_UNUSED(mover_name);
	Q_UNUSED(mover_unique_id);
	Q_UNUSED(move_message);
	ducker_g->onClientMoveEvent(sch_id, client_id, old_channel_id, new_channel_id, visibility, my_id);
	ducker_c->onClientMoveEvent(sch_id, client_id, old_channel_id, new_channel_id, visibility, my_id);
}

void Plugin::on_talk_status_changed(uint64 sch_id, int status, int is_received_whisper, anyID client_id, bool is_me)
{
	ducker_g->onTalkStatusChanged(sch_id, status, is_received_whisper, client_id, is_me);
    if (!ducker_g->running() || !ducker_g->isClientMusicBotRt(sch_id, client_id))
		ducker_c->onTalkStatusChanged(sch_id, status, is_received_whisper, client_id, is_me);
}

void Plugin::on_playback_pre_process(uint64 sch_id, anyID client_id, short* samples, int frame_count, int channels)
{
	if (!ducker_g->onEditPlaybackVoiceDataEvent(sch_id, client_id, samples, frame_count, channels))
		ducker_c->onEditPlaybackVoiceDataEvent(sch_id, client_id, samples, frame_count, channels);
}



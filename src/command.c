/*
 *   Command handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "falcon.h"
#include "fpp.h"
#include "fppd.h"
#include "log.h"
#include "command.h"
#include "Scheduler.h"
#include "e131bridge.h"
#include "Player.h"
#include "mediaoutput.h"
#include "settings.h"
#include "Sequence.h"
#include "effects.h"
#include "Plugins.h"
#include "FPD.h"
#include "events.h"
#include "channeloutput.h"
#include "E131.h"
#include "gpio.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

extern PluginCallbackManager pluginCallbackManager;

 char response[1056];
 int socket_fd;
 struct sockaddr_un server_address;
 struct sockaddr_un client_address;
 int bytes_received, bytes_sent;
 int integer_buffer;
 int fppdStartTime = 0;
 socklen_t address_length = sizeof(struct sockaddr_un);

 int Command_Initialize()
 {
   mode_t old_umask;

   LogInfo(VB_COMMAND, "Initializing Command Module\n");
   signal(SIGINT, exit_handler);
   signal(SIGTERM, exit_handler);

   if((socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
   {
    perror("server: socket");
    return 0;
   }

   fcntl(socket_fd, F_SETFL, O_NONBLOCK);
   memset(&server_address, 0, sizeof(server_address));
   server_address.sun_family = AF_UNIX;
   strcpy(server_address.sun_path, FPP_SERVER_SOCKET);
   unlink(FPP_SERVER_SOCKET);

   old_umask = umask(0011);

   if(bind(socket_fd, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0)
   {
    close(socket_fd);
    perror("command socket bind");
    return 0;
   }

   umask(old_umask);

   fppdStartTime = time(NULL);

   return socket_fd;
 }

 void CloseCommand()
 {
   close(socket_fd);
   unlink(FPP_SERVER_SOCKET);
 }

 void CommandProc()
{
  char command[256];

  bzero(command, sizeof(command));
  bytes_received = recvfrom(socket_fd, command,256, 0,
                           (struct sockaddr *) &(client_address),
                            &address_length);

  if(bytes_received > 0)
  {
    ProcessCommand(command);
  }

 }

  void ProcessCommand(char *command)
  {
    char *s;
    char *s2;
    char *s3;
    char *s4;
    char *response2 = NULL;
    int i;
		char NextPlaylist[128] = "No playlist scheduled.";
		char NextScheduleStartText[64] = "";
		char CommandStr[64];
		LogExcess(VB_COMMAND, "CMD: %s\n", command);
		s = strtok(command,",");
		strcpy(CommandStr, s);
		if (!strcmp(CommandStr, "s"))
		{
				Json::Value info = player->GetCurrentPlaylistInfo();
				char currentEntryType = 'u'; // Unknown
				const char *currentSequenceName = NULL;
				const char *currentMediaName = NULL;

				if (info["currentEntry"].isMember("sequenceName"))
					currentSequenceName = info["currentEntry"]["sequenceName"].asString().c_str();

				if (info["currentEntry"].isMember("mediaFilename"))
					currentMediaName = info["currentEntry"]["mediaFilename"].asString().c_str();

				if (info["currentEntry"].isMember("type"))
				{
					std::string type = info["currentEntry"]["type"].asString();
					if (type == "both")
						currentEntryType = 'b';
					else if (type == "event")
						currentEntryType = 'e';
					else if (type == "media")
						currentEntryType = 'm';
					else if (type == "pause")
						currentEntryType = 'p';
					else if (type == "plugin")
						currentEntryType = 'P';
					else if (type == "sequence")
						currentEntryType = 's';
				}

				player->GetNextScheduleStartText(NextScheduleStartText);
				player->GetNextPlaylistText(NextPlaylist);
				if(FPPstatus==FPP_STATUS_IDLE)
				{
					if (getFPPmode() == REMOTE_MODE)
					{
						sprintf(response,"%d,%d,%d,%s,%s,%d,%d\n",
							getFPPmode(), 0, getVolume(),
							currentSequenceName,
							currentMediaName,
							player->GetPlaybackSecondsElapsed(),
							player->GetPlaybackSecondsRemaining());
					}
					else
					{
						sprintf(response,"%d,%d,%d,%s,%s\n",
							getFPPmode(),0,getVolume(),
							NextPlaylist,NextScheduleStartText);
					}
				}
				else
				{
					sprintf(response,"%d,%d,%d,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s,%d\n",
							getFPPmode(),FPPstatus,getVolume(),
							info["name"].asString().c_str(),
							currentEntryType,
							currentSequenceName,
							currentMediaName,
							info["currentPosition"].asInt() + 1,
							info["size"].asInt(),
							info["currentEntry"]["secondsElapsed"].asInt(),
							info["currentEntry"]["secondsRemaining"].asInt(),
							NextPlaylist,NextScheduleStartText,
							info["repeat"].asInt());
				}
		}
		else if ((!strcmp(CommandStr, "p")) ||
				 (!strcmp(CommandStr, "P")))
		{
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
				{
					player->PlaylistStopNow();
					sleep(1);
				}
	
				s = strtok(NULL,",");
				if (s)
				{
					std::string playlistName(s);
					int repeat = !strcmp(CommandStr, "p") ? 1 : 0;
					int position = 0;

					s = strtok(NULL,",");
					if (s)
						position = atoi(s);

					if (player->PlaylistStart(playlistName, position, repeat))
					{
						sprintf(response,"%d,%d,Playlist Started,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
					}
					else
					{
						sprintf(response,"%d,%d,Playlist Start Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
					}
				}
				else
				{
					sprintf(response,"%d,%d,Unknown Playlist,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "S"))
		{
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING)
				{
					player->PlaylistStopGracefully();
					player->ReLoadCurrentScheduleInfo();
					sprintf(response,"%d,%d,Playlist Stopping Gracefully,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				}
				else
				{
					sprintf(response,"%d,Not playing,,,,,,,,,,,\n",COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "d"))
		{
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
				{
					player->PlaylistStopNow();
					player->ReLoadCurrentScheduleInfo();
					sprintf(response,"%d,%d,Playlist Stopping Now,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				}
				else
				{
					sprintf(response,"%d,%d,Not playing,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "R"))
		{
				if(FPPstatus==FPP_STATUS_IDLE)
				{
					player->ReLoadCurrentScheduleInfo();
				}
				player->ReLoadNextScheduleInfo();
				
				
				sprintf(response,"%d,%d,Reloading Schedule,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
		}
		else if (!strcmp(CommandStr, "v"))
		{
				s = strtok(NULL,",");
				if (s)
				{
					setVolume(atoi(s));
					sprintf(response,"%d,%d,Setting Volume,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				}
				else
				{
					sprintf(response,"%d,%d,Invalid Volume,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "w"))
		{
				LogInfo(VB_SETTING, "Sending Falcon hardware config\n");
				if (!DetectFalconHardware(1))
					SendFPDConfig();
		}
		else if (!strcmp(CommandStr, "r"))
		{
				WriteBytesReceivedFile();
				sprintf(response,"true\n");
		}
		else if (!strcmp(CommandStr, "q"))
		{
			// Quit/Shutdown fppd
			ShutdownFPPD();
		}
		else if (!strcmp(CommandStr, "e"))
		{
			// Start an Effect
			s = strtok(NULL,",");
			s2 = strtok(NULL,",");
			s3 = strtok(NULL,",");
			if (s && s2)
			{
				i = StartEffect(s, atoi(s2), atoi(s3));
				if (i >= 0)
					sprintf(response,"%d,%d,Starting Effect,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
				else
					sprintf(response,"%d,%d,Invalid Effect,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
			}
			else
				sprintf(response,"%d,%d,Invalid Effect,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
		}
		else if (!strcmp(CommandStr, "t"))
		{
			// Trigger an event
			s = strtok(NULL,",");
			pluginCallbackManager.eventCallback(s, "command");
			i = TriggerEventByID(s);
			if (i >= 0)
				sprintf(response,"%d,%d,Event Triggered,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
			else
				sprintf(response,"%d,%d,Event Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
		}
		else if (!strcmp(CommandStr, "GetTestMode"))
		{
			strcpy(response, channelTester->GetConfig().c_str());
			strcat(response, "\n");
		}
		else if (!strcmp(CommandStr, "SetTestMode"))
		{
			if (channelTester->SetupTest(std::string(s + strlen(s) + 1)))
			{
				sprintf(response, "0,%d,Test Mode Activated,,,,,,,,,\n",
					COMMAND_SUCCESS);
			}
			else
			{
				sprintf(response, "0,%d,Test Mode Deactivated,,,,,,,,,\n",
					COMMAND_SUCCESS);
			}
		}
		else if (!strcmp(CommandStr, "LogLevel"))
		{
			s = strtok(NULL,",");

			if (SetLogLevel(s)) {
				sprintf(response,"%d,%d,Log Level Updated,%d,%d,,,,,,,,,\n",
					getFPPmode(),COMMAND_SUCCESS,logLevel,logMask);
			} else {
				sprintf(response,"%d,%d,Error Updating Log Level,%d,%d,,,,,,,,,\n",
					getFPPmode(),COMMAND_FAILED,logLevel,logMask);
			}
		}
		else if (!strcmp(CommandStr, "LogMask"))
		{
			s = strtok(NULL,",");

			if ((s && SetLogMask(s)) || SetLogMask("")) {
				sprintf(response,"%d,%d,Log Mask Updated,%d,%d,,,,,,,,,\n",
					getFPPmode(),COMMAND_SUCCESS,logLevel,logMask);
			} else {
				sprintf(response,"%d,%d,Error Updating Log Mask,%d,%d,,,,,,,,,\n",
					getFPPmode(),COMMAND_FAILED,logLevel,logMask);
			}
		}
		else if (!strcmp(CommandStr, "SetSetting"))
		{
			char name[128];

			s = strtok(NULL,",");
			if (s)
			{
				strcpy(name, s);
				s = strtok(NULL,",");
				if (s)
					parseSetting(name, s);
			}
		}
		else if (!strcmp(CommandStr, "StopAllEffects"))
		{
			StopAllEffects();
			sprintf(response,"%d,%d,All Effects Stopped,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
		}
		else if (!strcmp(CommandStr, "StopEffectByName"))
		{
			s = strtok(NULL,",");
			if (strlen(s))
			{
				if (StopEffect(s))
						sprintf(response,"%d,%d,Stopping Effect,%s,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,s);
				else
						sprintf(response,"%d,%d,Stop Effect Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
			}
		}
		else if (!strcmp(CommandStr, "StopEffect"))
		{
			s = strtok(NULL,",");
			i = atoi(s);
			if (StopEffect(i))
					sprintf(response,"%d,%d,Stopping Effect,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
			else
					sprintf(response,"%d,%d,Stop Effect Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
		}
		else if (!strcmp(CommandStr, "GetRunningEffects"))
		{
			sprintf(response,"%d,%d,Running Effects",getFPPmode(),COMMAND_SUCCESS);
			GetRunningEffects(response, &response2);
		}
		else if (!strcmp(CommandStr, "ReloadChannelRemapData"))
		{
			if ((FPPstatus==FPP_STATUS_IDLE) &&
				(LoadChannelRemapData())) {
				sprintf(response,"%d,%d,Channel Remap Data Reloaded,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
			} else {
				sprintf(response,"%d,%d,Failed to reload Channel Remap Data,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
			}
		}
		else if (!strcmp(CommandStr, "GetFPPDUptime"))
		{
			sprintf(response,"%d,%d,FPPD Uptime,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS, time(NULL) - fppdStartTime);
		}
		else if (!strcmp(CommandStr, "StartSequence"))
		{
			s = strtok(NULL,",");
			s2 = strtok(NULL,",");
			if (s && s2)
			{
				i = atoi(s2);
				player->StartSequence(s, 0, i);
				sprintf(response,"%d,%d,Sequence Started,,,,,,,,,,,,\n",
					getFPPmode(), COMMAND_SUCCESS);
			}
			else
			{
				LogErr(VB_COMMAND, "Tried to start a sequence when a playlist or "
						"sequence is already running\n");
				sprintf(response,"%d,%d,Sequence Failed,,,,,,,,,,,,\n",
					getFPPmode(), COMMAND_FAILED);
			}
		}
		else if (!strcmp(CommandStr, "StopSequence"))
		{
			s = strtok(NULL,",");
			if (s)
			{
				player->StopSequence(s);
				sprintf(response,"%d,%d,Sequence Stopped,,,,,,,,,,,,\n",
					getFPPmode(), COMMAND_SUCCESS);
			}
			else
			{
				LogDebug(VB_COMMAND, "Invalid command: %s\n", command);
				sprintf(response,"%d,%d,Sequence Name Missing,,,,,,,,,,,,\n",
					getFPPmode(), COMMAND_FAILED);
			}
		}
		else if (!strcmp(CommandStr, "ToggleSequencePause"))
		{
			if ((player->SequencesRunning()) &&
				((FPPstatus == FPP_STATUS_IDLE) ||
				 (FPPstatus != FPP_STATUS_IDLE)))
			{
				player->ToggleSequencePause();
			}
		}
		else if (!strcmp(CommandStr, "SingleStepSequence"))
		{
			if ((player->SequencesRunning()) &&
				(player->SequencesArePaused()) &&
				((FPPstatus == FPP_STATUS_IDLE) ||
				 ((FPPstatus != FPP_STATUS_IDLE))))
			{
				player->SingleStepSequences();
			}
		}
		else if (!strcmp(CommandStr, "SingleStepSequenceBack"))
		{
			if ((player->SequencesRunning()) &&
				(player->SequencesArePaused()) &&
				((FPPstatus == FPP_STATUS_IDLE) ||
				 ((FPPstatus != FPP_STATUS_IDLE))))
			{
				player->SingleStepSequencesBack();
			}
		}
		else if (!strcmp(CommandStr, "NextPlaylistItem"))
		{
			switch (FPPstatus)
			{
				case FPP_STATUS_IDLE:
					sprintf(response,"%d,%d,No playlist running\n",getFPPmode(),COMMAND_FAILED);
					break;
				case FPP_STATUS_PLAYLIST_PLAYING:
					sprintf(response,"%d,%d,Skipping to next playlist item\n",getFPPmode(),COMMAND_SUCCESS);
					player->NextPlaylistItem();
					break;
				case FPP_STATUS_STOPPING_GRACEFULLY:
					sprintf(response,"%d,%d,Playlist is stopping gracefully\n",getFPPmode(),COMMAND_FAILED);
					break;
			}
		}
		else if (!strcmp(CommandStr, "PrevPlaylistItem"))
		{
			switch (FPPstatus)
			{
				case FPP_STATUS_IDLE:
					sprintf(response,"%d,%d,No playlist running\n",getFPPmode(),COMMAND_FAILED);
					break;
				case FPP_STATUS_PLAYLIST_PLAYING:
					sprintf(response,"%d,%d,Skipping to previous playlist item\n",getFPPmode(),COMMAND_SUCCESS);
					player->PrevPlaylistItem();
					break;
				case FPP_STATUS_STOPPING_GRACEFULLY:
					sprintf(response,"%d,%d,Playlist is stopping gracefully\n",getFPPmode(),COMMAND_FAILED);
					break;
			}
		}
		else if (!strcmp(CommandStr, "SetupExtGPIO"))
		{
			// Configure the given GPIO to the given mode
			s = strtok(NULL,",");
			s2 = strtok(NULL,",");
			if (s && s2)
			{
				
				if (!SetupExtGPIO(atoi(s), s2))
				{
					sprintf(response, "%d,%d,Configuring GPIO,%d,%s,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,atoi(s),s2);
				}
				else
				{
					sprintf(response, "%d,%d,Configuring GPIO,%d,%s,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED,atoi(s),s2);
				}
			}
		}
		else if (!strcmp(CommandStr, "ExtGPIO"))
		{
			s = strtok(NULL,",");
			s2 = strtok(NULL,",");
			s3 = strtok(NULL,",");
			if (s && s2 && s3)
			{
				i = ExtGPIO(atoi(s), s2, atoi(s3));
				if (i >= 0) 
				{
					sprintf(response, "%d,%d,Setting GPIO,%d,%s,%d,%d,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,atoi(s),s2,atoi(s3),i);
				}
				else
				{
					sprintf(response, "%d,%d,Setting GPIO,%d,%s,%d,,,,,,,,\n",getFPPmode(),COMMAND_FAILED,atoi(s),s2,atoi(s3));
				}
			}
		}
		else
		{
			sprintf(response,"Invalid command: '%s'\n", CommandStr);
		}

		if (response2)
		{
			bytes_sent = sendto(socket_fd, response2, strlen(response2), 0,
                          (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
			LogDebug(VB_COMMAND, "%s %s", CommandStr, response2);
			free(response2);
			response2 = NULL;
		}
		else
		{
			bytes_sent = sendto(socket_fd, response, strlen(response), 0,
                          (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
			LogDebug(VB_COMMAND, "%s %s", CommandStr, response);
		}
  }

void exit_handler(int signum)
{
	LogInfo(VB_GENERAL, "Caught signal %d\n",signum);
	CloseCommand();

	if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING)
	{
		player->StopMedia();
	}

	exit(signum);
}


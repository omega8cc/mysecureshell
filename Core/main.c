/*
 MySecureShell permit to add restriction to modified sftp-server
 when using MySecureShell as shell.
 Copyright (C) 2007-2018 MySecureShell Team

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation (version 2)

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "../config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "conf.h"
#include "FileSpec.h"
#include "hash.h"
#include "ip.h"
#include "prog.h"
#include "../SftpServer/Sftp.h"
#include "../SftpServer/Encoding.h"
#include "../SftpServer/Log.h"
#include "security.h"

static void reset_uid()
{
	//if we are in utset byte mode then we restore user's rights to avoid security problems
	if (getuid() == geteuid())
		return;
	if (seteuid(getuid()) == -1 || setegid(getgid()) == -1)
	{
		perror("revoke root rights");
		exit(1);
	}
}

static void showVersion(int showAll)
{
	(void) printf(
			"MySecureShell is version "PACKAGE_VERSION" build on " __DATE__ "%s",
#ifdef DODEBUG
			" with DEBUG"
#else
			""
#endif
	);
	if (showAll == 1)
	{
		(void) printf("\n\nOptions:\n  ACL support: "
#if(MSS_ACL)
					"yes"
#else
				"no"
#endif
				"\n  UTF-8 support: "
#if(HAVE_ICONV||HAVE_LIBICONV)
				"yes"
#else
				"no"
#endif
				"\n\nSftp Extensions:\n"
#ifdef MSSEXT_DISKUSAGE
				"  Disk Usage\n"
#endif
#ifdef MSSEXT_DISKUSAGE_SSH
				"  Disk Usage (OpenSSH)\n"
#endif
#ifdef MSSEXT_FILE_HASHING
				"  File Hashing\n"
#endif
		);
	}
}

static void parse_args(int ac, char **av)
{
	int verbose = 1;
	int i;

	if (ac == 1)
		return;
	for (i = 1; i < ac; i++)
		if (strcmp(av[i], "-c") == 0)
			i++;
		else if (strcmp(av[i], "--configtest") == 0)
		{
			load_config(verbose);
			if (hash_get("ApplyFileSpec") != NULL)
				FileSpecActiveProfils(hash_get("ApplyFileSpec"), verbose);
			(void) printf("Config is valid.\n");
			exit(0);
		}
		else if (strcmp(av[i], "--help") == 0)
		{
			help: (void) printf("Build:\n\t");
			showVersion(0);
			(void) printf("\nUsage:\n\t%s [verbose] [options]\n\nOptions:\n",
					av[0]);
			(void) printf(
					"\t--configtest : test the config file and show errors\n");
			(void) printf("\t--help       : show this screen\n");
			(void) printf("\t--version    : show version of MySecureShell\n");
			(void) printf("\nVerbose:\n");
			(void) printf("\t-v           : add a level at verbose mode\n");
			exit(0);
		}
		else if (strcmp(av[i], "--version") == 0)
		{
			showVersion(1);
			exit(0);
		}
		else if (strcmp(av[i], "-v") == 0)
			verbose++;
		else
		{
			(void) printf("--- UNKNOW OPTION: %s ---\n\n", av[i]);
			goto help;
		}
}

int main(int ac, char **av, char **env)
{
	char *hostname;
	int is_command = 0;
	int is_sftp = 0;

	create_hash();
	if (ac == 3 && av[1] != NULL && av[2] != NULL && strcmp("-c", av[1]) == 0
			&& (strstr(av[2], "sftp-server") != NULL || strstr(av[2], "MySecureShell") != NULL))
		is_sftp = 1;
	else if (ac == 5
			&& av[1] != NULL && av[2] != NULL && strcmp("--config", av[1]) == 0
			&& av[3] != NULL && av[4] != NULL && strcmp("-c", av[3]) == 0
			&& (strstr(av[4], "sftp-server") != NULL || strstr(av[4], "MySecureShell") != NULL))
	{
		reset_uid();
		set_custom_config_file(av[2]);
		is_sftp = 1;
	}
	else if (ac >= 3 && av[1] != NULL && av[2] != NULL && strcmp("-c", av[1]) == 0)
		is_command = 1;
	else
		parse_args(ac, av);
	hostname = get_ip(0);
	(void) setenv("SSH_IP", hostname, 1);
	free(hostname);
	FileSpecInit();
	load_config(0);
	if (is_sftp == 1)
	{
		tGlobal	*params;
		char	*ptr;
		int		max, fd, sftp_version;

		hostname = get_ip(hash_get_int("ResolveIP"));
		if (hostname == NULL)
		{
			perror("unable to resolve ip");
			exit(16);
		}
		params = calloc(1, sizeof(*params));
		if (params == NULL)
		{
			perror("unable to alloc memory");
			exit(15);
		}
		ptr = hash_get("Home");
		params->home = strdup(ptr == NULL ? "{error home}" : ptr);
		ptr = hash_get("User");
		params->user = strdup(ptr == NULL ? "{error user}" : ptr);
		params->ip = strdup(hostname == NULL ? "{error ip}" : hostname);
		params->portSource = get_port_client();

		params->who = SftpWhoGetStruct(1);
		if (params->who != NULL)
		{
			params->who->time_begin = (u_int32_t) time(0);
			params->who->pid = (u_int32_t) getpid();
			(void) strncat(params->who->home, params->home, sizeof(params->who->home) - 1);
			(void) strncat(params->who->user, params->user, sizeof(params->who->user) - 1);
			(void) strncat(params->who->ip, params->ip, sizeof(params->who->ip) - 1);
		}
		//check if the server is up and user is not admin
		if ((fd = open(SHUTDOWN_FILE, O_RDONLY)) >= 0)
		{
			xclose(fd);
			if (hash_get_int("IsAdmin") == 0 && hash_get_int("IsSimpleAdmin") == 0)
			{
				SftpWhoReleaseStruct(params->who);
				delete_hash();
				FileSpecDestroy();
				exit(0);
			}
		}
		max = hash_get_int("LogLevel");
		if (max >= MYLOG_OFF && max < MYLOG_MAX && hash_exists("LogLevel"))
			mylog_level(max);
		max = hash_get_int("LogSyslog");
		if (hash_get("LogFile") != NULL)
			mylog_open(strdup(hash_get("LogFile")), max);
		else
			mylog_open(strdup(MSS_LOG), max);
		if (params->who == NULL)
		{
			mylog_printf(MYLOG_ERROR, "[%s]Server '%s' reached maximum connexion (%i clients)",
					hash_get("User"), hash_get("SERVER_IP"), SFTPWHO_MAXCLIENT);
			SftpWhoReleaseStruct(NULL);
			delete_hash();
			FileSpecDestroy();
			mylog_close_and_free();
			exit(14);
		}
		max = hash_get_int("LimitConnectionByUser");
		if (max > 0 && count_program_for_uid(hash_get("User")) > max)
		{
			mylog_printf(MYLOG_ERROR, "[%s]Too many connection for this account",
					hash_get("User"));
			SftpWhoReleaseStruct(params->who);
			delete_hash();
			FileSpecDestroy();
			exit(10);
		}
		max = hash_get_int("LimitConnectionByIP");
		if (max > 0 && count_program_for_ip(hostname) > max)
		{
			mylog_printf(MYLOG_ERROR, "[%s]Too many connection for this IP : %s",
					hash_get("User"), hostname);
			SftpWhoReleaseStruct(params->who);
			delete_hash();
			FileSpecDestroy();
			exit(11);
		}
		max = hash_get_int("LimitConnection");
		if (max > 0 && count_program_for_uid(NULL) > max)
		{
			mylog_printf(MYLOG_ERROR, "[%s]Too many connection for the server : %s",
					hash_get("User"), hash_get("SERVER_IP"));
			SftpWhoReleaseStruct(params->who);
			delete_hash();
			FileSpecDestroy();
			exit(12);
		}
		if (hash_get_int("DisableAccount"))
		{
			mylog_printf(MYLOG_ERROR, "[%s]Account is closed", hash_get("User"));
			SftpWhoReleaseStruct(params->who);
			delete_hash();
			FileSpecDestroy();
			exit(13);
		}

		params->flagsGlobals
				|= (hash_get_int("StayAtHome") ? SFTPWHO_STAY_AT_HOME : 0)
						+ (hash_get_int("VirtualChroot") ? SFTPWHO_VIRTUAL_CHROOT : 0)
						+ (hash_get_int("ResolveIP") ? SFTPWHO_RESOLVE_IP : 0)
						+ (hash_get_int("IgnoreHidden") ? SFTPWHO_IGNORE_HIDDEN : 0)
						+ (hash_get_int("DirFakeUser") ? SFTPWHO_FAKE_USER : 0)
						+ (hash_get_int("DirFakeGroup") ? SFTPWHO_FAKE_GROUP : 0)
						+ (hash_get_int("DirFakeMode") ? SFTPWHO_FAKE_MODE : 0)
						+ (hash_get_int("HideNoAccess") ? SFTPWHO_HIDE_NO_ACESS : 0)
						+ (hash_get_int("ByPassGlobalDownload") ? SFTPWHO_BYPASS_GLB_DWN : 0)
						+ (hash_get_int("ByPassGlobalUpload") ? SFTPWHO_BYPASS_GLB_UPL : 0)
						+ (hash_get_int("ShowLinksAsLinks") ? SFTPWHO_LINKS_AS_LINKS : 0)
						+ (hash_get_int("IsAdmin") ? SFTPWHO_IS_ADMIN : 0)
						+ (hash_get_int("IsSimpleAdmin") ? SFTPWHO_IS_SIMPLE_ADMIN : 0)
						+ (hash_get_int("CanChangeRights") ? SFTPWHO_CAN_CHG_RIGHTS : 0)
						+ (hash_get_int("CanChangeTime") ? SFTPWHO_CAN_CHG_TIME : 0)
						+ (hash_get_int("CreateHome") ? SFTPWHO_CREATE_HOME : 0);
		params->flagsDisable
				= (hash_get_int("DisableRemoveDir") ? SFTP_DISABLE_REMOVE_DIR : 0)
						+ (hash_get_int("DisableRemoveFile") ? SFTP_DISABLE_REMOVE_FILE : 0)
						+ (hash_get_int("DisableReadDir") ? SFTP_DISABLE_READ_DIR : 0)
						+ (hash_get_int("DisableReadFile") ? SFTP_DISABLE_READ_FILE : 0)
						+ (hash_get_int("DisableWriteFile") ? SFTP_DISABLE_WRITE_FILE : 0)
						+ (hash_get_int("DisableSetAttribute") ? SFTP_DISABLE_SET_ATTRIBUTE : 0)
						+ (hash_get_int("DisableMakeDir") ? SFTP_DISABLE_MAKE_DIR : 0)
						+ (hash_get_int("DisableRename") ? SFTP_DISABLE_RENAME : 0)
						+ (hash_get_int("DisableSymLink") ? SFTP_DISABLE_SYMLINK : 0)
						+ (hash_get_int("DisableOverwrite") ? SFTP_DISABLE_OVERWRITE : 0)
						+ (hash_get_int("DisableStatsFs") ? SFTP_DISABLE_STATSFS : 0);
		params->who->status |= params->flagsGlobals;
		_sftpglobal->download_max = (u_int32_t) hash_get_int("GlobalDownload");
		_sftpglobal->upload_max = (u_int32_t) hash_get_int("GlobalUpload");
		if (hash_get_int("Download") > 0)
		{
			params->download_max = (u_int32_t) hash_get_int("Download");
			params->who->download_max = params->download_max;
		}
		if (hash_get_int("Upload") > 0)
		{
			params->upload_max = (u_int32_t) hash_get_int("Upload");
			params->who->upload_max = params->upload_max;
		}
		if (hash_get_int("IdleTimeOut") > 0)
			params->who->time_maxidle = (u_int32_t) hash_get_int("IdleTimeOut");
		if (hash_get_int("DirFakeMode") > 0)
			params->dir_mode = (u_int32_t) hash_get_int("DirFakeMode");
		sftp_version = hash_get_int("SftpProtocol");
		if (hash_get_int("ConnectionMaxLife") > 0)
			params->who->time_maxlife = (u_int32_t) hash_get_int("ConnectionMaxLife");
		if (hash_get("ExpireDate") != NULL)
		{
			struct tm tm;
			time_t currentTime, maxTime;

			if (strptime((const char *) hash_get("ExpireDate"),
					"%Y-%m-%d %H:%M:%S", &tm) != NULL)
			{
				maxTime = mktime(&tm);
				currentTime = time(NULL);
				if (currentTime > maxTime) //time elapsed
				{
					mylog_printf(MYLOG_ERROR, "[%s]Account has expired : %s",
							hash_get("User"), hash_get("ExpireDate"));
					SftpWhoReleaseStruct(params->who);
					delete_hash();
					mylog_close_and_free();
					exit(15);
				}
				else
				{ //check if expireDate < time_maxlife
					currentTime = maxTime - currentTime;
					if ((u_int32_t) currentTime < params->who->time_maxlife)
						params->who->time_maxlife = (u_int32_t) currentTime;
				}
			} DEBUG((MYLOG_DEBUG, "[%s][%s]ExpireDate time to rest: %i",
							params->who->user, params->who->ip, params->who->time_maxlife));
		}

		if (hash_exists("MaxOpenFilesForUser") == MSS_TRUE)
			params->max_openfiles = hash_get_int("MaxOpenFilesForUser");
		if (hash_exists("MaxReadFilesForUser") == MSS_TRUE)
			params->max_readfiles = hash_get_int("MaxReadFilesForUser");
		if (hash_exists("MaxWriteFilesForUser") == MSS_TRUE)
			params->max_writefiles = hash_get_int("MaxWriteFilesForUser");

		if (hash_get_int("MinimumRightsDirectory") > 0)
			params->minimum_rights_directory = hash_get_int(
					"MinimumRightsDirectory");
		if (hash_get_int("MinimumRightsFile") > 0)
			params->minimum_rights_file = hash_get_int("MinimumRightsFile");
		if (hash_get_int("MaximumRightsDirectory") > 0)
			params->maximum_rights_directory = hash_get_int(
					"MaximumRightsDirectory");
		else
			params->maximum_rights_directory = 07777;
		if (hash_get_int("MaximumRightsFile") > 0)
			params->maximum_rights_file = hash_get_int("MaximumRightsFile");
		else
			params->maximum_rights_file = 07777;
		if (hash_get_int("DefaultRightsDirectory") > 0)
			params->default_rights_directory = hash_get_int("DefaultRightsDirectory");
		else
			params->default_rights_directory = 0755;
		if (hash_get_int("DefaultRightsFile") > 0)
			params->default_rights_file = hash_get_int("DefaultRightsFile");
		else
			params->default_rights_file = 0644;
		if (hash_get_int("ForceRightsDirectory") > 0)
		{
			params->minimum_rights_directory = hash_get_int("ForceRightsDirectory");
			params->maximum_rights_directory = params->minimum_rights_directory;
		}
		if (hash_get_int("ForceRightsFile") > 0)
		{
			params->minimum_rights_file = hash_get_int("ForceRightsFile");
			params->maximum_rights_file = params->minimum_rights_file;
		}

		if (hash_get("ForceUser") != NULL)
			params->force_user = strdup(hash_get("ForceUser"));
		if (hash_get("ForceGroup") != NULL)
			params->force_group = strdup(hash_get("ForceGroup"));

		if (hash_get("CallbackDownload") != NULL)
			params->callback_download = strdup(hash_get("CallbackDownload"));
		if (hash_get("CallbackUpload") != NULL)
			params->callback_upload = strdup(hash_get("CallbackUpload"));

		if (hash_get("Charset") != NULL)
			setCharset(hash_get("Charset"));
		if (hash_get("ApplyFileSpec") != NULL)
			FileSpecActiveProfils(hash_get("ApplyFileSpec"), 0);
		delete_hash();
		if (hostname != NULL)
			free(hostname);
		params->current_user = getuid();
		params->current_group = getgid();
		return (SftpMain(params, sftp_version));
	}
	else
	{
		char *ptr;

		reset_uid();
		ptr = hash_get("Shell");
		if (ptr != NULL)
		{
			if (strcmp(ptr, av[0]) != 0)
			{
				av[0] = ptr;
				if (is_command == 1)
				{
					size_t	len = 0;
					char	**new_env;
					char	*cmd, *envVar;
					int		i;

					for (i = 2; i < ac; i++)
						len += strlen(av[i]);
					cmd = malloc(len + ac + 1);
					envVar = malloc(len + ac + 1 + 21);
					cmd[0] = '\0';
					for (i = 2; i < ac; i++)
					{
						if (i > 2)
							strcat(cmd, " ");
						strcat(cmd, av[i]);
					}
					av[2] = cmd;
					av[3] = NULL;
					strcpy(envVar, "SSH_ORIGINAL_COMMAND=");
					strcat(envVar, cmd);
					len = 0;
					for (i = 0; env[i] != NULL; i++)
						len++;
					new_env = calloc(len + 2, sizeof(*new_env));
					for (i = 0; i < len; i++)
						new_env[i] = env[i];
					new_env[len] = envVar;
					(void) execve(av[0], av, new_env);
				}
				else
					(void) execve(av[0], av, env);
				perror("execute shell");
			}
			else
				(void) fprintf(stderr, "You cannot specify MySecureShell has shell (in the MySecureShell configuration) !");
		}
		else
			(void) fprintf(stderr, "Shell access is disabled !");
		exit(1);
	}
}

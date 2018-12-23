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
#ifdef HAVE_CYGWIN_SOCKET_H
#include <cygwin/socket.h>
#endif
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include "hash.h"
#include "ip.h"

/*@null@*/ char *get_ip(int resolv)
{
	struct hostent *h;
	unsigned char addr6[sizeof(struct in6_addr)];
	in_addr_t addr;
	char *env, *ip = NULL;

	if ((env = getenv("SSH_CONNECTION")) != NULL)
	{
		char *ptr;

		env = strdup(env);
		if ((ptr = strchr(env, ' ')))
			*ptr = '\0';
		if (resolv == 0)
			ip = strdup(env);
		else if ((int) (addr = inet_addr(env)) != -1)
		{
			if ((h = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET)) != NULL)
				if (h != NULL && h->h_name != NULL && strlen(h->h_name) > 0)//check if a name is defined
					ip = strdup(h->h_name);
		}
		else if (inet_pton(AF_INET6, env, addr6) == 1)
		{
			if ((h = gethostbyaddr((char *) &addr6, sizeof(addr6), AF_INET6)) != NULL)
				if (h != NULL && h->h_name != NULL && strlen(h->h_name) > 0)//check if a name is defined
					ip = strdup(h->h_name);
		}
		free(env);
		if (ip == NULL)
			ip = strdup("");
	}
	else
		ip = strdup("");
	return (ip);
}

/*@null@*/ char *get_ip_server()
{
	char *env;
	char *ip, *ptr;

	if ((env = getenv("SSH_CONNECTION")) != NULL)
	{
		env = strdup(env);
		ip = env;
		if ((ptr = strrchr(env, ' ')) != NULL)
			*ptr = '\0';
		if ((ptr = strrchr(env, ' ')) != NULL)
			ip = ptr + 1;
		ip = strdup(ip);
		free(env);
	}
	else
		ip = strdup("");
	return (ip);
}

int get_port_client()
{
	char *ip, *ptr;
	int port = -1;

	if ((ip = getenv("SSH_CONNECTION")) != NULL)
	{
		if ((ptr = strchr(ip, ' ')) != NULL)
		{
			char	*portClient;

			ip = ptr + 1;
			portClient = strdup(ip);
			if ((ptr = strchr(portClient, ' ')) != NULL)
			{
				*ptr = '\0';
				port = atoi(portClient);
			}
			free(portClient);
		}
	}
	return (port);
}

int get_port_server()
{
	char *ip, *ptr;
	int port = -1;

	if ((ip = getenv("SSH_CONNECTION")) != NULL)
	{
		if ((ptr = strrchr(ip, ' ')) != NULL)
		{
			ip = ptr + 1;
			port = atoi(ip);
		}
	}
	return (port);
}

/*
 MySecureShell permit to add restriction to modified sftp-server
 when using MySecureShell as shell.
 Copyright (C) 2007 Sebastien Tardif

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

#include "config.h"
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
	in_addr_t addr;
	char *ip, *ptr;

	if ((ip = getenv("SSH_CONNECTION")) != NULL)
		if ((ptr = strchr(ip, ' ')))
			*ptr = '\0';
	if (resolv == 1 && ip != NULL && (int) (addr = inet_addr(ip)) != -1)
		if ((h = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET)) != NULL)
			if (h != NULL && h->h_name != NULL && strlen(h->h_name) > 0)//check if a name is defined
				ip = (char *) h->h_name;
	return (strdup(ip != NULL ? ip : ""));
}

/*@null@*/ char *get_ip_server()
{
	char *ip, *ptr;

	if ((ip = getenv("SSH_CONNECTION")) != NULL)
	{
		if ((ptr = strrchr(ip, ' ')) != NULL)
			*ptr = '\0';
		if ((ptr = strrchr(ip, ' ')) != NULL)
			ip = ptr + 1;
	}
	return (strdup(ip != NULL ? ip : ""));
}

int get_port_server()
{
	char *ip, *ptr;

	if ((ip = getenv("SSH_CONNECTION")) != NULL)
	{
		if ((ptr = strrchr(ip, ' ')) != NULL)
			ip = ptr + 1;
	}
	return (ip != NULL ? atoi(ip) : 0);
}

/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Web Sniffer.
 * 
 * The Initial Developer of the Original Code is Erik van der Poel.
 * Portions created by Erik van der Poel are
 * Copyright (C) 1998,1999,2000 Erik van der Poel.
 * All Rights Reserved.
 * 
 * Contributor(s): Bruce Robson
 */

#include "plat.h"

#include <errno.h>
#include <memory.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#ifdef PLAT_UNIX
#include <thread.h>
#include <sys/systeminfo.h>
#endif
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "main.h"
#include "mutex.h"
#include "net.h"
#include "view.h"

static int connectCount = 0;
static int dnsCount = 0;

static char *
getHostName(void)
{
	size_t	alloc;
	char	*hostName;
#ifdef PLAT_UNIX
	long	size;
#endif

	alloc = 512;
	hostName = calloc(alloc, 1);
	if (!hostName)
	{
		return NULL;
	}

#ifdef PLAT_UNIX
	while (1)
	{
		size = sysinfo(SI_HOSTNAME, hostName, alloc);
		if (size < 0)
		{
			fprintf(stderr, "sysinfo failed\n");
			return NULL;
		}
		else if (size > alloc)
		{
			alloc = size + 1;
			hostName = realloc(hostName, alloc);
			if (!hostName)
			{
				return NULL;
			}
		}
		else
		{
			break;
		}
	}
#else /* Windows */
        if (gethostname(hostName, alloc) != 0)
        {
                fprintf(stderr, "gethostname failed\n");
                free(hostName);
                return NULL;
        }
#endif

	return hostName;
}

static int
getSocketAndIPAddress(void *a, unsigned char *hostName, int port,
	struct sockaddr_in *addr)
{
#ifdef PLAT_UNIX
	char			buf[512];
	int			err;
#endif
	struct hostent		host;
	struct hostent		*ret;
	unsigned short		shortPort;
	int			sock;
	struct timeval		theTime;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("socket failed");
		return -1;
	}

	viewReport(a, "calling gethostbyname_r() on");
	viewReport(a, (char *) hostName);

	reportStatus(a, "gethostbyname_r", __FILE__, __LINE__);

	gettimeofday(&theTime, NULL);

	/* XXX implement my own DNS lookup to do timeouts? */
	/* XXX implement my own DNS lookup to try again? */
#ifdef PLAT_UNIX
	ret = gethostbyname_r((char *) hostName, &host, buf, sizeof(buf), &err);
	if (!ret)
#else /* Windows */
        ret = gethostbyname((char *) hostName);
        if (ret != NULL)
        {
                host = *ret;
        }
        else
#endif
	{
		reportTime(REPORT_TIME_GETHOSTBYNAME_FAILURE, &theTime);
		reportStatus(a, "gethostbyname_r failed", __FILE__, __LINE__);
		fprintf(stdout, "failed<br><hr><br>");
		close(sock);
		return -1;
	}

	reportTime(REPORT_TIME_GETHOSTBYNAME_SUCCESS, &theTime);

	reportStatus(a, "gethostbyname_r succeeded", __FILE__, __LINE__);

	fprintf(stdout, "succeeded<br><hr><br>");

	MUTEX_LOCK();
	dnsCount++;
	MUTEX_UNLOCK();
	memset(addr, 0, sizeof(*addr));
	addr->sin_family = host.h_addrtype /* PF_INET */;
	shortPort = port;
	addr->sin_port = htons(shortPort);
	memcpy(&addr->sin_addr, host.h_addr, host.h_length /* 4 */);

	return sock;
}

int
netListen(void *a, unsigned char **host, u_short *port)
{
	unsigned char		*hostName;
	struct sockaddr_in	name;
	int			namelen = sizeof(name);
	int			fd;

	hostName = (unsigned char *) getHostName();
	if (!hostName)
	{
		return -1;
	}

	fd = getSocketAndIPAddress(a, hostName, *port, &name);
	if (fd < 0)
	{
		return -1;
	}
	if (host)
	{
		*host = hostName;
	}
	else
	{
		free(hostName);
	}

	if (bind(fd, (struct sockaddr *) &name, sizeof(name)))
	{
		perror("bind");
		return -1;
	}

	if (listen(fd, 5))
	{
		perror("listen");
		return -1;
	}

	if (!*port)
	{
		if (getsockname(fd, (struct sockaddr *) &name, &namelen))
		{
			return -1;
		}
		*port = ntohs(name.sin_port);
	}

	return fd;
}

int
netAccept(int fd)
{
	int		newFD;
	int		addrlen = sizeof(struct sockaddr);
	struct sockaddr	addr;

	while ((newFD = accept(fd, &addr, &addrlen)) < 0)
	{
		if (errno != EINTR)
		{
			return -1;
		}
	}

	return newFD;
}

int
netConnect(void *a, unsigned char *hostName, int port)
{
	struct sockaddr_in	addr;
	int			sock;
	struct timeval		theTime;

	sock = getSocketAndIPAddress(a, hostName, port, &addr);
	if (sock < 0)
	{
		return -1;
	}

	viewReport(a, "calling connect()");

	reportStatus(a, "connect", __FILE__, __LINE__);

	gettimeofday(&theTime, NULL);

	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1)
	{
		reportTime(REPORT_TIME_CONNECT_FAILURE, &theTime);

		/* XXX try again if Connection timed out? */
		/* XXX try again if Connection refused? */
		if
		(
			(errno != ETIMEDOUT) &&
			(errno != ECONNREFUSED)
		)
		{
			fprintf(stderr, "cannot connect to %s at %d: ",
				hostName, port);
			perror(NULL);
		}
		close(sock);
		reportStatus(a, "connect failed", __FILE__, __LINE__);
		viewReport(a, "failed:");
		viewReport(a, strerror(errno) ? strerror(errno) : "NULL");
		fprintf(stdout, "<hr><br>");
		return -1;
	}

	reportTime(REPORT_TIME_CONNECT_SUCCESS, &theTime);

	reportStatus(a, "connect succeeded", __FILE__, __LINE__);

	fprintf(stdout, "succeeded<br><hr><br>");

	MUTEX_LOCK();
	connectCount++;
	MUTEX_UNLOCK();

	return sock;
}

int
netGetConnectCount(void)
{
	return connectCount;
}

int
netGetDNSCount(void)
{
	return dnsCount;
}

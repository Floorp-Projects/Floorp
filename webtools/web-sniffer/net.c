/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bruce Robson <bns_robson@hotmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "all.h"

static int connectCount = 0;
static int dnsCount = 0;

static char *
getHostName(void)
{
	size_t	alloc;
	char	*hostName;

	alloc = 512;
	hostName = calloc(alloc, 1);
	if (!hostName)
	{
		return NULL;
	}

#if HAVE_GETHOSTNAME
        if (gethostname(hostName, alloc) != 0)
        {
                fprintf(stderr, "gethostname failed\n");
                free(hostName);
                return NULL;
        }
#else
#if HAVE_SYSINFO
	while (1)
	{
		long	size;

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
#endif
#endif

	return hostName;
}

static int
getSocketAndIPAddress(App *app, unsigned char *hostName, int port,
	struct sockaddr_in *addr)
{
#if HAVE_GETHOSTBYNAME_R
	char			buf[512];
	int			err;
#endif
	struct hostent		host;
	struct protoent		*proto;
	struct hostent		*ret;
	unsigned short		shortPort;
	int			sock;
	struct timeval		theTime;

	proto = getprotobyname("tcp");
	if (!proto)
	{
		perror("getprotobyname");
		viewReport(app, "getprotobyname failed");
		viewReport(app, strerror(errno) ? strerror(errno) : "NULL");
		return -1;
	}

	sock = socket(PF_INET, SOCK_STREAM, proto->p_proto);
	if (sock < 0)
	{
		perror("socket");
		viewReport(app, "socket failed");
		viewReport(app, strerror(errno) ? strerror(errno) : "NULL");
		return -1;
	}

	viewReport(app, "calling gethostbyname_r() on");
	viewReport(app, (char *) hostName);

	app->status(app, "gethostbyname_r", __FILE__, __LINE__);

	gettimeofday(&theTime, NULL);

	/* XXX implement my own DNS lookup to do timeouts? */
	/* XXX implement my own DNS lookup to try again? */

#if HAVE_GETHOSTBYNAME_R
#if HAVE_GETHOSTBYNAME_R_SOLARIS
	ret = gethostbyname_r((char *) hostName, &host, buf, sizeof(buf), &err);
	if (!ret)
#else
	gethostbyname_r((const char *) hostName, &host, buf, sizeof(buf), &ret,
		&err);
	if (!ret)
#endif
#else
	threadMutexLock();
        ret = gethostbyname((char *) hostName);
        if (ret != NULL)
        {
                host = *ret;
        }
        else
#endif
	{
		app->time(app, appTimeGetHostByNameFailure, &theTime);
		app->status(app, "gethostbyname_r failed", __FILE__, __LINE__);
		viewReportHTML(app, "failed<br><hr>");
		close(sock);
#if !defined(HAVE_GETHOSTBYNAME_R)
		threadMutexUnlock();
#endif
		return -1;
	}

	app->time(app, appTimeGetHostByNameSuccess, &theTime);

	app->status(app, "gethostbyname_r succeeded", __FILE__, __LINE__);

	viewReportHTML(app, "succeeded<br><hr>");

	memset(addr, 0, sizeof(*addr));
	addr->sin_family = host.h_addrtype /* PF_INET */;
	shortPort = port;
	addr->sin_port = htons(shortPort);
	memcpy(&addr->sin_addr, host.h_addr, host.h_length /* 4 */);

#if HAVE_GETHOSTBYNAME_R
	threadMutexLock();
#endif
	dnsCount++;
	threadMutexUnlock();

	return sock;
}

int
netListen(App *app, unsigned char **host, unsigned short *port)
{
	unsigned char		*hostName;
	struct sockaddr_in	name;
	socklen_t		namelen = sizeof(name);
	int			fd;

	hostName = (unsigned char *) getHostName();
	if (!hostName)
	{
		return -1;
	}

	fd = getSocketAndIPAddress(app, hostName, *port, &name);
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
	socklen_t	addrlen = sizeof(struct sockaddr);
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
netConnect(App *app, unsigned char *hostName, int port)
{
	struct sockaddr_in	addr;
	int			sock;
	struct timeval		theTime;

	sock = getSocketAndIPAddress(app, hostName, port, &addr);
	if (sock < 0)
	{
		return -1;
	}

	viewReport(app, "calling connect()");

	app->status(app, "connect", __FILE__, __LINE__);

	gettimeofday(&theTime, NULL);

	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1)
	{
		app->time(app, appTimeConnectFailure, &theTime);

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
		app->status(app, "connect failed", __FILE__, __LINE__);
		viewReport(app, "failed:");
		viewReport(app, strerror(errno) ? strerror(errno) : "NULL");
		viewReportHTML(app, "<hr>");
		return -1;
	}

	app->time(app, appTimeConnectSuccess, &theTime);

	app->status(app, "connect succeeded", __FILE__, __LINE__);

	viewReportHTML(app, "succeeded<br><hr>");

	threadMutexLock();
	connectCount++;
	threadMutexUnlock();

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

int
netInit(void)
{
#if WINDOWS
	int	ret;
	WSADATA	wsaData;

	ret = WSAStartup(0x0001, &wsaData);
	if (ret)
	{
		fprintf(stderr, "WSAStartup failed\n");
		return 0;
	}
#endif

	return 1;
}

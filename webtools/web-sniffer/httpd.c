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
 *
 * ***** END LICENSE BLOCK ***** */

#include "all.h"

typedef void (*Handler)(int fd);

typedef struct FD
{
	Handler	handler;
	FILE	*file;
} FD;

static unsigned short mainPort = 8008;
static int maxFD = -1;
static FD **table = NULL;
static fd_set fdSet;

static FD *
addFD(int fd, Handler func)
{
	FD	*f;

	if (fd > maxFD)
	{
		if (table)
		{
			table = utilRealloc(table,
				(maxFD + 1) * sizeof(*table),
				(fd + 1) * sizeof(*table));
		}
		else
		{
			table = calloc(fd + 1, sizeof(*table));
		}
		if (!table)
		{
			return NULL;
		}
		maxFD = fd;
	}

	f = malloc(sizeof(FD));
	if (!f)
	{
		return NULL;
	}
	f->handler = func;
	f->file = NULL;

	table[fd] = f;

	FD_SET(fd, &fdSet);

	return f;
}

static void
removeFD(int fd)
{
	FD	*f;

	f = table[fd];
	if (f)
	{
		FD_CLR(fd, &fdSet);
		if (f->file && (fileno(f->file) == fd))
		{
			fclose(f->file);
		}
		else
		{
			close(fd);
		}
		free(f);
		table[fd] = NULL;
	}
}

static void
readClientRequest(int fd)
{
	unsigned char	buf[10240];
	int		bytesRead;
	FILE		*file;

	bytesRead = recv(fd, buf, sizeof(buf) - 1, 0);
	if (bytesRead < 0)
	{
		if (errno != ECONNRESET)
		{
			perror("recv");
		}
		removeFD(fd);
		return;
	}
	else if (!bytesRead)
	{
		removeFD(fd);
		return;
	}
	buf[bytesRead] = 0;

	file = fdopen(fd, "w");
	if (!file)
	{
		char *err = "fdopen failed\n";
		send(fd, err, strlen(err), 0);
		removeFD(fd);
		return;
	}

	table[fd]->file = file;

	if (strstr((char *) buf, "/exit"))
	{
		char *goodbye =
			"HTTP/1.0 200 OK\n"
			"Content-Type: text/html\n"
			"\n"
			"Bye!"
		;
		fprintf(file, goodbye);
		removeFD(fd);
		exit(0);
	}
	else if (strstr((char *) buf, "/no-content-type"))
	{
		char *begin =
			"HTTP/1.0 200 OK\n"
			"\n"
		;

		fprintf(file, begin);
		fprintf(file, "<html><head><title>No Content-Type</title>");
		fprintf(file, "</head><body><h1>No Content-Type</h1></body>");
		fprintf(file, "</html>");
		removeFD(fd);
	}
	else
	{
		char *hello =
			"HTTP/1.0 200 OK\n"
			"Content-Type: text/html\n"
			"\n"
		;

		fprintf(file, hello);
		fprintf(file, "<html><head><title>SniffURI httpd</title>");
		fprintf(file, "</head><body>");
		fprintf(file, "<a href=exit>Exit</a><br>");
		fprintf(file,
			"<a href=no-content-type>No Content-Type</a><br>");
		fprintf(file, "</body></html>");
		removeFD(fd);
	}
}

static void
acceptNewClient(int fd)
{
	FD	*f;
	int	newFD;

	newFD = netAccept(fd);
	if (newFD < 0)
	{
		fprintf(stderr, "netAccept failed\n");
		return;
	}

	f = addFD(newFD, readClientRequest);
	if (!f)
	{
		fprintf(stderr, "addFD failed\n");
		return;
	}
}

int
main(int argc, char *argv[])
{
	App	*app;
	FD	*f;
	int	fd;
	fd_set	localFDSet;
	int	ret;

	app = appAlloc();
	fd = netListen(app, NULL, &mainPort);
	if (fd < 0)
	{
		fprintf(stderr, "netListen failed\n");
		return 1;
	}

	f = addFD(fd, acceptNewClient);
	if (!f)
	{
		fprintf(stderr, "addFD failed\n");
		return 1;
	}

	while (1)
	{
		localFDSet = fdSet;
		ret = select(maxFD + 1, &localFDSet, NULL, NULL, NULL);
		if (ret == -1)
		{
			perror("select");
		}
		for (fd = 0; fd <= maxFD; fd++)
		{
			if (FD_ISSET(fd, &localFDSet))
			{
				(*table[fd]->handler)(fd);
			}
		}
	}

	return 0;
}

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

/* host and port of proxy that this proxy connects to */
#define PROXY_HOST "w3proxy.netscape.com"
#define PROXY_PORT 8080
/*
#define PROXY_HOST "127.0.0.1"
#define PROXY_PORT 4444
*/

#include "plat.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef PLAT_WINDOWS
#include <windows.h>
#endif

#include "html.h"
#include "http.h"
#include "mutex.h"
#include "net.h"
#include "utils.h"
#include "view.h"

typedef int (*Handler)(int fd);

typedef struct FD
{
	Handler	handler;
	int	id;
	FILE	*logFile;
	int	port;
	int	suspend;
	int	writeFD;
} FD;

typedef struct Arg
{
	View	*view;
} Arg;

DECLARE_MUTEX;

static fd_set fdSet;
static int id = 0;
static u_short mainPort = 40404;
static int maxFD = -1;
static FD **table = NULL;

static unsigned char suspendStr[1024];

static char *welcome =
"HTTP/1.0 200 OK
Content-Type: text/html

<title>Interceptor</title>
<h3>HTTP Interceptor Persistent Window</h3>
<p>
Keep this window alive as long as you want to continue the session.
It is recommended that you Minimize (Iconify) this window.
Do not click the Back button in this window.
Do not load another document in this window.
</p>
<script>
interceptorSuspendResumeWindow =
	window.open
	(
		\"\",
		\"interceptorSuspendResumeWindow\",
		\"menubar,toolbar,location,directories,scrollbars,status\"
	);
interceptorSuspendResumeWindow.document.write(
\"<title>Welcome to the HTTP Interceptor</title>\" +
\"<h3>Welcome to the HTTP Interceptor</h3>\" +
\"<p>\" +
\"A new HTTP Interceptor session has been started for you. \" +
\"To start using this session, set your HTTP Proxy preference to the \" +
\"following. (Edit | Preferences | Advanced | Proxies | \" +
\"Manual proxy configuration | View | HTTP)\" +
\"</p>\" +
\"<pre>\" +
\"\\n\" +
\"\\tHTTP Proxy Server Address %s; Port %d\" +
\"\\n\" +
\"</pre>\" +
\"<h3>How to Suspend and Resume Logging</h3>\" +
\"<p>\" +
\"You can temporarily suspend and resume the HTTP Interceptor logging \" +
\"feature by clicking the links below.\" +
\"</p>\" +
\"<a href=http://%s:%d/suspend/%d>Suspend Logging</a><br>\" +
\"<a href=http://%s:%d/resume/%d>Resume Logging</a>\" +
\"<p>\" +
\"You may find it useful to drag these links to your Personal Toolbar.\" +
\"</p>\"
);
</script>
";

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
	f->id = -1;
	f->logFile = NULL;
	f->port = 0;
	f->suspend = 0;
	f->writeFD = -1;

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
		if (f->logFile && (fileno(f->logFile) == fd))
		{
			fclose(f->logFile);
		}
		else
		{
			close(fd);
		}
		free(f);
		table[fd] = NULL;
	}
}

static int
logRequest(FD *f, Input *input)
{
	Arg	arg;
	HTTP	*http;

	if
	(
		(table[fileno(f->logFile)]->suspend) ||
		(strstr(current(input), suspendStr))
	)
	{
		table[f->writeFD]->suspend = 1;
		return 0;
	}
	table[f->writeFD]->suspend = 0;
	fprintf
	(
		f->logFile,
		"<script>\n"
			"w%d = window.open(\"\", \"%d\", \"scrollbars\");\n"
			"w%d.document.write(\""
			"<h3>Client Request</h3>"
			"<pre><b>",
		f->id,
		f->id,
		f->id
	);
	http = httpAlloc();
	http->input = input;
	arg.view = viewAlloc();
	arg.view->backslash = 1;
	arg.view->out = f->logFile;
	httpParseRequest(http, &arg, "logRequest");
	free(arg.view);
	httpFree(http);
	fprintf(f->logFile, "</b></pre>\");\n</script>\n");
	fflush(f->logFile);

	return 1;
}

static int
readClientRequest(int fd)
{
	FD	*f;
	Input	*input;

	f = table[fd];
	input = readAvailableBytes(fd);
	write(f->writeFD, current(input), inputLength(input));
	if (!logRequest(f, input))
	{
		inputFree(input);
	}

	return 0;
}

static int
logResponse(FD *f, Input *input)
{
	Arg	arg;
	HTTP	*http;

	if ((table[fileno(f->logFile)]->suspend) || (f->suspend))
	{
		return 0;
	}
	fprintf
	(
		f->logFile,
		"<script>\n"
			"w%d.document.write(\""
			"<h3>Server Response</h3>"
			"<pre><b>",
		f->id
	);
	http = httpAlloc();
	http->input = input;
	arg.view = viewAlloc();
	arg.view->backslash = 1;
	arg.view->out = f->logFile;
	httpParseStream(http, &arg, "readProxyResponse");
	free(arg.view);
	httpFree(http);
	fprintf(f->logFile, "</b></pre>\");\n</script>\n");
	fflush(f->logFile);

	return 1;
}

static int
readProxyResponse(int fd)
{
	FD	*f;
	Input	*input;

	f = table[fd];
	input = readStream(fd, "readProxyResponse");
	write(f->writeFD, current(input), inputLength(input));
	if (!logResponse(f, input))
	{
		inputFree(input);
	}
	removeFD(f->writeFD);
	removeFD(fd);

	return 0;
}

static int
acceptNewClient(int fd)
{
	FD	*client;
	int	clientFD;
	FD	*f;
	FD	*proxy;
	int	proxyFD;

	clientFD = netAccept(fd);
	if (clientFD < 0)
	{
		fprintf(stderr, "netAccept failed\n");
		return 0;
	}

	client = addFD(clientFD, readClientRequest);
	if (!client)
	{
		fprintf(stderr, "addFD failed\n");
		return 0;
	}

	proxyFD = netConnect(NULL, PROXY_HOST, PROXY_PORT);
	if (proxyFD < 0)
	{
		fprintf(stderr, "netConnect to proxy %s:%d failed\n",
			PROXY_HOST, PROXY_PORT);
		return 0;
	}

	proxy = addFD(proxyFD, readProxyResponse);
	if (!proxy)
	{
		fprintf(stderr, "addFD failed\n");
		return 0;
	}

	client->writeFD = proxyFD;
	proxy->writeFD = clientFD;

	f = table[fd];
	client->logFile = f->logFile;
	proxy->logFile = f->logFile;

	client->id = id;
	proxy->id = id;
	id++;

	return 0;
}

static int
readLoggerRequest(int fd)
{
	unsigned char	buf[10240];
	int		bytesRead;
	int		doSuspend;
	FD		*f;
	FILE		*file;
	unsigned char	*host;
	int		i;
	unsigned char	*p;
	u_short		port;
	int		proxyListenFD;
	unsigned char	*resume;
	unsigned char	*str;
	unsigned char	*suspend;
	int		suspendPort;

	bytesRead = read(fd, buf, sizeof(buf) - 1);
	if (bytesRead < 0)
	{
		if (errno != ECONNRESET)
		{
			perror("read");
		}
		removeFD(fd);
		return 0;
	}
	else if (!bytesRead)
	{
		removeFD(fd);
		return 0;
	}
	buf[bytesRead] = 0;

	resume = "/resume";
	suspend = "/suspend";
	if (strstr(buf, "/exit"))
	{
		char *goodbye =
			"HTTP/1.0 200 OK\n"
			"Content-Type: text/html\n"
			"\n"
			"Bye!"
		;
		write(fd, goodbye, strlen(goodbye));
		removeFD(fd);
		return 1;
	}
	else if ((strstr(buf, resume)) || (strstr(buf, suspend)))
	{
		if (strstr(buf, resume))
		{
			str = resume;
			doSuspend = 0;
		}
		else
		{
			str = suspend;
			doSuspend = 1;
		}
		p = strstr(buf, str);
		p += strlen(str);
		if (*p != '/')
		{
			char *notOK =
				"HTTP/1.0 200 OK\n"
				"Content-Type: text/html\n"
				"\n"
				"No backslash after command!"
			;
			write(fd, notOK, strlen(notOK));
			removeFD(fd);
			return 0;
		}
		sscanf(p + 1, "%d", &suspendPort);
		for (i = 0; i <= maxFD; i++)
		{
			if (table[i] && (table[i]->port == suspendPort))
			{
				table[i]->suspend = doSuspend;
				break;
			}
		}
		if (i <= maxFD)
		{
			char *ok =
				"HTTP/1.0 200 OK\n"
				"Content-Type: text/html\n"
				"\n"
				"OK!"
			;
			write(fd, ok, strlen(ok));
		}
		else
		{
			char *notOK =
				"HTTP/1.0 200 OK\n"
				"Content-Type: text/html\n"
				"\n"
				"Cannot find port number in table!"
			;
			write(fd, notOK, strlen(notOK));
		}
		removeFD(fd);
		return 0;
	}

	/* XXX write(1, buf, bytesRead); */

	file = fdopen(fd, "w");
	if (!file)
	{
		char *err = "fdopen failed\n";
		write(fd, err, strlen(err));
		removeFD(fd);
		return 0;
	}
	table[fd]->logFile = file;

	port = 0;
	proxyListenFD = netListen(NULL, &host, &port);
	if (proxyListenFD < 0)
	{
		fprintf(file, "listen failed\n");
		removeFD(fd);
		fclose(file);
		return 0;
	}
	f = addFD(proxyListenFD, acceptNewClient);
	if (!f)
	{
		fprintf(file, "addFD failed\n");
		removeFD(fd);
		fclose(file);
		return 0;
	}

	fprintf
	(
		file,
		welcome,
		host,
		port,
		host,
		mainPort,
		port,
		host,
		mainPort,
		port
	);
	sprintf(suspendStr, "http://%s:%d/suspend/%d", host, mainPort, port);
	free(host);
	fflush(file);
	f->logFile = file;
	table[fd]->port = port;

	return 0;
}

static int
acceptNewLogger(int fd)
{
	FD	*f;
	int	newFD;

	newFD = netAccept(fd);
	if (newFD < 0)
	{
		fprintf(stderr, "netAccept failed\n");
		return 0;
	}

	f = addFD(newFD, readLoggerRequest);
	if (!f)
	{
		fprintf(stderr, "addFD failed\n");
		return 0;
	}

	return 0;
}

void
reportContentType(void *a, unsigned char *contentType)
{
}

void
reportHTML(void *a, Input *input)
{
	Arg	*arg;

	arg = a;
	viewHTML(arg->view, input);
}

void
reportHTMLAttributeName(void *a, HTML *html, Input *input)
{
	Arg	*arg;

	arg = a;
	viewHTMLAttributeName(arg->view, input);
}

void
reportHTMLAttributeValue(void *a, HTML *html, Input *input)
{
	Arg	*arg;

	arg = a;
	viewHTMLAttributeValue(arg->view, input);
}

void
reportHTMLTag(void *a, HTML *html, Input *input)
{
	Arg	*arg;

	arg = a;
	viewHTMLTag(arg->view, input);
}

void
reportHTMLText(void *a, Input *input)
{
	Arg	*arg;

	arg = a;
	viewHTMLText(arg->view, input);
}

void
reportHTTP(void *a, Input *input)
{
	Arg	*arg;

	arg = a;
	viewHTTP(arg->view, input);
}

void
reportHTTPBody(void *a, Input *input)
{
	Arg    *arg;

	arg = a;
	viewHTTP(arg->view, input);
}

void
reportHTTPCharSet(void *a, unsigned char *charset)
{
}

void
reportHTTPHeaderName(void *a, Input *input)
{
	Arg	*arg;

	arg = a;
	viewHTTPHeaderName(arg->view, input);
}

void
reportHTTPHeaderValue(void *a, Input *input, unsigned char *url)
{
	Arg	*arg;

	arg = a;
	viewHTTPHeaderValue(arg->view, input);
}

void
reportStatus(void *a, char *message, char *file, int line)
{
}

void
reportTime(int task, struct timeval *theTime)
{
}

int
main(int argc, char *argv[])
{
	FD	*f;
	int	fd;
	fd_set	localFDSet;
	int	ret;

	MUTEX_INIT();

	fd = netListen(NULL, NULL, &mainPort);
	if (fd < 0)
	{
		fprintf(stderr, "netListen failed\n");
		return 1;
	}

	f = addFD(fd, acceptNewLogger);
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
				if ((*table[fd]->handler)(fd))
				{
					for (fd = 0; fd <= maxFD; fd++)
					{
						removeFD(fd);
					}
					return 0;
				}
			}
		}
	}

	return 1;
}

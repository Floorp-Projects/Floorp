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

static int
readReply(int fd, char *buf, int size)
{
	int	bytesRead;

	bytesRead = recv(fd, buf, size - 1, 0);
	if (bytesRead < 0)
	{
		buf[0] = 0;
	}
	else
	{
		buf[bytesRead] = 0;
	}
	if (bytesRead < 3)
	{
		fprintf(stderr, "bytesRead %d at line %d\n", bytesRead,
			__LINE__);
		return -1;
	}

	return ((buf[0] - '0') * 100) + ((buf[1] - '0') * 10) + (buf[2] - '0');
}

static int
writeRequest(int fd, char *command, char *argument)
{
	char	buf[1024];
	int	bytesWritten;
	int	len;

	strcpy(buf, command);
	if (argument)
	{
		strcat(buf, argument);
	}
	strcat(buf, "\r\n");
	len = strlen(buf);
	bytesWritten = send(fd, buf, len, 0);
	if (bytesWritten != len)
	{
		fprintf(stderr, "bytesWritten at line %d\n", __LINE__);
		return 1;
	}

	return 0;
}

void
ftpProcess(void *a, URL *url)
{
	char	buf[4096];
	int	fd;
	int	port;
	int	reply;
	int	ret;

	if (url->port == -1)
	{
		port = 21;
	}
	else
	{
		port = url->port;
	}

	fd = netConnect(a, url->host, port);
	if (fd < 0)
	{
		fprintf(stderr, "netConnect failed\n");
		return;
	}
	reply = readReply(fd, buf, sizeof(buf));
	if (reply != 220)
	{
		fprintf(stderr, "reply %d at line %d\n", reply, __LINE__);
		return;
	}
	ret = writeRequest(fd, "USER ", "anonymous");
	if (ret)
	{
		return;
	}
	reply = readReply(fd, buf, sizeof(buf));
	if (reply != 331)
	{
		fprintf(stderr, "reply %d buf %s", reply, buf);
		return;
	}
	ret = writeRequest(fd, "PASS ", "foo@bar.com");
	if (ret)
	{
		return;
	}
	reply = readReply(fd, buf, sizeof(buf));
	if (reply != 230)
	{
		fprintf(stderr, "reply %d buf %s", reply, buf);
		return;
	}
	ret = writeRequest(fd, "TYPE ", "I");
	if (ret)
	{
		return;
	}
	reply = readReply(fd, buf, sizeof(buf));
	if (reply == 230)
	{
		reply = readReply(fd, buf, sizeof(buf));
	}
	if (reply != 200)
	{
		fprintf(stderr, "reply %d buf %s", reply, buf);
		return;
	}
	ret = writeRequest(fd, "PASV", NULL);
	if (ret)
	{
		return;
	}
	reply = readReply(fd, buf, sizeof(buf));
	printf("buf %s", buf);
}

void
reportContentType(void *a, unsigned char *contentType)
{
}

void
reportHTML(void *a, Input *input)
{
}

void
reportHTMLAttributeName(void *a, HTML *html, Input *input)
{
}

void
reportHTMLAttributeValue(void *a, HTML *html, Input *input)
{
}

void
reportHTMLTag(void *a, HTML *html, Input *input)
{
}

void
reportHTMLText(void *a, Input *input)
{
}

void
reportHTTP(void *a, Input *input)
{
}

void
reportHTTPBody(void *a, Input *input)
{
}

void
reportHTTPCharSet(void *a, unsigned char *charset)
{
}

void
reportHTTPHeaderName(void *a, Input *input)
{
}

void
reportHTTPHeaderValue(void *a, Input *input, unsigned char *url)
{
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
	unsigned char	*str;
	URL		*url;

	if (!netInit())
	{
		return 1;
	}
	if (!threadInit())
	{
		return 1;
	}

	str = (unsigned char *) "ftp://ftp.somedomain.com/somedir/somefile";
	url = urlParse(str);
	if (!url)
	{
		fprintf(stderr, "urlParse failed\n");
		return 1;
	}
	ftpProcess(NULL, url);

	return 0;
}

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

static unsigned char *emptyHTTPResponse = (unsigned char *) "";
static unsigned char *http09Response = (unsigned char *) "";
static unsigned char *locationURLWasAdded = (unsigned char *) "";

static int nonEmptyHTTPResponseCount = 0;
static int http10OrGreaterCount = 0;

static unsigned short
readLine(Input *input, unsigned short c)
{
	while ((c != 256) && (c != '\r') && (c != '\n'))
	{
		c = getByte(input);
	}
	if (c == '\r')
	{
		c = getByte(input);
		if (c == '\n')
		{
			c = getByte(input);
		}
	}
	else if (c == '\n')
	{
		c = getByte(input);
	}

	return c;
}

static unsigned short
readSpaceTab(Input *input, unsigned short c)
{
	while ((c == ' ') || (c == '\t'))
	{
		c = getByte(input);
	}

	return c;
}

static unsigned short
readNonWhiteSpace(Input *input, unsigned short c)
{
	while
	(
		(c != 256) &&
		(c != ' ') &&
		(c != '\t') &&
		(c != '\r') &&
		(c != '\n')
	)
	{
		c = getByte(input);
	}

	return c;
}

static unsigned char *
httpReadHeaders(HTTP *http, App *app, Input *input, unsigned char *url)
{
	unsigned short	c;
	unsigned char	*charset;
	unsigned char	*contentType;
	int		locationFound;
	unsigned char	*name;
	URL		*rel;
	ContentType	*type;
	unsigned char	*value;

	contentType = NULL;
	locationFound = 0;

	if (!*current(input))
	{
		return emptyHTTPResponse;
	}
	nonEmptyHTTPResponseCount++;
	if (strncmp((char *) current(input), "HTTP/", 5))
	{
		/* XXX deal with HTTP/0.9? */
		return http09Response;
	}
	http10OrGreaterCount++;
	mark(input, 0);
	c = readNonWhiteSpace(input, getByte(input));
	c = readSpaceTab(input, c);
	sscanf((char *) current(input) - 1, "%d", &http->status);
	c = readLine(input, c);
	while (1)
	{
		if (c == 256)
		{
			mark(input, 0);
			app->http(app, input);
			break;
		}
		mark(input, -1);
		app->http(app, input);
		if ((c == '\r') || (c == '\n'))
		{
			readLine(input, c);
			unGetByte(input);
			mark(input, 0);
			app->http(app, input);
			break;
		}
		while
		(
			(c != 256) &&
			(c != '\r') &&
			(c != '\n') &&
			(c != ':')
		)
		{
			c = getByte(input);
		}
		if (c != ':')
		{
			mark(input, -1);
			fprintf(stderr, "no colon in HTTP header \"%s\": %s\n",
				copy(input), url);
			return NULL;
		}
		mark(input, -1);
		app->httpHeaderName(app, input);
		name = copyLower(input);
		c = readSpaceTab(input, getByte(input));
		mark(input, -1);
		app->http(app, input);
		c = readLine(input, c);
		if ((c == ' ') || (c == '\t'))
		{
			do
			{
				c = readLine(input, c);
			} while ((c == ' ') || (c == '\t'));
		}
		c = trimTrailingWhiteSpace(input);
		mark(input, -1);
		value = copy(input);
		if (!strcasecmp((char *) name, "content-type"))
		{
			app->httpHeaderValue(app, input, NULL);
			type = mimeParseContentType(value);
			contentType = mimeGetContentType(type);
			charset = mimeGetContentTypeParameter(type, "charset");
			if (charset)
			{
				app->httpCharSet(app, charset);
			}
			mimeFreeContentType(type);
		}
		else if (!strcasecmp((char *) name, "location"))
		{
			app->httpHeaderValue(app, input, value);
			/* XXX supposed to be absolute URL */
			rel = urlRelative(url, value);
			addURL(app, rel->url);
			urlFree(rel);
			locationFound = 1;
		}
		else
		{
			app->httpHeaderValue(app, input, NULL);
		}
		free(name);
		free(value);
		c = readLine(input, c);
		mark(input, -1);
		app->http(app, input);
	}

	if (!contentType)
	{
		if (locationFound)
		{
			return locationURLWasAdded;
		}
	}

	return contentType;
}

void
httpParseRequest(HTTP *http, App *app, char *url)
{
	unsigned short	c;

	mark(http->input, 0);
	do
	{
		c = getByte(http->input);
	} while (c != 256);
	mark(http->input, -1);
	app->http(app, http->input);
}

static void
httpDefaultType(HTTP *http, App *app)
{
	unsigned short	c;

	do
	{
		c = getByte(http->input);
	}
	while (c != 256);
	mark(http->input, -1);
	app->httpBody(app, http->input);
}

void
httpParseStream(HTTP *http, App *app, unsigned char *url)
{
	const unsigned char	*begin;
	unsigned char		*contentType;

	begin = current(http->input);
	contentType = httpReadHeaders(http, app, http->input, url);
	http->body = current(http->input);
	http->bodyLen = inputLength(http->input) - (http->body - begin);
	if (contentType)
	{
		if
		(
			(contentType != emptyHTTPResponse) &&
			(contentType != http09Response) &&
			(contentType != locationURLWasAdded)
		)
		{
			app->contentType(app, contentType);
			if (!strcasecmp((char *) contentType, "text/html"))
			{
				htmlRead(app, http->input, url);
			}
			else
			{
				httpDefaultType(http, app);
			}
			free(contentType);
		}
	}
	else
	{
		httpDefaultType(http, app);
	}
}

void
httpRead(HTTP *http, App *app, int sock, unsigned char *url)
{
	struct timeval	theTime;

	app->status(app, "readStream", __FILE__, __LINE__);
	gettimeofday(&theTime, NULL);
	http->input = readStream(sock, url);
	app->time(app, appTimeReadStream, &theTime);
	app->status(app, "readStream done", __FILE__, __LINE__);
	httpParseStream(http, app, url);
}

static int
countOrCopy(char **p, char *str)
{
	char	*orig;
	char	*q;

	orig = str;
	if (*p)
	{
		q = *p;
		while ((*q++ = *str++));
		*p = --q;
	}
	else
	{
		while (*str++);
	}

	return --str - orig;
}

static void
httpGetObject(HTTP *http, App *app, int sock, URL *url, unsigned char **headers)
{
	char		*get;
	unsigned char	**h;
	char		*httpStr;
	char		*p;
	int		len;
	int		i;
	char		*buf;

	get = "GET ";
	httpStr = " HTTP/1.0\n";

	p = NULL;
	buf = NULL;
	for (i = 0; i < 2; i++)
	{
		len = countOrCopy(&p, get);
		if (url->path)
		{
			len += countOrCopy(&p, (char *) url->path);
		}
		if (url->params)
		{
			len += countOrCopy(&p, (char *) url->params);
		}
		if (url->query)
		{
			len += countOrCopy(&p, (char *) url->query);
		}
		len += countOrCopy(&p, httpStr);
		h = headers;
		if (h)
		{
			while (*h)
			{
				len += countOrCopy(&p, (char *) *h);
				len += countOrCopy(&p, "\n");
				h++;
			}
		}
		len += countOrCopy(&p, "\n");
		if (!buf)
		{
			buf = malloc(len + 1);
			if (!buf)
			{
				return;
			}
			p = buf;
		}
	}
	send(sock, buf, len, 0);

	httpRead(http, app, sock, url->url);
}

HTTP *
httpAlloc(void)
{
	HTTP	*http;

	http = calloc(sizeof(HTTP), 1);
	if (!http)
	{
		fprintf(stderr, "cannot calloc HTTP\n");
		exit(0);
	}

	return http;
}

void
httpFree(HTTP *http)
{
	if (http)
	{
		inputFree(http->input);
		free(http);
	}
}

HTTP *
httpProcess(App *app, URL *url, unsigned char **headers)
{
	HTTP	*http;
	int	port;
	int	sock;

	port = -1;
	if (url->port == -1)
	{
		port = 80;
	}
	else
	{
		port = url->port;
	}
	if (!url->host)
	{
		fprintf(stderr, "url->host is NULL for %s\n",
			url->url ? (char *) url->url : "<NULL>");
		return NULL;
	}
	sock = netConnect(app, url->host, port);
	if (sock == -1)
	{
		return NULL;
	}

	http = httpAlloc();

	httpGetObject(http, app, sock, url, headers);

	close(sock);

	return http;
}

int
httpGetHTTP10OrGreaterCount(void)
{
	return http10OrGreaterCount;
}

int
httpGetNonEmptyHTTPResponseCount(void)
{
	return nonEmptyHTTPResponseCount;
}

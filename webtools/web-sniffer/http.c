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
readLine(Buf *buf, unsigned short c)
{
	while ((c != 256) && (c != '\r') && (c != '\n'))
	{
		c = bufGetByte(buf);
	}
	if (c == '\r')
	{
		c = bufGetByte(buf);
		if (c == '\n')
		{
			c = bufGetByte(buf);
		}
	}
	else if (c == '\n')
	{
		c = bufGetByte(buf);
	}

	return c;
}

static unsigned short
readNumber(Buf *buf, unsigned short c, int *num)
{
	int	n;

	n = 0;
	while ((c != 256) && (c >= '0') && (c <= '9'))
	{
		n = ((n * 10) + (c - '0'));
		c = bufGetByte(buf);
	}
	*num = n;

	return c;
}

static unsigned short
readSpaceTab(Buf *buf, unsigned short c)
{
	while ((c == ' ') || (c == '\t'))
	{
		c = bufGetByte(buf);
	}

	return c;
}

static unsigned short
readNonWhiteSpace(Buf *buf, unsigned short c)
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
		c = bufGetByte(buf);
	}

	return c;
}

static unsigned short
httpReadHeaders(HTTP *http, App *app, Buf *buf, unsigned char *url,
	unsigned char **ct, int *chunked)
{
	unsigned short	c;
	unsigned char	*charset;
	unsigned char	*contentType;
	int		locationFound;
	unsigned char	*name;
	URL		*rel;
	ContentType	*type;
	unsigned char	*value;
	char		*version;

	app->printHTML(app, "<h4>Response</h4>");
	app->printHTML(app, "<pre>");

	contentType = NULL;
	locationFound = 0;

	bufMark(buf, 0);
	c = bufGetByte(buf);
	if (c == 256)
	{
		*ct = emptyHTTPResponse;
		return c;
	}
	nonEmptyHTTPResponseCount++;
	c = readNonWhiteSpace(buf, c);
	bufMark(buf, -1);
	app->httpResponse(app, buf);
	version = (char *) bufCopy(buf);
	if (!strcmp(version, "HTTP/1.0"))
	{
	}
	else if (!strcmp(version, "HTTP/1.1"))
	{
	}
	else if (!strncmp(version, "HTTP/", 5))
	{
	}
	else
	{
		/* XXX deal with HTTP/0.9? */
		*ct = http09Response;
		return c;
	}
	free(version);
	http10OrGreaterCount++;
	c = readSpaceTab(buf, c);
	c = readNumber(buf, c, &http->status);
	c = readLine(buf, c);
	while (1)
	{
		if (c == 256)
		{
			bufMark(buf, 0);
			app->httpResponse(app, buf);
			break;
		}
		bufMark(buf, -1);
		app->httpResponse(app, buf);
		if ((c == '\r') || (c == '\n'))
		{
			readLine(buf, c);
			bufUnGetByte(buf);
			bufMark(buf, 0);
			app->httpResponse(app, buf);
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
			c = bufGetByte(buf);
		}
		if (c != ':')
		{
			bufMark(buf, -1);
			fprintf(stderr, "no colon in HTTP header \"%s\": %s\n",
				bufCopy(buf), url);
			*ct = NULL;
			return c;
		}
		bufMark(buf, -1);
		app->httpResponseHeaderName(app, buf);
		name = bufCopyLower(buf);
		c = readSpaceTab(buf, bufGetByte(buf));
		bufMark(buf, -1);
		app->httpResponse(app, buf);
		c = readLine(buf, c);
		if ((c == ' ') || (c == '\t'))
		{
			do
			{
				c = readLine(buf, c);
			} while ((c == ' ') || (c == '\t'));
		}
		c = bufTrimTrailingWhiteSpace(buf);
		bufMark(buf, -1);
		value = bufCopy(buf);
		if (!strcasecmp((char *) name, "content-type"))
		{
			app->httpResponseHeaderValue(app, buf, NULL);
			type = mimeParseContentType(value);
			contentType = mimeGetContentType(type);
			charset = mimeGetContentTypeParameter(type, "charset");
			if (charset)
			{
				app->httpResponseCharSet(app, charset);
				free(charset);
			}
			mimeFreeContentType(type);
		}
		else if (!strcasecmp((char *) name, "location"))
		{
			app->httpResponseHeaderValue(app, buf, value);
			/* XXX supposed to be absolute URL */
			rel = urlRelative(url, value);
			addURL(app, rel->url);
			urlFree(rel);
			locationFound = 1;
		}
		else if (!strcasecmp((char *) name, "transfer-encoding"))
		{
			app->httpResponseHeaderValue(app, buf, NULL);
			if (!strcasecmp((char *) value, "chunked"))
			{
				*chunked = 1;
			}
		}
		else
		{
			app->httpResponseHeaderValue(app, buf, NULL);
		}
		free(name);
		free(value);
		c = readLine(buf, c);
		bufMark(buf, -1);
		app->httpResponse(app, buf);
	}

	if (!contentType)
	{
		if (locationFound)
		{
			*ct = locationURLWasAdded;
			return c;
		}
	}

	*ct = contentType;

	return c;
}

void
httpParseRequest(HTTP *http, App *app, char *url)
{
	unsigned short	c;

	bufMark(http->in, 0);
	do
	{
		c = bufGetByte(http->in);
	} while (c != 256);
	bufMark(http->in, -1);
	app->httpResponse(app, http->in);
}

static void
httpDefaultType(HTTP *http, App *app)
{
	unsigned short	c;

	do
	{
		c = bufGetByte(http->in);
	}
	while (c != 256);
	bufMark(http->in, -1);
	app->httpResponseBody(app, http->in);
}

void
httpParseStream(HTTP *http, App *app, unsigned char *url)
{
	Buf		*buf;
	unsigned short	c;
	unsigned char	*contentType;
	int		chunked;
	int		i;
	unsigned char	*line;
	unsigned int	size;

	chunked = 0;
	c = httpReadHeaders(http, app, http->in, url, &contentType, &chunked);

	if (chunked)
	{
		buf = bufAlloc(-1);
		while (1)
		{
			bufMark(http->in, 0);
			c = bufGetByte(http->in);
			c = readLine(http->in, c);
			bufMark(http->in, -1);
			line = bufCopy(http->in);
			size = 0;
			sscanf((char *) line, "%x", &size);
			free(line);
			if (!size)
			{
				break;
			}
			bufUnGetByte(http->in);
			for (i = 0; i < size; i++)
			{
				c = bufGetByte(http->in);
				if (c == 256)
				{
					break;
				}
				else
				{
					bufPutChar(buf, c);
				}
			}
			c = bufGetByte(http->in);
			if (c != '\r')
			{
				break;
			}
			c = bufGetByte(http->in);
			if (c != '\n')
			{
				break;
			}
		}
		bufSet(buf, 0);
		bufMark(buf, 0);
	}
	else
	{
		buf = http->in;
	}
	http->body = bufCurrent(buf);

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
				htmlRead(app, buf, url);
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

	if (chunked)
	{
		bufFree(buf);
	}

	app->printHTML(app, "</pre>");
}

static void
httpRead(App *app, HTTP *http, int sock)
{
	struct timeval	theTime;

	app->status(app, "httpRead", __FILE__, __LINE__);
	gettimeofday(&theTime, NULL);
	http->in = bufAlloc(sock);
	httpParseStream(http, app, http->url->url);
	app->time(app, appTimeReadStream, &theTime);
	app->status(app, "httpRead done", __FILE__, __LINE__);
}

static void
httpPutHeader(App *app, Buf *buf, char *name, char *value)
{
	bufPutString(buf, (unsigned char *) name);
	bufMark(buf, 0);
	app->httpRequestHeaderName(app, buf);
	bufPutString(buf, (unsigned char *) ": ");
	bufMark(buf, 0);
	app->httpRequest(app, buf);
	bufPutString(buf, (unsigned char *) value);
	bufMark(buf, 0);
	app->httpRequestHeaderValue(app, buf);
	bufPutString(buf, (unsigned char *) "\r\n");
	bufMark(buf, 0);
	app->httpRequest(app, buf);
}

static void
httpGetObject(App *app, HTTP *http, int sock)
{
	Buf		*buf;
	HTTPNameValue	*h;

	app->printHTML(app, "<h4>Request</h4>");
	app->printHTML(app, "<pre>");

	buf = bufAlloc(sock);

	bufMark(buf, 0);
	bufPutString(buf, (unsigned char *) "GET ");
	if (http->url->path)
	{
		bufPutString(buf, http->url->path);
	}
	if (http->url->params)
	{
		bufPutString(buf, http->url->params);
	}
	if (http->url->query)
	{
		bufPutString(buf, http->url->query);
	}
	bufPutChar(buf, ' ');
	bufPutString(buf, http->version);
	bufPutString(buf, (unsigned char *) "\r\n");
	bufMark(buf, 0);
	app->httpRequest(app, buf);

	h = http->headers;
	if (h)
	{
		while (h->name)
		{
			httpPutHeader(app, buf, (char *) h->name,
				(char *) h->value);
			h++;
		}
	}

	httpPutHeader(app, buf, "Connection", "close");

	httpPutHeader(app, buf, "Host", (char *) http->url->host);

	bufPutString(buf, (unsigned char *) "\r\n");
	bufMark(buf, 0);
	app->httpRequest(app, buf);

	app->printHTML(app, "</pre>");

	if (bufError(buf))
	{
		return;
	}

	bufSend(buf);

	bufFree(buf);

	httpRead(app, http, sock);
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
		bufFree(http->in);
		free(http);
	}
}

HTTP *
httpProcess(App *app, URL *url, char *version, HTTPNameValue *headers)
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
	http->url = url;
	if (version)
	{
		http->version = (unsigned char *) version;
	}
	else
	{
		http->version = (unsigned char *) "HTTP/1.0";
	}
	http->headers = headers;

	httpGetObject(app, http, sock);

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

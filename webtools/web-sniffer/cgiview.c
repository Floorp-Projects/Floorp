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
 * Contributor(s): 
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "html.h"
#include "http.h"
#include "io.h"
#include "main.h"
#include "mutex.h"
#include "url.h"
#include "utils.h"
#include "view.h"

mutex_t mainMutex;

static char *me = NULL;

static char *passThese[] =
{
	"HTTP_USER_AGENT=",
	"HTTP_ACCEPT=",
	"HTTP_ACCEPT_CHARSET=",
	"HTTP_ACCEPT_LANGUAGE=",
	NULL
};

void
reportHTTPCharSet(void *a, unsigned char *charset)
{
}

void
reportContentType(void *a, unsigned char *contentType)
{
}

void
reportHTML(void *a, Input *input)
{
	View	*view;

	view = a;

	viewHTML(view, input);
}

void
reportHTMLAttributeName(void *a, HTML *html, Input *input)
{
	View	*view;

	view = a;

	viewHTMLAttributeName(view, input);
}

void
reportHTMLAttributeValue(void *a, HTML *html, Input *input)
{
	URL	*url;
	View	*view;
	char	*urlstring;

	view = a;

	if (html->currentAttributeIsURL)
	{
		url = urlRelative(html->base, html->currentAttribute->value);
		urlstring = escapeHTML(url ? (char*) url->url : "");
		fprintf(view->out, "<a href=\"%s%s\">", me, urlstring);
		free(urlstring);
		urlFree(url);
	}
	viewHTMLAttributeValue(view, input);
	if (html->currentAttributeIsURL)
	{
		fprintf(view->out, "</a>");
	}
}

void
reportHTMLTag(void *a, HTML *html, Input *input)
{
	View	*view;

	view = a;

	viewHTMLTag(view, input);
}

void
reportHTMLText(void *a, Input *input)
{
	View	*view;

	view = a;

	viewHTMLText(view, input);
}

void
reportHTTP(void *a, Input *input)
{
	View	*view;

	view = a;

	viewHTTP(view, input);
}

void
reportHTTPBody(void *a, Input *input)
{
	View	*view;

	view = a;

	viewHTTP(view, input);
}

void
reportHTTPHeaderName(void *a, Input *input)
{
	View	*view;

	view = a;

	viewHTTPHeaderName(view, input);
}

void
reportHTTPHeaderValue(void *a, Input *input, unsigned char *url)
{
	View	*view;
	char	*urlstring;

	view = a;

	if (url)
	{
		urlstring = escapeHTML(url);
		fprintf(view->out, "<a href=\"%s%s\">", me, urlstring);
		free(urlstring);
	}
	viewHTTPHeaderValue(view, input);
	if (url)
	{
		fprintf(view->out, "</a>");
	}
}

void
reportStatus(void *a, char *message, char *file, int line)
{
}

void
reportTime(int task, struct timeval *theTime)
{
}

unsigned char **
getHTTPRequestHeaders(View *view, char *host, char *verbose)
{
	char		**e;
	extern char	**environ;
	int		firstLetter;
	char		**h;
	char		*p;
	char		*q;
	char		**r;
	char		**ret;
	char		*scriptName;
	char		*str;

	scriptName = "view.cgi";
	e = environ;
	while (*e)
	{
		e++;
	}
	ret = malloc((e - environ + 1) * sizeof(*e));
	if (!ret)
	{
		return NULL;
	}
	me = malloc(strlen(scriptName) + strlen(verbose) + 1);
	if (!me)
	{
		return NULL;
	}
	strcpy(me, scriptName);
	strcat(me, verbose);

	e = environ;
	r = ret;
	viewReport(view, "will send these HTTP Request headers:");
	while (*e)
	{
		h = passThese;
		while (*h)
		{
			if (!strncmp(*e, *h, strlen(*h)))
			{
				break;
			}
			h++;
		}
		if (*h)
		{
			str = malloc(strlen(*e) - 5 + 1 + 1);
			if (!str)
			{
				continue;
			}
			p = *e + 5;
			q = str;
			while (*p && (*p != '='))
			{
				firstLetter = 1;
				while (*p && (*p != '=') && (*p != '_'))
				{
					if (firstLetter)
					{
						*q++ = *p++;
						firstLetter = 0;
					}
					else
					{
						*q++ = tolower(*p);
						p++;
					}
				}
				if (*p == '_')
				{
					*q++ = '-';
					p++;
				}
			}
			if (*p == '=')
			{
				p++;
				*q++ = ':';
				*q++ = ' ';
				while (*p)
				{
					*q++ = *p++;
				}
				*q = 0;
				*r++ = str;
				viewReport(view, str);
			}
		}
		e++;
	}
	str = malloc(6 + strlen(host) + 1);
	if (str)
	{
		strcpy(str, "Host: ");
		strcat(str, host);
		*r++ = str;
		viewReport(view, str);
	}
	fprintf(view->out, "<hr><br>");
	*r = NULL;

	return (unsigned char **) ret;
}

int
main(int argc, char *argv[])
{
	char		*ampersand;
	unsigned char	*equals;
	char		*name;
	unsigned char	*newURL;
	char		*p;
	char		*query;
	URL		*u;
	unsigned char	*url;
	char		*verbose;
	View		*view;

	MUTEX_INIT();

	url = NULL;

	verbose = "?url=";

	query = getenv("QUERY_STRING");
	view = viewAlloc();
	view->out = stdout;
	freopen("/dev/null", "w", stderr);
	fprintf(view->out, "Content-Type: text/html\n");
	fprintf(view->out, "\n");
	if (query)
	{
		p = query;
		do
		{
			name = p;
			ampersand = strchr(p, '&');
			if (ampersand)
			{
				*ampersand = 0;
				p = ampersand + 1;
			}
			equals = (unsigned char *) strchr(name, '=');
			if (equals)
			{
				*equals = 0;
				if (!strcmp(name, "url"))
				{
					url = equals + 1;
					urlDecode(url);
				}
				else if (!strcmp(name, "verbose"))
				{
					verbose = "?verbose=on&url=";
					viewVerbose();
				}
			}
		} while (ampersand);
	}
	else if (argc > 1)
	{
		url = (unsigned char *) argv[1];
	}
	else
	{
		fprintf(view->out, "no environment variable QUERY_STRING<br>\n");
		fprintf(view->out, "and no arg passed<br>\n");
		return 1;
	}
	if (url && (*url))
	{
		fprintf
		(
			view->out,
			"<html><head><title>View %s</title></head>"
				"<body><tt><b>\n",
			url
		);
		viewReport(view, "input url:");
		viewReport(view, (char *) url);
		fprintf(view->out, "<hr><br>");
		u = urlParse(url);
		if
		(
			((!u->scheme)||(!strcmp((char *) u->scheme, "http"))) &&
			(!u->host) &&
			(*url != '/')
		)
		{
			newURL = calloc(strlen((char *) url) + 3, 1);
			if (!newURL)
			{
				viewReport(view, "calloc failed");
				return 1;
			}
			strcpy((char *) newURL, "//");
			strcat((char *) newURL, (char *) url);
		}
		else
		{
			newURL = copyString(url);
		}
		urlFree(u);
		u = urlParse(newURL);
		if
		(
			(
				(!u->scheme) ||
				(!strcmp((char *) u->scheme, "http"))
			) &&
			(!*u->path)
		)
		{
			url = newURL;
			newURL = calloc(strlen((char *) url) + 2, 1);
			if (!newURL)
			{
				viewReport(view, "calloc failed");
				return 1;
			}
			strcpy((char *) newURL, (char *) url);
			free(url);
			strcat((char *) newURL, "/");
		}
		urlFree(u);
		u = urlRelative(
			(unsigned char *) "http://www.mozilla.org/index.html",
			newURL);
		free(newURL);
		viewReport(view, "fully qualified url:");
		viewReport(view, (char *) u->url);
		fprintf(view->out, "<hr><br>");
		fflush(view->out);
		if (!strcmp((char *) u->scheme, "http"))
		{
			httpProcess(view, u,
				getHTTPRequestHeaders(view, (char *) u->host,
				verbose));
		}
		else
		{
			fprintf
			(
				view->out,
				"Sorry, %s URLs are not supported yet. "
					"Only http URLs are supported.",
				u->scheme
			);
		}
		fprintf(view->out, "</b></tt></body></html>\n");
	}
	else
	{
		viewReport(view, "no URL or empty URL specified");
	}

	exit(0);
	return 1;
}

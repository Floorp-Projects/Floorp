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

static char *me = NULL;

static char *passThese[] =
{
	"HTTP_USER_AGENT=",
	"HTTP_ACCEPT=",
	"HTTP_ACCEPT_CHARSET=",
	"HTTP_ACCEPT_LANGUAGE=",
	NULL
};

static void
cgiviewHTML(App *app, Input *input)
{
	viewHTML(app, input);
}

static void
cgiviewHTMLAttributeName(App *app, HTML *html, Input *input)
{
	viewHTMLAttributeName(app, input);
}

static void
cgiviewHTMLAttributeValue(App *app, HTML *html, Input *input)
{
	unsigned char	*referer;
	URL		*url;
	unsigned char	*urlstring;
	View		*view;

	view = &app->view;

	if (html->currentAttributeIsURL)
	{
		url = urlRelative(html->base, html->currentAttribute->value);
		urlstring = escapeHTML(url ? url->url : (unsigned char *) "");
		fprintf(view->out, "<a href=\"%s%s", me, urlstring);
		free(urlstring);
		referer = escapeHTML(html->url);
		fprintf(view->out, "&referer=%s\">", referer);
		free(referer);
		urlFree(url);
	}
	viewHTMLAttributeValue(app, input);
	if (html->currentAttributeIsURL)
	{
		fprintf(view->out, "</a>");
	}
}

static void
cgiviewHTMLTag(App *app, HTML *html, Input *input)
{
	viewHTMLTag(app, input);
}

static void
cgiviewHTMLText(App *app, Input *input)
{
	viewHTMLText(app, input);
}

static void
cgiviewHTTP(App *app, Input *input)
{
	viewHTTP(app, input);
}

static void
cgiviewHTTPBody(App *app, Input *input)
{
	viewHTTP(app, input);
}

static void
cgiviewHTTPHeaderName(App *app, Input *input)
{
	viewHTTPHeaderName(app, input);
}

static void
cgiviewHTTPHeaderValue(App *app, Input *input, unsigned char *url)
{
	unsigned char	*urlstring;
	View		*view;

	view = &app->view;

	if (url)
	{
		urlstring = escapeHTML(url);
		fprintf(view->out, "<a href=\"%s%s\">", me, urlstring);
		free(urlstring);
	}
	viewHTTPHeaderValue(app, input);
	if (url)
	{
		fprintf(view->out, "</a>");
	}
}

unsigned char **
getHTTPRequestHeaders(App *app, char *host, char *referer, char *verbose)
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
	ret = malloc((e - environ + 2) * sizeof(*e));
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
	viewReport(app, "will send these HTTP Request headers:");
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
				viewReport(app, str);
			}
		}
		e++;
	}
	str = malloc(6 + strlen(host) + 1);
	if (str)
	{
		strcpy(str, "Host: ");
		strcat(str, host);
		/* *r++ = str; */	/* http.c will do Host header */
		viewReport(app, str);
	}
	if (referer)
	{
		str = malloc(9 + strlen(referer) + 1);
		if (str)
		{
			strcpy(str, "Referer: ");
			strcat(str, referer);
			*r++ = str;
			viewReport(app, str);
		}
	}
	viewReportHTML(app, "<hr>");
	*r = NULL;

	return (unsigned char **) ret;
}

int
main(int argc, char *argv[])
{
	char		*ampersand;
	App		*app;
	unsigned char	*equals;
	char		*name;
	unsigned char	*newURL;
	char		*p;
	char		*query;
	unsigned char	*referer;
	URL		*u;
	unsigned char	*url;
	char		*verbose;
	View		*view;

	if (!netInit())
	{
		return 1;
	}
	if (!threadInit())
	{
		return 1;
	}

	referer = NULL;
	url = NULL;

	verbose = "?url=";

	query = getenv("QUERY_STRING");
	app = appAlloc();
	app->html = cgiviewHTML;
	app->htmlAttributeName = cgiviewHTMLAttributeName;
	app->htmlAttributeValue = cgiviewHTMLAttributeValue;
	app->htmlTag = cgiviewHTMLTag;
	app->htmlText = cgiviewHTMLText;
	app->http = cgiviewHTTP;
	app->httpBody = cgiviewHTTPBody;
	app->httpHeaderName = cgiviewHTTPHeaderName;
	app->httpHeaderValue = cgiviewHTTPHeaderValue;
	view = &app->view;
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
				if (!strcmp(name, "referer"))
				{
					referer = equals + 1;
					urlDecode(referer);
				}
				else if (!strcmp(name, "url"))
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
			"<html><head><title>View %s</title></head><body><pre>",
			url
		);
		viewReport(app, "input url:");
		viewReport(app, (char *) url);
		viewReportHTML(app, "<hr>");
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
				viewReport(app, "calloc failed");
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
				viewReport(app, "calloc failed");
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
		viewReport(app, "fully qualified url:");
		viewReport(app, (char *) u->url);
		viewReportHTML(app, "<hr>");
		if (!strcmp((char *) u->scheme, "http"))
		{
			httpProcess(app, u,
				getHTTPRequestHeaders(app, (char *) u->host,
				(char *) referer, verbose));
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
		fprintf(view->out, "</pre></body></html>\n");
	}
	else
	{
		fprintf(view->out, "<html><head><title>SniffURI Error</title>");
		fprintf(view->out, "</head><body><h2>Please enter a URI</h2>");
		fprintf(view->out, "<a href=index.html>Go Back</a>");
		fprintf(view->out, "</body></html>");
	}

	exit(0);
	return 1;
}

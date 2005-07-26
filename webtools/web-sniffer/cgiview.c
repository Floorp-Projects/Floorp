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
	"HTTP_ACCEPT",
	"HTTP_ACCEPT_CHARSET",
	"HTTP_ACCEPT_LANGUAGE",
	"HTTP_USER_AGENT",

	/*
	"HTTP_ACCEPT_ENCODING",
	"HTTP_CONNECTION",
	"HTTP_HOST",
	"HTTP_KEEP_ALIVE",
	"HTTP_REFERER",
	*/

	NULL
};

static void
cgiviewHTML(App *app, Buf *buf)
{
	viewHTML(app, buf);
}

static void
cgiviewHTMLAttributeName(App *app, HTML *html, Buf *buf)
{
	viewHTMLAttributeName(app, buf);
}

static void
cgiviewHTMLAttributeValue(App *app, HTML *html, Buf *buf)
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
		viewHTMLText(app, buf);
		fprintf(view->out, "</a>");
	}
	else
	{
		viewHTMLAttributeValue(app, buf);
	}
}

static void
cgiviewHTMLDeclaration(App *app, Buf *buf)
{
	viewHTMLDeclaration(app, buf);
}

static void
cgiviewHTMLProcessingInstruction(App *app, Buf *buf)
{
	viewHTMLProcessingInstruction(app, buf);
}

static void
cgiviewHTMLTag(App *app, HTML *html, Buf *buf)
{
	viewHTMLTag(app, buf);
}

static void
cgiviewHTMLText(App *app, Buf *buf)
{
	viewHTMLText(app, buf);
}

static void
cgiviewHTTPRequest(App *app, Buf *buf)
{
	viewHTTP(app, buf);
}

static void
cgiviewHTTPRequestHeaderName(App *app, Buf *buf)
{
	viewHTTPHeaderName(app, buf);
}

static void
cgiviewHTTPRequestHeaderValue(App *app, Buf *buf)
{
	viewHTTPHeaderValue(app, buf);
}

static void
cgiviewHTTPResponse(App *app, Buf *buf)
{
	viewHTTP(app, buf);
}

static void
cgiviewHTTPResponseBody(App *app, Buf *buf)
{
	viewHTTP(app, buf);
}

static void
cgiviewHTTPResponseHeaderName(App *app, Buf *buf)
{
	viewHTTPHeaderName(app, buf);
}

static void
cgiviewHTTPResponseHeaderValue(App *app, Buf *buf, unsigned char *url)
{
	unsigned char	*urlstring;
	View		*view;

	view = &app->view;

	if (url)
	{
		urlstring = escapeHTML(url);
		fprintf(view->out, "<a href=\"%s%s\">", me, urlstring);
		free(urlstring);
		viewHTTP(app, buf);
		fprintf(view->out, "</a>");
	}
	else
	{
		viewHTTPHeaderValue(app, buf);
	}
}

static void
cgiviewPrintHTML(App *app, char *str)
{
	viewPrintHTML(app, str);
}

static HTTPNameValue *
cgiviewGetEnv(App *app, char *referer, char *verbose, char **version)
{
	char		**e;
	extern char	**environ;
	char		*equals;
	int		firstLetter;
	char		**h;
	char		*p;
	char		*q;
	HTTPNameValue	*r;
	HTTPNameValue	*ret;
	char		*scriptName;

	scriptName = "view.cgi";
	ret = malloc((NELEMS(passThese) + 1) * sizeof(HTTPNameValue));
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
	viewReport(app, "will ignore these:");
	while (*e)
	{
		equals = strchr(*e, '=');
		if (!equals)
		{
			e++;
			continue;
		}
		*equals = 0;
		h = passThese;
		while (*h)
		{
			if (!strcmp(*e, *h))
			{
				break;
			}
			h++;
		}
		if (*h)
		{
			r->name = malloc(strlen(*e) - 5 + 1);
			if (!r->name)
			{
				return NULL;
			}
			p = *e + 5;
			q = (char *) r->name;
			while (*p)
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
			*q = 0;
			r->value = (unsigned char *) strdup(equals + 1);
			if (!r->value)
			{
				return NULL;
			}
			r++;
		}
		else if (!strncmp(*e, "HTTP_", 5))
		{
			viewReport(app, *e);
		}
		else if (!strcmp(*e, "SERVER_PROTOCOL"))
		{
			*version = strdup(equals + 1);
		}
		e++;
	}
	if (referer)
	{
		r->name = (unsigned char *) strdup("Referer");
		if (!r->name)
		{
			return NULL;
		}
		r->value = (unsigned char *) strdup(referer);
		if (!r->value)
		{
			return NULL;
		}
		r++;
	}
	r->name = NULL;
	viewReportHTML(app, "<hr>");

	return ret;
}

int
main(int argc, char *argv[])
{
	char		*ampersand;
	App		*app;
	unsigned char	*equals;
	HTTPNameValue	*h;
	HTTPNameValue	*headers;
	char		*name;
	unsigned char	*newURL;
	char		*p;
	char		*query;
	unsigned char	*referer;
	URL		*u;
	unsigned char	*url;
	char		*verbose;
	char		*version;
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
	app->htmlDeclaration = cgiviewHTMLDeclaration;
	app->htmlProcessingInstruction = cgiviewHTMLProcessingInstruction;
	app->htmlTag = cgiviewHTMLTag;
	app->htmlText = cgiviewHTMLText;
	app->httpRequest = cgiviewHTTPRequest;
	app->httpRequestHeaderName = cgiviewHTTPRequestHeaderName;
	app->httpRequestHeaderValue = cgiviewHTTPRequestHeaderValue;
	app->httpResponse = cgiviewHTTPResponse;
	app->httpResponseBody = cgiviewHTTPResponseBody;
	app->httpResponseHeaderName = cgiviewHTTPResponseHeaderName;
	app->httpResponseHeaderValue = cgiviewHTTPResponseHeaderValue;
	app->printHTML = cgiviewPrintHTML;
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
		fprintf(view->out, "<html><head><title>View %s</title>", url);
		fprintf(view->out, "<link rel=stylesheet href=view.css>");
		fprintf(view->out, "</head><body>\n");
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
			version = NULL;
			headers = cgiviewGetEnv(app, (char *) referer,
				verbose, &version);
			if (!headers)
			{
				return 1;
			}
			httpFree(httpProcess(app, u, version, headers));
			h = headers;
			while (h->name)
			{
				free(h->name);
				free(h->value);
				h++;
			}
			free(headers);
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
		fprintf(view->out, "</body></html>\n");
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

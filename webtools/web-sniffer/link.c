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

typedef struct Arg
{
	URL	*url;
} Arg;

static char *limitURLs[] =
{
	"http://sniffuri.org/test/",
	NULL
};

static URL *lastURL = NULL;
static URL *urls = NULL;

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

static void
addURLFunc(void *a, URL *url)
{
	lastURL->next = url;
	lastURL = url;
}

int
main(int argc, char *argv[])
{
	Arg	arg;
	HTTP	*http;
	URL	*url;

	if (!netInit())
	{
		return 1;
	}
	if (!threadInit())
	{
		return 1;
	}

	if (argc > 1)
	{
		limitURLs[0] = argv[1];
	}

	addURLInit(addURLFunc, limitURLs, NULL);

	url = urlParse((unsigned char *) limitURLs[0]);
	urls = url;
	lastURL = url;
	while (url)
	{
		arg.url = url;
		http = httpProcess(&arg, url, NULL);
		if (http)
		{
			switch (http->status)
			{
			case 200:
				printf("%s\n", (char *) url->url);
				break;
			case 302:
				break;
			case 403: /* forbidden */
				break;
			case 404:
				/*
				printf("bad link %s\n", url->url);
				*/
				break;
			default:
				printf("status %d for %s\n", http->status,
					url->url);
				break;
			}
			httpFree(http);
		}
		else
		{
			printf("httpProcess failed: %s\n", url->url);
		}
		url = url->next;
	}

	return 0;
}

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

#include <malloc.h>
#include <string.h>

#include "addurl.h"
#include "hash.h"
#include "html.h"
#include "http.h"
#include "main.h"
#include "mutex.h"
#include "url.h"
#include "utils.h"

typedef struct Arg
{
	URL	*url;
} Arg;

mutex_t mainMutex;

static unsigned char *limitURLs[] =
{
	"http://lemming/people/erik/",
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
reportHTMLAttributeName(void *a, Input *input)
{
}

void
reportHTMLAttributeValue(void *a, HTML *html, Input *input)
{
}

void
reportHTMLTag(void *a, Input *input)
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
reportHTTPHeaderValue(void *a, Input *input)
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

	MUTEX_INIT();

	if (argc > 1)
	{
		limitURLs[0] = argv[1];
	}

	addURLInit(addURLFunc, limitURLs, NULL);

	url = urlParse(limitURLs[0]);
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
				printf("%s\n", url->url);
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

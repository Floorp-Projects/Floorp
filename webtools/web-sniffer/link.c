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

static char *limitURLs[] =
{
	"http://sniffuri.org/test/",
	NULL
};

static URL *lastURL = NULL;
static URL *urls = NULL;

static void
addURLFunc(App *app, URL *url)
{
	lastURL->next = url;
	lastURL = url;
}

int
main(int argc, char *argv[])
{
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
		appDefault.data = url;
		http = httpProcess(&appDefault, url, NULL, NULL);
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

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
	NULL,
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

static void
grab(char *dir, unsigned char *url, HTTP *http)
{
	char	*add;
	int	baseLen;
	FILE	*file;
	char	*p;
	char	*slash;
	char	*str;

	baseLen = strlen(limitURLs[0]);
	if (strncmp((char *) url, limitURLs[0], baseLen))
	{
		fprintf(stderr, "no match: %s vs %s\n", url, limitURLs[0]);
		return;
	}

	if (url[strlen((char *) url) - 1] == '/')
	{
		add = "index.html";
	}
	else
	{
		add = "";
	}

	str = calloc(strlen(dir) + 1 + strlen((char *) url + baseLen) +
		strlen(add) + 1, 1);
	if (!str)
	{
		fprintf(stderr, "cannot calloc string\n");
		exit(0);
	}
	strcpy(str, dir);
	strcat(str, "/");
	strcat(str, (char *) url + baseLen);
	p = strchr(str, '#');
	if (p)
	{
		*p = 0;
	}
	strcat(str, add);
	p = str;
	while (1)
	{
		slash = strchr(p, '/');
		if (!slash)
		{
			break;
		}
		*slash = 0;
		if (mkdir(str, 0777))
		{
			if (errno != EEXIST)
			{
				perror("mkdir");
			}
		}
		*slash = '/';
		p = slash + 1;
	}
	file = fopen(str, "w");
	if (!file)
	{
		fprintf(stderr, "cannot open file %s for writing\n", str);
		exit(0);
	}
	bufSetFD(http->in, fileno(file));
	bufSet(http->in, http->body);
	bufWrite(http->in);
	fclose(file);
	free(str);
}

static void
usage(char *prog)
{
	fprintf(stderr, "%s [ -d dir ] [ -u uri ]\n", prog);
	exit(0);
}

int
main(int argc, char *argv[])
{
	char	*dir;
	HTTP	*http;
	int	i;
	char	*prog;
	URL	*url;

	if (!netInit())
	{
		return 1;
	}
	if (!threadInit())
	{
		return 1;
	}

	prog = strrchr(argv[0], '/');
	if (prog)
	{
		prog++;
	}
	else
	{
		prog = argv[0];
	}

	dir = NULL;
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-d"))
		{
			if ((++i < argc) && (!dir))
			{
				dir = argv[i];
			}
			else
			{
				usage(prog);
			}
		}
		else if (!strcmp(argv[i], "-u"))
		{
			if ((++i < argc) && (!limitURLs[0]))
			{
				limitURLs[0] = argv[i];
			}
			else
			{
				usage(prog);
			}
		}
		else
		{
			usage(prog);
		}
	}
	if (!dir)
	{
		dir = "test/grab";
	}
	if (!limitURLs[0])
	{
		limitURLs[0] = "http://sniffuri.org/";
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
			case 200: /* OK */
				grab(dir, url->url, http);
				break;
			case 302: /* Moved Temporarily */
				break;
			case 403: /* Forbidden */
				break;
			case 404: /* Not Found */
				break;
			default:
				printf("status %d\n", http->status);
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

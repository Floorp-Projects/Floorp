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

static AddURLFunc addURLFunc = NULL;

static char **limitDomains = NULL;
static char **limitURLs = NULL;

static HashTable *rejectedURLTable = NULL;
static HashTable *urlTable = NULL;

static void
addThisURL(App *app, unsigned char *str)
{
	int			addIt;
	/*
	HashEntry		*anchorEntry;
	*/
	unsigned char		*fragless;
	int			i;
	char			**limit;
	HashEntry		*urlEntry;
	unsigned char		*sharp;
	URL			*url;

	if (!urlTable)
	{
		return;
	}

	url = urlParse(str);
	addIt = 0;
	if (limitURLs)
	{
		if (limitURLs[0])
		{
			limit = limitURLs;
			while (*limit)
			{
				if (!strncmp(*limit, (char *) url->url,
					strlen(*limit)))
				{
					addIt = 1;
					break;
				}
				limit++;
			}
		}
	}
	else
	{
		if (url->host)
		{
			if (limitDomains[0])
			{
				limit = limitDomains;
				while (*limit)
				{
					i = strlen((char *) url->host) -
						strlen(*limit);
					if (i >= 0)
					{
						if (!strcmp(*limit,
							(char *) &url->host[i]))
						{
							addIt = 1;
							break;
						}
					}
					limit++;
				}
			}
			else
			{
				addIt = 1;
			}
		}
	}
	if (addIt)
	{
		fragless = copyString(url->url);
		sharp = (unsigned char *) strchr((char *) fragless, '#');
		if (sharp)
		{
			*sharp = 0;
		}
		urlEntry = hashLookup(urlTable, fragless);
		if (urlEntry)
		{
			/*
			if (url->fragment)
			{
				anchorEntry = hashLookup(urlEntry->value,
					url->fragment + 1);
			}
			*/
			urlFree(url);
			free(fragless);
		}
		else
		{
			/*
			printf("%s\n", fragless);
			*/
			hashAdd(urlTable, fragless, NULL);
			(*addURLFunc)(app, url);
		}
	}
	else
	{
		urlEntry = hashLookup(rejectedURLTable, url->url);
		if (!urlEntry)
		{
			hashAdd(rejectedURLTable, copyString(url->url), NULL);
			/* XXX
			printf("rejected %s\n", url->url);
			*/
		}
		urlFree(url);
	}
}

void
addURL(App *app, unsigned char *str)
{
	int		len;
	unsigned char	*s;
	unsigned char	*slash;
	unsigned char	*u;
	URL		*url;

	addThisURL(app, str);

	url = urlParse(str);
	if (!url)
	{
		return;
	}
	if ((!url->net_loc) || (!url->path))
	{
		urlFree(url);
		return;
	}
	s = copyString(url->path);
	len = strlen((char *) s);
	if
	(
		(len > 0) &&
		(
			(s[len - 1] != '/') ||
			(len > 1)
		)
	)
	{
		if (s[len - 1] == '/')
		{
			s[len - 1] = 0;
		}
		len = strlen((char *) url->scheme) + 3 +
			strlen((char *) url->net_loc);
		u = calloc(len + strlen((char *) url->path) + 1, 1);
		if (!u)
		{
			fprintf(stderr, "cannot calloc url\n");
			exit(0);
		}
		strcpy((char *) u, (char *) url->scheme);
		strcat((char *) u, "://");
		strcat((char *) u, (char *) url->net_loc);
		while (1)
		{
			slash = (unsigned char *) strrchr((char *) s, '/');
			if (slash)
			{
				slash[1] = 0;
				u[len] = 0;
				strcat((char *) u, (char *) s);
				addThisURL(app, u);
				slash[0] = 0;
			}
			else
			{
				break;
			}
		}
		free(u);
	}
	free(s);
	urlFree(url);
}

static void
urlHandler(App *app, HTML *html)
{
	URL	*url;

	url = urlRelative(html->base, html->currentAttribute->value);
	if (url)
	{
		/*
		printf("--------------------------------\n");
		printf("%s +\n", html->base);
		printf("%s =\n", html->currentAttribute->value);
		printf("%s\n", url->url);
		printf("--------------------------------\n");
		*/
		addURL(app, url->url);
		urlFree(url);
	}
}

void
addURLInit(AddURLFunc func, char **URLs, char **domains)
{
	addURLFunc = func;

	limitURLs = URLs;
	limitDomains = domains;

	rejectedURLTable = hashAlloc(NULL);
	urlTable = hashAlloc(NULL);

	htmlRegisterURLHandler(urlHandler);
}

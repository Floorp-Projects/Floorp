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

static unsigned char *baseURLTest = (unsigned char *) "http://a/b/c/d;p?q#f";

static char *relativeURLTests[] =
{
	"g:h",			"g:h",
	"g",			"http://a/b/c/g",
	"./g",			"http://a/b/c/g",
	"g/",			"http://a/b/c/g/",
	"/g",			"http://a/g",
	"//g",			"http://g",
	"?y",			"http://a/b/c/d;p?y",
	"g?y",			"http://a/b/c/g?y",
	"g?y/./x",		"http://a/b/c/g?y/./x",
	"#s",			"http://a/b/c/d;p?q#s",
	"g#s",			"http://a/b/c/g#s",
	"g#s/./x",		"http://a/b/c/g#s/./x",
	"g?y#s",		"http://a/b/c/g?y#s",
	";x",			"http://a/b/c/d;x",
	"g;x",			"http://a/b/c/g;x",
	"g;x?y#s",		"http://a/b/c/g;x?y#s",
	".",			"http://a/b/c/",
	"./",			"http://a/b/c/",
	"..",			"http://a/b/",
	"../",			"http://a/b/",
	"../g",			"http://a/b/g",
	"../..",		"http://a/",
	"../../",		"http://a/",
	"../../g",		"http://a/g",
	"",			"http://a/b/c/d;p?q#f",
	"../../../g",		"http://a/../g",
	"../../../../g",	"http://a/../../g",
	"/./g",			"http://a/./g",
	"/../g",		"http://a/../g",
	"g.",			"http://a/b/c/g.",
	".g",			"http://a/b/c/.g",
	"g..",			"http://a/b/c/g..",
	"..g",			"http://a/b/c/..g",
	"./../g",		"http://a/b/g",
	"./g/.",		"http://a/b/c/g/",
	"g/./h",		"http://a/b/c/g/h",
	"g/../h",		"http://a/b/c/h",
	"http:g",		"http:g",
	"http:",		"http:",
	NULL
};

static unsigned char *loginTest = (unsigned char *)
	"ftp://user:password@ftp.domain.com:64000/path1/path2/file#fragment";

static void
printURL(URL *url)
{
	printf("url %s\n", url->url);
	printf("scheme %s, ", url->scheme ? (char *) url->scheme : "NULL");
	printf("login %s, ", url->login ? (char *) url->login : "NULL");
	printf("password %s, ", url->password ? (char *)url->password : "NULL");
	printf("host %s, ", url->host ? (char *) url->host : "NULL");
	printf("port %d, ", url->port);
	printf("path %s, ", url->path ? (char *) url->path : "NULL");
	printf("file %s, ", url->file ? (char *) url->file : "NULL");
	printf("fragment %s\n", url->fragment ? (char *)url->fragment : "NULL");
	printf("======================================\n");
}

int
main(int argc, char *argv[])
{
	int		failures;
	char		**p;
	int		total;
	URL		*url;

	printURL(urlParse(loginTest));

	failures = 0;
	total = 0;

	p = relativeURLTests;
	while (p[0])
	{
		total++;
		url = urlRelative(baseURLTest, (unsigned char *) p[0]);
		if (url)
		{
			if (strcmp((char *) url->url, p[1]))
			{
				failures++;
				printf("urlRelative failed:\n");
				printf("\"%s\" +\n", baseURLTest);
				printf("\"%s\" =\n", p[0]);
				printf("\"%s\"\n", url->url);
				printf("should be:\n");
				printf("\"%s\"\n", p[1]);
				printf("-------------------\n");
			}
			urlFree(url);
		}
		else
		{
			failures++;
			printf("urlRelative return NULL for \"%s\"\n", p[0]);
			printf("----------------------------------\n");
		}
		p += 2;
	}
	printf("%d failures out of %d\n", failures, total);

	return 0;
}

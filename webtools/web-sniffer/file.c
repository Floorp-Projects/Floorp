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
#include <stdio.h>
#include <string.h>

#include "file.h"
#include "html.h"
#include "io.h"
#include "url.h"

void
fileProcess(void *a, URL *url)
{
	char	*dot;
	FILE	*file;
	Input	*input;

	/* XXX temporary? */
	if (!url->file)
	{
		return;
	}
	dot = strrchr((char *) url->file, '.');
	if (dot)
	{
		if (strcasecmp(dot, ".html") && strcasecmp(dot, ".htm"))
		{
			return;
		}
	}
	else
	{
		return;
	}

	file = fopen((char *) url->path, "r");
	if (!file)
	{
		fprintf(stderr, "cannot open file %s\n", url->path);
		return;
	}
	input = readStream(fileno(file), url->url);
	htmlRead(a, input, url->url);
	inputFree(input);
}

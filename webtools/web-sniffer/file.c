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

void
fileProcess(void *a, URL *url)
{
	char	*dot;
	FILE	*file;
	Buf	*buf;

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
	buf = bufAlloc(fileno(file));
	if (!buf)
	{
		fprintf(stderr, "cannot alloc buf\n");
		return;
	}
	htmlRead(a, buf, url->url);
	bufFree(buf);
}

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

static void
appContentType(App *app, unsigned char *contentType)
{
}

static void
appHTML(App *app, Input *input)
{
}

static void
appHTMLAttributeName(App *app, HTML *html, Input *input)
{
}

static void
appHTMLAttributeValue(App *app, HTML *html, Input *input)
{
}

static void
appHTMLTag(App *app, HTML *html, Input *input)
{
}

static void
appHTMLText(App *app, Input *input)
{
}

static void
appHTTP(App *app, Input *input)
{
}

static void
appHTTPBody(App *app, Input *input)
{
}

static void
appHTTPCharSet(App *app, unsigned char *charset)
{
}

static void
appHTTPHeaderName(App *app, Input *input)
{
}

static void
appHTTPHeaderValue(App *app, Input *input, unsigned char *url)
{
}

static void
appStatus(App *app, char *message, char *file, int line)
{
}

static void
appTime(App *app, int task, struct timeval *theTime)
{
}

App appDefault =
{
	appContentType,
	appHTML,
	appHTMLAttributeName,
	appHTMLAttributeValue,
	appHTMLTag,
	appHTMLText,
	appHTTP,
	appHTTPBody,
	appHTTPCharSet,
	appHTTPHeaderName,
	appHTTPHeaderValue,
	appStatus,
	appTime
};

App *
appAlloc(void)
{
	App	*app;

	app = calloc(1, sizeof(App));
	if (!app)
	{
		fprintf(stderr, "cannot calloc app\n");
		exit(0);
	}
	*app = appDefault;

	return app;
}

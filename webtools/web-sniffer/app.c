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
appHTML(App *app, Buf *buf)
{
}

static void
appHTMLAttributeName(App *app, HTML *html, Buf *buf)
{
}

static void
appHTMLAttributeValue(App *app, HTML *html, Buf *buf)
{
}

static void
appHTMLDeclaration(App *app, Buf *buf)
{
}

static void
appHTMLProcessingInstruction(App *app, Buf *buf)
{
}

static void
appHTMLTag(App *app, HTML *html, Buf *buf)
{
}

static void
appHTMLText(App *app, Buf *buf)
{
}

static void
appHTTPRequest(App *app, Buf *buf)
{
}

static void
appHTTPRequestHeaderName(App *app, Buf *buf)
{
}

static void
appHTTPRequestHeaderValue(App *app, Buf *buf)
{
}

static void
appHTTPResponse(App *app, Buf *buf)
{
}

static void
appHTTPResponseBody(App *app, Buf *buf)
{
}

static void
appHTTPResponseCharSet(App *app, unsigned char *charset)
{
}

static void
appHTTPResponseHeaderName(App *app, Buf *buf)
{
}

static void
appHTTPResponseHeaderValue(App *app, Buf *buf, unsigned char *url)
{
}

static void
appPrintHTML(App *app, char *str)
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
	appHTMLDeclaration,
	appHTMLProcessingInstruction,
	appHTMLTag,
	appHTMLText,
	appHTTPRequest,
	appHTTPRequestHeaderName,
	appHTTPRequestHeaderValue,
	appHTTPResponse,
	appHTTPResponseBody,
	appHTTPResponseCharSet,
	appHTTPResponseHeaderName,
	appHTTPResponseHeaderValue,
	appPrintHTML,
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

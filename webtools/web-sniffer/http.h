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

#ifndef _HTTP_H_
#define _HTTP_H_

#include "url.h"

typedef struct HTTPNameValue HTTPNameValue;

typedef struct HTTP
{
	unsigned long	body;
	HTTPNameValue	*headers;
	Buf		*in;
	int		status;
	URL		*url;
	unsigned char	*version;
} HTTP;

struct HTTPNameValue
{
	unsigned char	*name;
	unsigned char	*value;
};

HTTP *httpAlloc(void);
void httpFree(HTTP *http);
int httpGetHTTP10OrGreaterCount(void);
int httpGetNonEmptyHTTPResponseCount(void);
void httpParseRequest(HTTP *http, App *app, char *url);
void httpParseStream(HTTP *http, App *app, unsigned char *url);
HTTP *httpProcess(App *app, URL *url, char *version, HTTPNameValue *headers);

#endif /* _HTTP_H_ */

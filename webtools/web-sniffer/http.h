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

#ifndef _HTTP_H_
#define _HTTP_H_

#include <stdio.h>

#include "io.h"
#include "url.h"

typedef struct HTTP
{
	const unsigned char	*body;
	unsigned long		bodyLen;
	Input			*input;
	int			status;
} HTTP;

HTTP *httpAlloc(void);
void httpFree(HTTP *http);
int httpGetHTTP10OrGreaterCount(void);
int httpGetNonEmptyHTTPResponseCount(void);
void httpParseRequest(HTTP *http, void *a, unsigned char *url);
void httpParseStream(HTTP *http, void *a, unsigned char *url);
HTTP *httpProcess(void *a, URL *url, unsigned char **headers);
void httpRead(HTTP *http, void *a, int fd, unsigned char *url);

#endif /* _HTTP_H_ */

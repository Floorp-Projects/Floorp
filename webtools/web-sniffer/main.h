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
 * Contributor(s): Bruce Robson
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include "plat.h"

#ifdef PLAT_UNIX
#include <synch.h>
#else
#include <windows.h>
#endif

#include "html.h"
#include "io.h"

#ifdef PLAT_UNIX
extern mutex_t mainMutex;
#else
extern HANDLE mainMutex;
#endif

#define REPORT_TIME_CONNECT_SUCCESS		0
#define REPORT_TIME_CONNECT_FAILURE		1
#define REPORT_TIME_GETHOSTBYNAME_SUCCESS	2
#define REPORT_TIME_GETHOSTBYNAME_FAILURE	3
#define REPORT_TIME_READSTREAM			4
#define REPORT_TIME_TOTAL			5

#define REPORT_TIME_MAX				6	/* highest + 1 */

void reportContentType(void *a, unsigned char *contentType);
void reportHTML(void *a, Input *input);
void reportHTMLAttributeName(void *a, HTML *html, Input *input);
void reportHTMLAttributeValue(void *a, HTML *html, Input *input);
void reportHTMLTag(void *a, HTML *html, Input *input);
void reportHTMLText(void *a, Input *input);
void reportHTTP(void *a, Input *input);
void reportHTTPBody(void *a, Input *input);
void reportHTTPCharSet(void *a, unsigned char *charset);
void reportHTTPHeaderName(void *a, Input *input);
void reportHTTPHeaderValue(void *a, Input *input, unsigned char *url);
void reportStatus(void *a, char *message, char *file, int line);
void reportTime(int task, struct timeval *theTime);

#endif /* _MAIN_H_ */

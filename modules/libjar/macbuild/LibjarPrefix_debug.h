/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */

 
#include "MacPrefix_debug.h"
 
#define STANDALONE 1
#define XP_MAC 1

#include <string.h>
#include <stdlib.h>
char * strdup(const char *src);
char * strdup(const char *src)
{
	long len = strlen(src);
	char *dup = (char *)malloc(len+1 * sizeof(char));
	memcpy(dup, src, len+1);
	return dup;
}
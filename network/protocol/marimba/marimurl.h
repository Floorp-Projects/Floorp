/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef MKMARIMBA_H
#define MKMARIMBA_H

PR_BEGIN_EXTERN_C

extern void NET_InitMarimbaProtocol(void);

/* MIME Stuff */

typedef struct _NET_AppMarimbaStruct {
    MWContext *context;
    int32      content_length;
    int32      bytes_read;
    int32          bytes_alloc;
    char           *channelData;
} NET_ApplicationMarimbaStruct;


NET_StreamClass * NET_DoMarimbaApplication(int format_out, 
												  void *data_obj, 
												  URL_Struct *url_struct, 
												  MWContext *context);


PR_END_EXTERN_C

#endif 



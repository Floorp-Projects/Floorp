/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef	_AUTOUPDT_H_
#define	_AUTOUPDT_H_

#include "nspr.h"
#include "xp.h"
#include "xp_str.h"
#include "client.h"
#include "net.h"

#ifdef XP_UNIX
#include <sys/fcntl.h>
#elif defined(XP_MAC)
#include <fcntl.h>
#endif

/* autoupdt.c data structures and defines */

#include <stdlib.h>
#include <string.h>

typedef enum _backgroundState {
    BACKGROUND_NEW, 
    BACKGROUND_ERROR, 
    BACKGROUND_COMPLETE, 
    BACKGROUND_SUSPEND, 
    BACKGROUND_DOWN_LOADING,
    BACKGROUND_DOWN_LOADING_NOW,
    BACKGROUND_DONE
} BackgroundState;


typedef struct _AutoUpdateConnnectionStruct {
  /* The following data indicates what we are trying to transfer */
  char*   id;
  char*   url;
  int32   file_size;
  int32   range;
  uint32  interval;
  char*   script;

  /* The following data indicates the current state of the transfer */
  BackgroundState state;
  char*   errorMsg;
  int32   start_byte;
  int32   end_byte;
  char*   outFile;
  void*   timeout;
  MWContext*   mwcontext;

  /* The following data is used while dealing with NET_... routines */
  int32   status;
  void*   stream;
  void*   urls;
  char*   address;
  char*   outputBuffer;
  int32   cur_len;

} AutoUpdateConnnectionStruct;

typedef AutoUpdateConnnectionStruct* AutoUpdateConnnection;

/* autoupdt.c function prototypes */

PR_BEGIN_EXTERN_C

PR_IMPLEMENT(NET_StreamClass *)
Autoupdate_Converter(FO_Present_Types format_out, void *data_object, 
                     URL_Struct *URL_s, MWContext *window_id);

PR_IMPLEMENT(void)
Autoupdate_Suspend(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(void)
Autoupdate_Resume(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(void)
Autoupdate_Done(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(void)
Autoupdate_DownloadNow(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(void)
Autoupdate_Abort(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(AutoUpdateConnnection)
AutoUpdate_Setup(MWContext *cx, char* id, char* url, int32 file_size, 
                 char* script);

PR_IMPLEMENT(void)
AutoUpdate_LoadMainScript(MWContext *cx, char* url);

/* The following are the accessors for the internal data */
PR_IMPLEMENT(const char*)
AutoUpdate_GetID(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(const char*)
AutoUpdate_GetURL(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(BackgroundState)
AutoUpdate_GetState(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(int32)
AutoUpdate_GetFileSize(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(int32)
AutoUpdate_GetCurrentFileSize(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(const char*)
AutoUpdate_GetDestFile(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(int32)
AutoUpdate_GetBytesRange(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(uint32)
AutoUpdate_GetInterval(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(const char*)
AutoUpdate_GetErrorMessage(AutoUpdateConnnection autoupdt);

PR_IMPLEMENT(AutoUpdateConnnection)
AutoUpdate_GetPending(char* id);


PR_END_EXTERN_C

#endif /* _AUTOUPDT_H_ */

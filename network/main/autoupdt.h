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

#define getMem(x) PR_Calloc(1,(x))
#define freeMem(x) PR_Free((x))

#ifndef copyString
#define copyString(source) PL_strdup(source)
#endif

#ifndef stringEquals
#define stringEquals(x, y) (strcasecomp(x, y) ==0)
#endif



#ifndef true
#define true PR_TRUE
#endif
#ifndef false
#define false PR_FALSE
#endif
#define null NULL
#define nullp(x) (((void*)x) == ((void*)0))

typedef struct _AutoUpdateConnnectionStruct {
  char*   url;
  int32   file_size;
  int32   start_byte;
  int32   end_byte;
  int32   range;
  char*   outFile;
  uint32  interval;
  void*   timeout;
  MWContext*   mwcontext;

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

unsigned int autoupdate_write_ready(NET_StreamClass *stream);
int	 autoupdate_write(NET_StreamClass *stream, const char *str, int32 len);
void autoupdate_abort(NET_StreamClass *stream, int status);
void autoupdate_complete(NET_StreamClass *stream);

PUBLIC NET_StreamClass * autoupdate_Converter(FO_Present_Types format_out, 
                                              void *data_object, 
                                              URL_Struct *URL_s, 
                                              MWContext *window_id);

void autoupdate_GetUrlExitFunc(URL_Struct *urls, int status, MWContext *cx);
int  autoupdate_ExecuteFile( const char * cmdline );

#ifdef	XP_MAC
PR_PUBLIC_API(void)
#else
PUBLIC void
#endif
checkForAutoUpdate(void *cx, char* url, int32 file_size, int32 bytes_range, uint32 interval);

PR_END_EXTERN_C

#endif /* _AUTOUPDT_H_ */

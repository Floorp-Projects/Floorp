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

#include "mkutils.h"
#include "autoupdt.h"
#include "client.h"
#include "prefapi.h"
#include "prio.h"

#ifndef MAXPATHLEN
#define  MAXPATHLEN   1024
#endif

#if defined (WIN32)
extern char * FE_GetProgramDirectory(char *buffer, int length);
#elif defined (XP_UNIX)
extern void fe_GetProgramDirectory( char *, int );
#endif

static int32 autoupdt_getCurrentSize(char *file);
static void  autoupdt_free(AutoUpdateConnnection autoupdt);
static void  autoupdate_execute(AutoUpdateConnnection autoupdt);
static void  autoupdate_flush(AutoUpdateConnnection autoupdt);
static void  autoupdate_loadurl(AutoUpdateConnnection autoupdt);
static void  autoupdate_TimerCallback(void* data);
static char* autoupdate_AssureDir(char* path);

AutoUpdateConnnection gAutoUpdateConnnection;

static int32
autoupdt_getCurrentSize(char *file)
{
  struct stat in_stat;
  int stat_result = -1;
  stat_result = stat(file, &in_stat);
  if (stat_result == 0) {
    return in_stat.st_size;
  }
  return 0;
}


static void
autoupdt_free(AutoUpdateConnnection autoupdt)
{
  if (autoupdt == NULL) {
    return;
  }
  PR_FREEIF(autoupdt->url);
  PR_FREEIF(autoupdt->outFile);
  /* Don't free the autoupdt memory because Timer may still have 
   * a pointer to it.
   */
  memset(autoupdt, '\0', sizeof(AutoUpdateConnnectionStruct));
}


static void
autoupdate_execute(AutoUpdateConnnection autoupdt)
{
#ifdef TEST_AUTOUPDATE
  PRBool ok = PR_TRUE;
  ok = FE_Confirm(autoupdt->mwcontext,
                  "A new browser has been downloaded. Should we install it?");
  if (ok) {
    autoupdate_ExecuteFile(autoupdt->outFile);
  }
#endif /* TEST_AUTOUPDATE */
}


static void
autoupdate_flush(AutoUpdateConnnection autoupdt) 
{
  if (autoupdt == NULL) {
    return;
  }
  if ((autoupdt->cur_len > 0) && (autoupdt->outputBuffer != NULL) &&
      (autoupdt->outFile != NULL)) {
    FILE* fp = fopen(autoupdt->outFile, "ab+");
    if (fp != NULL) { 
      fwrite(autoupdt->outputBuffer, 1, autoupdt->cur_len, fp);
      fclose(fp);
    }

    autoupdt->start_byte = autoupdt->start_byte + autoupdt->cur_len;
    autoupdt->end_byte = autoupdt->start_byte + autoupdt->range;
  }

  if (autoupdt->outputBuffer != NULL) {
    PR_FREEIF(autoupdt->outputBuffer);
  }
  autoupdt->outputBuffer = NULL;
  autoupdt->cur_len = 0;
  autoupdt->urls = NULL;
}


unsigned int
autoupdate_write_ready(NET_StreamClass *stream)
{
  /* tell it how many bytes you are willing to take */
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)stream->data_object;
  return autoupdt->range;
}


int 
autoupdate_write(NET_StreamClass *stream, const char* blob, int32 size) 
{
  /*  stream->data_object holds your connection data.
   *  return 0 or greater to get more bytes
   */
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)stream->data_object;

  if ((autoupdt == NULL) || (autoupdt->outFile == NULL) ||
      (autoupdt->range == 0) || (autoupdt->outputBuffer == NULL)) {
    return -1;
  }

  if (autoupdt->cur_len+size > autoupdt->range) {
    autoupdt->outputBuffer = PR_REALLOC(autoupdt->outputBuffer, 
                                        autoupdt->cur_len+size);
    if (!autoupdt->outputBuffer) {
      autoupdt->cur_len = 0;
      TRACEMSG(("Out of memory in autoupdate_write\n"));
      return -1;
    }
  }
  memcpy(&(autoupdt->outputBuffer[autoupdt->cur_len]), blob, size);
  autoupdt->cur_len += size;       
  if (autoupdt->cur_len >= autoupdt->range) {
    /* We got the range of bytes we wanted to get */
    return -1;
  }
    
  return autoupdt->range;
}


void
autoupdate_abort(NET_StreamClass *stream, int status)
{
  /* something bad happened */
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)stream->data_object;
  autoupdate_flush(autoupdt);
}


void
autoupdate_complete(NET_StreamClass *stream)
{
  /* the complete function which gets called when the last packet comes in */
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)stream->data_object;
  autoupdate_flush(autoupdt);
}


#ifdef	XP_MAC
PR_PUBLIC_API(NET_StreamClass *)
#else
PUBLIC NET_StreamClass *
#endif
autoupdate_Converter(FO_Present_Types format_out, void *data_object, 
                     URL_Struct *URL_s, MWContext *cx)
{
    AutoUpdateConnnection autoupdt = (AutoUpdateConnnection) URL_s->fe_data;
    if ((autoupdt == NULL) || (autoupdt->url == NULL)) {
      return NULL;
    }
    if (!URL_s->server_can_do_byteranges) {
      /* Server doesn't support byte ranges. Stop the autoupdate */
      autoupdt_free(autoupdt);
      return NULL;
    }
    TRACEMSG(("Setting up Autoupdate stream. Have URL: %s %s\n", 
              autoupdt->url, URL_s->address));
    
    return NET_NewStream("AUTOUPDATE", (MKStreamWriteFunc)autoupdate_write, 
                         (MKStreamCompleteFunc)autoupdate_complete,
                         (MKStreamAbortFunc)autoupdate_abort, 
                         (MKStreamWriteReadyFunc)autoupdate_write_ready, 
                         URL_s->fe_data, cx);
}


void
autoupdate_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx)
{
  NET_FreeURLStruct (urls);
}


static void
autoupdate_loadurl(AutoUpdateConnnection autoupdt)
{
  URL_Struct *urls;
  char *range=NULL;
  if (autoupdt->url == NULL) {
    return;
  }
  if (NET_AreThereActiveConnections()) {
    return;
  }
  urls = NET_CreateURLStruct(autoupdt->url, NET_DONT_RELOAD);
  if (urls == NULL) {
    return;
  }
  urls->fe_data = autoupdt;
  urls->range_header = PR_sprintf_append(range, "bytes=%ld-%ld", 
                                         autoupdt->start_byte, 
                                         autoupdt->end_byte);
  autoupdt->urls = urls;

  autoupdt->outputBuffer = PR_Malloc(autoupdt->range);
  memset(autoupdt->outputBuffer, '\0', autoupdt->range);
  autoupdt->cur_len = 0;

  autoupdt->status = 1;
  NET_GetURL(urls, FO_AUTOUPDATE, autoupdt->mwcontext, autoupdate_GetUrlExitFunc);
}


static void 
autoupdate_TimerCallback(void* data)
{
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)data;
  if ((autoupdt == NULL) || (autoupdt->url == NULL)) {
    autoupdt_free(autoupdt);
    /* We are not setting timer callbacks, thus we can free autoupdt */
    PR_FREEIF(autoupdt);
    return;
  }

  if (autoupdt->file_size > autoupdt->start_byte) {
    autoupdt->timeout = FE_SetTimeout(autoupdate_TimerCallback, 
                                      (void*)autoupdt, autoupdt->interval);
    autoupdate_loadurl(autoupdt);
  } else {
    autoupdate_execute(autoupdt);
  }
}


void
autoupdate_suspend()
{
}

void
autoupdate_resume()
{
  if (!NET_AreThereActiveConnections())
      autoupdate_TimerCallback((void *)gAutoUpdateConnnection);
}

static char*
autoupdate_AssureDir(char* path)
{
  char *autoupdt_dir = PR_smprintf("%sautoupdt", path);
  if (PR_SUCCESS != PR_Access(autoupdt_dir, PR_ACCESS_WRITE_OK)) {
    if ((PR_MkDir(autoupdt_dir, 0777)) < 0) {
      /* Creation of directory failed. Don't do autoupdate. */
      return NULL;
    }
  }
  return autoupdt_dir;
}

#ifdef	XP_MAC
PR_PUBLIC_API(void)
#else
PUBLIC void
#endif
checkForAutoUpdate(void *cx, char* url, int32 file_size)
{
  int32 bytes_range; 
  int32 interval;
  char *directory = NULL;
#ifdef WIN32 
  char  Path[_MAX_PATH+1];
#else
  char  Path[MAXPATHLEN+1];
#endif
  char *filename;
  char *slash;
  PRInt32 cur_size;
  AutoUpdateConnnection autoupdt;
  XP_Bool enabled;
  char *autoupdt_dir;

  PREF_GetBoolPref( "autoupdate.background_download_enabled", &enabled);
  if (!enabled)
    return;
  
  if (PREF_OK != PREF_CopyCharPref("autoupdate.background_download_directory",
                                   &directory)) {
    directory = NULL;
  } else {
    if ((directory) && (XP_STRCMP(directory, "") == 0)) {
      directory = NULL;
    }
  }
  
  if (PREF_OK != PREF_GetIntPref("autoupdate.background_download_byte_range", 
                                 &bytes_range)) {
    bytes_range = 3000;
  }

  if (PREF_OK != PREF_GetIntPref("autoupdate.background_download_interval", 
                                 &interval)) {
    interval = 10000;
  }

  autoupdt = PR_Malloc(sizeof(AutoUpdateConnnectionStruct));
  memset(autoupdt, '\0', sizeof(AutoUpdateConnnectionStruct));

  autoupdt->url = copyString(url);
  autoupdt->file_size = file_size;
  autoupdt->interval = interval;

  slash = PL_strrchr(url, '/');
  if (slash != NULL) {
    filename = ++slash;
  } else {
    filename = "prg";
  }

#ifdef XP_UNIX

  if (directory) {
    PR_snprintf( Path, MAXPATHLEN, "%s/", directory);
    PR_FREEIF(directory);
  } else {
    directory = getenv("MOZILLA_HOME");
    if (directory) {
      PR_snprintf( Path, MAXPATHLEN, "%s/", directory);
    } else {
      fe_GetProgramDirectory( Path, MAXPATHLEN-1 );
    }
  }
  autoupdt_dir = autoupdate_AssureDir(Path);
  if (autoupdt_dir != NULL) {
    autoupdt->outFile = PR_smprintf("%s/%s", autoupdt_dir, filename);
    PR_FREEIF(autoupdt_dir);
  }
#elif defined(WIN32)

  if (directory) {
    PR_snprintf( Path, MAXPATHLEN, "%s\\", directory);
    PR_FREEIF(directory);
  } else {
    FE_GetProgramDirectory( Path, _MAX_PATH );
  }
  autoupdt_dir = autoupdate_AssureDir(Path);
  if (autoupdt_dir != NULL) {
    autoupdt->outFile = PR_smprintf("%s\\%s", autoupdt_dir, filename);
    PR_FREEIF(autoupdt_dir);
  } 

#elif defined(MAC)
  /* XXX: Fix it for Mac with the correct folder */
  if (directory) {
    PR_snprintf( Path, MAXPATHLEN, "%s:", directory);
    PR_FREEIF(directory);
  } else {
    directory = XP_TempDirName();
    PR_snprintf( Path, MAXPATHLEN, "%s:", directory);
  }
  autoupdt->outFile = PR_smprintf("%s:%s", Path, filename);
#else
  autoupdt->outFile = NULL;
#endif

  if (autoupdt->outFile == NULL) {
    autoupdt_free(autoupdt);
    PR_FREEIF(autoupdt);
    return;
  }

  cur_size = autoupdt_getCurrentSize(autoupdt->outFile);

  if (cur_size >= autoupdt->file_size) {
    /* XXX: We have all the data. We need not do anything more.
     * Should we nag him with do you want to run install 
     * (if he didn't run it already)?
     */
    autoupdt_free(autoupdt);
    PR_FREEIF(autoupdt);
    return;
  }

  autoupdt->start_byte = cur_size;
  autoupdt->end_byte = cur_size + bytes_range;
  autoupdt->range = bytes_range;
  autoupdt->outputBuffer = NULL;
  autoupdt->cur_len = 0;
  
  autoupdt->mwcontext = (MWContext*)cx;  
  gAutoUpdateConnnection = autoupdt;

  autoupdt->timeout = FE_SetTimeout(autoupdate_TimerCallback, 
                                    (void*)autoupdt, autoupdt->interval);
}





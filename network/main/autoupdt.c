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

PR_STATIC_CALLBACK(unsigned int)
autoupdate_WriteReadyNetStream(NET_StreamClass *stream);
PR_STATIC_CALLBACK(int)
autoupdate_WriteNetStream(NET_StreamClass *stream, const char *str, int32 len);
PR_STATIC_CALLBACK(void)
autoupdate_AbortNetStream(NET_StreamClass *stream, int status);
PR_STATIC_CALLBACK(void)
autoupdate_CompleteNetStream(NET_StreamClass *stream);
PR_STATIC_CALLBACK(void)
autoupdate_GetUrlExitFunc(URL_Struct *urls, int status, MWContext *cx);

static int32 autoupdate_GetCurrentSize(char *file);
static void  autoupdate_Free(AutoUpdateConnnection autoupdt);
static void  autoupdate_Flush(AutoUpdateConnnection autoupdt);
static void  autoupdate_LoadURL(AutoUpdateConnnection autoupdt);
static PRBool autoupdate_IsInQueue(AutoUpdateConnnection autoupdt);
static void  autoupdate_TimerCallback(void* data);
static char* autoupdate_AssureDir(char* path);
static void  autoupdate_LoadScript(MWContext* context, char* script);
static void  autoupdate_InvokeJSTimerCallback(void* data);

AutoUpdateConnnection gAutoUpdateConnnection;
char* gMainScript = NULL;

static int32
autoupdate_GetCurrentSize(char *file)
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
autoupdate_Free(AutoUpdateConnnection autoupdt)
{
  if (autoupdt == NULL) {
    return;
  }

  /* XXX: We should remove the downloaded file.
   * We may have to introduce locks while modifying 
   * this data strcuture, because we may be doing 
   * a background transfer. Then we can free all
   * the memory.
   */

  PR_FREEIF(autoupdt->id);
  PR_FREEIF(autoupdt->url);
  PR_FREEIF(autoupdt->outFile);
  /* Don't free the autoupdt memory because Timer may still have 
   * a pointer to it.
   */
  memset(autoupdt, '\0', sizeof(AutoUpdateConnnectionStruct));

  /* XXX: Remove the object from the global list */
  gAutoUpdateConnnection = NULL;
}



static void
autoupdate_Flush(AutoUpdateConnnection autoupdt) 
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

    /* Setup start and end bytes for the next round of download */
    autoupdt->start_byte = autoupdt->start_byte + autoupdt->cur_len;
    autoupdt->end_byte = autoupdt->start_byte + autoupdt->range;
  }

  if (autoupdt->outputBuffer != NULL) {
    PR_FREEIF(autoupdt->outputBuffer);
  }
  autoupdt->outputBuffer = NULL;
  autoupdt->cur_len = 0;
  autoupdt->urls = NULL;
  autoupdt->status = 0;
}


PR_STATIC_CALLBACK(unsigned int)
autoupdate_WriteReadyNetStream(NET_StreamClass *stream)
{
  /* tell it how many bytes you are willing to take */
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)stream->data_object;
  return autoupdt->range;
}


PR_STATIC_CALLBACK(int)
autoupdate_WriteNetStream(NET_StreamClass *stream, const char* blob, int32 size) 
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
      TRACEMSG(("Out of memory in autoupdate_WriteNetStream\n"));
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


PR_STATIC_CALLBACK(void)
autoupdate_AbortNetStream(NET_StreamClass *stream, int status)
{
  /* something bad happened */
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)stream->data_object;
  autoupdate_Flush(autoupdt);
}


PR_STATIC_CALLBACK(void)
autoupdate_CompleteNetStream(NET_StreamClass *stream)
{
  /* the complete function which gets called when the last packet comes in */
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)stream->data_object;
  autoupdate_Flush(autoupdt);
}


PR_IMPLEMENT(NET_StreamClass *)
Autoupdate_Converter(FO_Present_Types format_out, void *data_object, 
                     URL_Struct *URL_s, MWContext *cx)
{
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection) URL_s->fe_data;

  if ((autoupdt == NULL) || (autoupdt->url == NULL)) {
    return NULL;
  }

  if (!URL_s->server_can_do_byteranges) {
    autoupdt->state = BACKGROUND_ERROR;
    autoupdt->errorMsg = "Server doesn't support byte ranges.";
    autoupdate_LoadScript(autoupdt->mwcontext, autoupdt->script);
    return NULL;
  }

  TRACEMSG(("Setting up Autoupdate stream. Have URL: %s %s\n", 
            autoupdt->url, URL_s->address));
    
  autoupdt->status = 1;
  return NET_NewStream("AUTOUPDATE", (MKStreamWriteFunc)autoupdate_WriteNetStream, 
                       (MKStreamCompleteFunc)autoupdate_CompleteNetStream,
                       (MKStreamAbortFunc)autoupdate_AbortNetStream, 
                       (MKStreamWriteReadyFunc)autoupdate_WriteReadyNetStream, 
                       URL_s->fe_data, cx);
}


PR_STATIC_CALLBACK(void)
autoupdate_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx)
{
  NET_FreeURLStruct (urls);
}


static void
autoupdate_LoadURL(AutoUpdateConnnection autoupdt)
{
  URL_Struct *urls;
  char *range=NULL;

  if (NET_AreThereActiveConnections()) {
    /* libnet is busy. Try later */
    return;
  }

  /* If we haven't got all the bytes from the previous request
   * don't start a new one again, until we get all the bytes.
   *
   * This check prevents if NET_AreThereActiveConnections 
   * return FALSE, but we haven't cleared autoupdt's buffers yet.
   */
  if (autoupdt->status == 1) {
    return;
  }

  urls = NET_CreateURLStruct(autoupdt->url, NET_DONT_RELOAD);
  if (urls == NULL) {
    autoupdt->state = BACKGROUND_ERROR;
    autoupdt->errorMsg = "Couldn't set up download. Out of memory";
    autoupdate_LoadScript(autoupdt->mwcontext, autoupdt->script);
    return;
  }

  urls->fe_data = autoupdt;
  urls->range_header = PR_sprintf_append(range, "bytes=%ld-%ld", 
                                         autoupdt->start_byte, 
                                         autoupdt->end_byte);

  autoupdt->outputBuffer = PR_Malloc(autoupdt->range);
  memset(autoupdt->outputBuffer, '\0', autoupdt->range);
  autoupdt->cur_len = 0;

  autoupdt->status = 0;
  NET_GetURL(urls, FO_AUTOUPDATE, autoupdt->mwcontext, autoupdate_GetUrlExitFunc);
  autoupdt->urls = urls;
}

static PRBool
autoupdate_IsInQueue(AutoUpdateConnnection autoupdt)
{
  if (gAutoUpdateConnnection == autoupdt)
    return PR_TRUE;
  return PR_FALSE;
}


static void 
autoupdate_TimerCallback(void* data)
{
  AutoUpdateConnnection autoupdt = (AutoUpdateConnnection)data;

  /* if there is no autoupdate, or the given object is not in our 
   * list of things to do, then stop the timer callback.
   */
  if ((autoupdt == NULL) || (!autoupdate_IsInQueue(autoupdt))) {
    return;
  }

  if (autoupdt->url == NULL) {
    autoupdt->state = BACKGROUND_ERROR;
    autoupdt->errorMsg = "No URL to download";
    autoupdate_LoadScript(autoupdt->mwcontext, autoupdt->script);
    return;
  }

  if ((autoupdt->start_byte == 0) ||
      (autoupdt->file_size > autoupdt->start_byte)) {

    /* We will download bytes if the state is in DOWN_LOADING state.
     * The Script needs to call resume to set the state into DOWN_LOADING
     * and to clear the ERROR or move it from SUSPEND/NEW states.
     * If the state is either COMPLETE or DONE, we don't 
     * need to do anything.
     *
     * We won't enable the timer callbacks, until the script does the 
     * resume
     */
    if (autoupdt->state != BACKGROUND_DOWN_LOADING) {
      return;
    }
    autoupdt->timeout = FE_SetTimeout(autoupdate_TimerCallback, 
                                      (void*)autoupdt, autoupdt->interval);
    autoupdate_LoadURL(autoupdt);
  } else {
    autoupdt->state = BACKGROUND_COMPLETE;
    autoupdate_LoadScript(autoupdt->mwcontext, autoupdt->script);
  }
}


PR_IMPLEMENT(void)
Autoupdate_Suspend(AutoUpdateConnnection autoupdt)
{
  if (autoupdt == NULL) {
    return;
  }

  autoupdt->state = BACKGROUND_SUSPEND;
  /* We need to stop everything right away. They way it is now, 
   * it will stop next time around.
   */
}

PR_IMPLEMENT(void)
Autoupdate_Resume(AutoUpdateConnnection autoupdt)
{
  if (autoupdt == NULL) {
    return;
  }

  autoupdt->state = BACKGROUND_DOWN_LOADING;
  autoupdate_TimerCallback((void *)autoupdt);
}


PR_IMPLEMENT(void)
Autoupdate_Done(AutoUpdateConnnection autoupdt)
{
  if (autoupdt == NULL) {
    return;
  }

  autoupdate_Free(autoupdt);
}


PR_IMPLEMENT(void)
Autoupdate_DownloadNow(AutoUpdateConnnection autoupdt)
{
  if (autoupdt == NULL) {
    return;
  }

  autoupdt->state = BACKGROUND_DOWN_LOADING_NOW;
  /* We should initiate a new command to donwload all bytes.
   * May be we should directly all bytes into the file, instead of 
   * trying to store data into a buffer.
   *
   * Changing the state to BACKGROUND_DOWN_LOADING_NOW, will stop
   * the background download.
   *
   */
}


PR_IMPLEMENT(void)
Autoupdate_Abort(AutoUpdateConnnection autoupdt)
{
  if (autoupdt == NULL) {
    return;
  }

  /* XXX: We should remove the downloaded file.
   * We should free up the memory.
   * We should disable TimerCallbacks for this object.
   */
  autoupdate_Free(autoupdt);
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


PR_IMPLEMENT(AutoUpdateConnnection)
AutoUpdate_Setup(MWContext* cx, char* id, char* url, int32 file_size, 
                 char* script)
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
    return NULL;
  
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

  /* TODO: XXX Support downloading of multiple downloads */
  if (gAutoUpdateConnnection)
    return gAutoUpdateConnnection;

  autoupdt = PR_Malloc(sizeof(AutoUpdateConnnectionStruct));
  memset(autoupdt, '\0', sizeof(AutoUpdateConnnectionStruct));

  autoupdt->id = PL_strdup(id);
  autoupdt->url = PL_strdup(url);
  autoupdt->file_size = file_size;
  autoupdt->interval = interval;
  autoupdt->range = bytes_range;
  autoupdt->script = PL_strdup(script);

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

  autoupdt->outputBuffer = NULL;
  autoupdt->cur_len = 0;
  autoupdt->mwcontext = cx;  
  autoupdt->start_byte = 0;
  autoupdt->end_byte = 0;

  if (autoupdt->outFile == NULL) {
    autoupdt->state = BACKGROUND_ERROR;
    autoupdt->errorMsg = "Couldn't access destination directory to save file";
    return autoupdt;
  }

  cur_size = autoupdate_GetCurrentSize(autoupdt->outFile);
  autoupdt->start_byte = cur_size;
  if (autoupdt->start_byte == 0) {
    autoupdt->state = BACKGROUND_NEW;
    autoupdt->end_byte = cur_size + bytes_range;
  } else if (cur_size >= autoupdt->file_size) {
    autoupdt->end_byte = cur_size;
    autoupdt->state = BACKGROUND_COMPLETE;
  } else {
    autoupdt->end_byte = cur_size + bytes_range;
    autoupdt->state = BACKGROUND_SUSPEND;
  }

  /* TODO: XXX Push the object into the link list */
  gAutoUpdateConnnection = autoupdt;
  return autoupdt;
}

static void 
autoupdate_LoadScript(MWContext* context, char* script)
{
  MWContext* new_context;
  Chrome chrome;
  URL_Struct* urls;  

  /* if there is no JS to run, then exit.
   */
  if ((script == NULL) || (context == NULL)) {
    return;
  }
  
  XP_BZERO(&chrome, sizeof(Chrome));
  chrome.location_is_chrome = TRUE;
  chrome.type = MWContextDialog;
  chrome.l_hint = -3000;
  chrome.t_hint = -3000;
  urls = NET_CreateURLStruct(script, NET_DONT_RELOAD);
  if (urls == NULL) {
    return;
  }
  new_context = FE_MakeNewWindow(context, NULL, NULL, &chrome);
  if (new_context == NULL)
    return;
  (void)FE_GetURL(new_context, urls);
}


static void 
autoupdate_InvokeJSTimerCallback(void* data)
{
  autoupdate_LoadScript((MWContext*)data, gMainScript);
}

PR_IMPLEMENT(void)
AutoUpdate_LoadMainScript(MWContext * context, char* url)
{
  gMainScript = PL_strdup(url);
  /* May be we should download the script */
  FE_SetTimeout(autoupdate_InvokeJSTimerCallback, 
                (void*)context, 3000);
}

PR_IMPLEMENT(const char*)
AutoUpdate_GetID(AutoUpdateConnnection autoupdt)
{
  return autoupdt->id;
}


PR_IMPLEMENT(const char*)
AutoUpdate_GetURL(AutoUpdateConnnection autoupdt)
{
  return autoupdt->url;
}

PR_IMPLEMENT(BackgroundState)
AutoUpdate_GetState(AutoUpdateConnnection autoupdt)
{
  return autoupdt->state;
}

PR_IMPLEMENT(int32)
AutoUpdate_GetFileSize(AutoUpdateConnnection autoupdt)
{
  return autoupdt->file_size;
}

PR_IMPLEMENT(int32)
AutoUpdate_GetCurrentFileSize(AutoUpdateConnnection autoupdt)
{
  return autoupdt->start_byte;
}

PR_IMPLEMENT(const char*)
AutoUpdate_GetDestFile(AutoUpdateConnnection autoupdt)
{
  return autoupdt->outFile;
}

PR_IMPLEMENT(int32)
AutoUpdate_GetBytesRange(AutoUpdateConnnection autoupdt)
{
  return autoupdt->range;
}

PR_IMPLEMENT(uint32)
AutoUpdate_GetInterval(AutoUpdateConnnection autoupdt)
{
  return autoupdt->interval;
}

PR_IMPLEMENT(const char*)
AutoUpdate_GetErrorMessage(AutoUpdateConnnection autoupdt)
{
  return autoupdt->errorMsg;
}


PR_IMPLEMENT(AutoUpdateConnnection)
AutoUpdate_GetPending(char* id)
{
  /* XXX: We should look up in a hash table and return the object.
   * Because we have only one download going on, just return the global object
   */
  return gAutoUpdateConnnection;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xp.h"
#include <windowsx.h>

#include "resource.h"
#include "logger.h"
#include "profile.h"
#include "plugload.h"

Logger::Logger() :
  bMutedAll(FALSE),
  bOnTop(TRUE),
  bToWindow(TRUE),
  bToConsole(FALSE),
  bToFile(FALSE),
  bSPALID(FALSE)
{
  if(0 != GetPluginsDir(szFile, strlen(szFile)))
  {
    strcat(szFile, "\\");
    strcat(szFile, DEFAULT_LOG_FILE_NAME);
  }
  else
    szFile[0] = '\0';

  for(int i = 0; i < sizeof(bMutedCalls)/sizeof(BOOL); i++)
    bMutedCalls[i] = FALSE;
  
  bMutedCalls[action_npn_mem_alloc] = TRUE;
  bMutedCalls[action_npn_mem_free]  = TRUE;
  bMutedCalls[action_npn_mem_flush] = TRUE;
}

Logger::~Logger()
{
}

BOOL Logger::init()
{
  if(bToFile)
    filer.create(szFile, TRUE);

  return TRUE;
}

void Logger::shut()
{
  filer.close();
}

#define MAX_OUTPUT_SIZE 8192

void Logger::logNS_NP_GetEntryPoints()
{
  char szLog[] = "NP_GetEntryPoints by Netscape\r\n";

  if(bToConsole)
    printf("%s", szLog);
  
  if(bToFile)
    filer.write(szLog);

  if(bToWindow)
    dumpStringToMainWindow(szLog);
}

void Logger::logNS_NP_Initialize()
{
  char szLog[] = "NP_Initialize by Netscape\r\n";

  if(bToConsole)
    printf("%s", szLog);
  
  if(bToFile)
    filer.write(szLog);

  if(bToWindow)
    dumpStringToMainWindow(szLog);
}

void Logger::logNS_NP_Shutdown()
{
  char szLog[] = "NP_Shutdown by Netscape\r\n";

  if(bToConsole)
    printf("%s", szLog);
  
  if(bToFile)
    filer.write(szLog);

  if(bToWindow)
    dumpStringToMainWindow(szLog);
}

void Logger::logSPY_NP_GetEntryPoints(NPPluginFuncs * pNPPFuncs)
{
  char szLog[80] = "NP_GetEntryPoints by NPSpy\r\n";

  if(bToConsole)
    printf("%s", szLog);
  
  if(bToFile)
    filer.write(szLog);

  if(bToWindow)
    dumpStringToMainWindow(szLog);

  if(!pNPPFuncs)
    return;

  char szLog1[80],szLog2[80],szLog3[80],szLog4[80],szLog5[80],szLog6[80],szLog7[80],
       szLog8[80],szLog9[80],szLog10[80],szLog11[80],szLog12[80],szLog13[80],szLog14[80],
       szLog15[80],szLog16[80],szLog17[80],szLog18[80],szLog19[80],szLog20[80];

  sprintf(szLog1, "\r\n");
  sprintf(szLog2, "     Plugin entry point table\r\n");
  sprintf(szLog3, "     ========================\r\n");

  if(pNPPFuncs->size)
    sprintf(szLog4, "    size          = %i\r\n", pNPPFuncs->size);
  else
    sprintf(szLog4, "    size          = not set!\r\n");

  if(pNPPFuncs->version)
    sprintf(szLog5, "    version       = %i\r\n", pNPPFuncs->version);
  else
    sprintf(szLog5, "    version       = not set!\r\n");
  
  if(pNPPFuncs->newp)
    sprintf(szLog6, "    newp          = %#08lx\r\n", pNPPFuncs->newp);
  else
    sprintf(szLog6, "    newp          = not set!\r\n");
  
  if(pNPPFuncs->destroy)
    sprintf(szLog7, "    destroy       = %#08lx\r\n", pNPPFuncs->destroy);
  else
    sprintf(szLog7, "    destroy       = not set!\r\n");
  
  if(pNPPFuncs->setwindow)
    sprintf(szLog8, "    setwindow     = %#08lx\r\n", pNPPFuncs->setwindow);
  else
    sprintf(szLog8, "    setwindow     = not set!\r\n");
  
  if(pNPPFuncs->newstream)
    sprintf(szLog9, "    newstream     = %#08lx\r\n", pNPPFuncs->newstream);
  else
    sprintf(szLog9, "    newstream     = not set!\r\n");
  
  if(pNPPFuncs->destroystream)
    sprintf(szLog10, "    destroystream = %#08lx\r\n", pNPPFuncs->destroystream);
  else
    sprintf(szLog10, "    destroystream = not set!\r\n");
  
  if(pNPPFuncs->asfile)
    sprintf(szLog11, "    asfile        = %#08lx\r\n", pNPPFuncs->asfile);
  else
    sprintf(szLog11, "    asfile        = not set!\r\n");
  
  if(pNPPFuncs->writeready)
    sprintf(szLog12, "    writeready    = %#08lx\r\n", pNPPFuncs->writeready);
  else
    sprintf(szLog12, "    writeready    = not set!\r\n");
  
  if(pNPPFuncs->write)
    sprintf(szLog13, "    write         = %#08lx\r\n", pNPPFuncs->write);
  else
    sprintf(szLog13, "    write         = not set!\r\n");
  
  if(pNPPFuncs->print)
    sprintf(szLog14, "    print         = %#08lx\r\n", pNPPFuncs->print);
  else
    sprintf(szLog14, "    print         = not set!\r\n");
  
  if(pNPPFuncs->event)
    sprintf(szLog15, "    event         = %#08lx\r\n", pNPPFuncs->event);
  else
    sprintf(szLog15, "    event         = not set!\r\n");
  
  if(pNPPFuncs->urlnotify)
    sprintf(szLog16, "    urlnotify     = %#08lx\r\n", pNPPFuncs->urlnotify);
  else
    sprintf(szLog16, "    urlnotify     = not set!\r\n");
  
  if(pNPPFuncs->javaClass)
    sprintf(szLog17, "    javaClass     = %#08lx\r\n", pNPPFuncs->javaClass);
  else
    sprintf(szLog17, "    javaClass     = not set!\r\n");
  
  if(pNPPFuncs->getvalue)
    sprintf(szLog18, "    getvalue      = %#08lx\r\n", pNPPFuncs->getvalue);
  else
    sprintf(szLog18, "    getvalue      = not set!\r\n");
  
  if(pNPPFuncs->setvalue)
    sprintf(szLog19, "    setvalue      = %#08lx\r\n", pNPPFuncs->setvalue);
  else
    sprintf(szLog19, "    setvalue      = not set!\r\n");

  sprintf(szLog20, "\r\n");

  if(bToConsole)
  {
    printf("%s", szLog1); printf("%s", szLog2); printf("%s", szLog3); printf("%s", szLog4);
    printf("%s", szLog5); printf("%s", szLog6); printf("%s", szLog7); printf("%s", szLog8);
    printf("%s", szLog9); printf("%s", szLog10); printf("%s", szLog11); printf("%s", szLog12);
    printf("%s", szLog13); printf("%s", szLog14); printf("%s", szLog15); printf("%s", szLog16);
    printf("%s", szLog17); printf("%s", szLog18); printf("%s", szLog19); printf("%s", szLog20);
  }
  
  if(bToFile)
  {
    filer.write(szLog1); filer.write(szLog2); filer.write(szLog3); filer.write(szLog4);
    filer.write(szLog5); filer.write(szLog6); filer.write(szLog7); filer.write(szLog8);
    filer.write(szLog9); filer.write(szLog10); filer.write(szLog11); filer.write(szLog12);
    filer.write(szLog13); filer.write(szLog14); filer.write(szLog15); filer.write(szLog16);
    filer.write(szLog17); filer.write(szLog18); filer.write(szLog19); filer.write(szLog20);
  }
  
  if(bToWindow)
  {
    dumpStringToMainWindow(szLog1); dumpStringToMainWindow(szLog2);
    dumpStringToMainWindow(szLog3); dumpStringToMainWindow(szLog4);
    dumpStringToMainWindow(szLog5); dumpStringToMainWindow(szLog6);
    dumpStringToMainWindow(szLog7); dumpStringToMainWindow(szLog8);
    dumpStringToMainWindow(szLog9); dumpStringToMainWindow(szLog10);
    dumpStringToMainWindow(szLog11); dumpStringToMainWindow(szLog12);
    dumpStringToMainWindow(szLog13); dumpStringToMainWindow(szLog14);
    dumpStringToMainWindow(szLog15); dumpStringToMainWindow(szLog16);
    dumpStringToMainWindow(szLog17); dumpStringToMainWindow(szLog18);
    dumpStringToMainWindow(szLog19); dumpStringToMainWindow(szLog20);
  }
}

void Logger::logSPY_NP_Initialize()
{
  char szLog[] = "NP_Initialize by NPSpy\r\n";

  if(bToConsole)
    printf("%s", szLog);
  
  if(bToFile)
    filer.write(szLog);

  if(bToWindow)
    dumpStringToMainWindow(szLog);
}

void Logger::logSPY_NP_Shutdown(char * mimetype)
{
  char szLog[512] = "NP_Shutdown by NPSpy\r\n";
  if(mimetype)
  {
    strcat(szLog, " for \"");
    strcat(szLog, mimetype);
    strcat(szLog, "\"");
  }

  if(bToConsole)
    printf("%s", szLog);
  
  if(bToFile)
    filer.write(szLog);

  if(bToWindow)
    dumpStringToMainWindow(szLog);
}

void Logger::logCall(NPAPI_Action action, DWORD dw1, DWORD dw2, DWORD dw3, DWORD dw4, DWORD dw5, DWORD dw6, DWORD dw7)
{
  if(isMuted(action))
    return;

  static char szLog[512];

  LogItemStruct * lis = makeLogItemStruct(action, dw1, dw2, dw3, dw4, dw5, dw6, dw7);
  int iLength = formatLogItem(lis, szLog, TRUE);
  freeLogItemStruct(lis);

  if(bToConsole)
    printf("%s", szLog);
  
  if(bToFile)
    filer.write(szLog);

  if(bToWindow)
    dumpStringToMainWindow(szLog);
}

void Logger::logReturn(DWORD dwRet)
{
}

void Logger::setOnTop(BOOL ontop)
{
  bOnTop = ontop;
}

void Logger::setToFile(BOOL tofile, char * filename)
{
  if(!filename || (strlen(filename) == 0) || (strlen(filename) > _MAX_PATH))
  {
    bToFile = FALSE;
    return;
  }

  //don't screw up the file on false call
  BOOL samefile = (stricmp(szFile, filename) == 0);
  BOOL sameaction = (bToFile == tofile);
  
  if(sameaction)
  {
    if(samefile)
      return;

    strcpy(szFile, filename);

    if(bToFile)
    {
      filer.close();
      filer.create(szFile, TRUE);
    }
  }

  if(!sameaction)
  {
    bToFile = tofile;

    if(!samefile)
      strcpy(szFile, filename);

    if(bToFile)
      filer.create(szFile, TRUE);
    else
      filer.close();
  }
}

BOOL Logger::isMuted(NPAPI_Action action)
{
  if(bMutedAll)
    return TRUE;

  if(action >= TOTAL_NUMBER_OF_API_CALLS)
  {
    assert(0);
    return FALSE;
  }

  return bMutedCalls[action];
}

BOOL * Logger::getMutedCalls()
{
  return &bMutedCalls[0];
}

void Logger::setMutedCalls(BOOL * mutedcalls)
{
  for(int i = 0; i < sizeof(bMutedCalls)/sizeof(BOOL); i++)
    bMutedCalls[i] = mutedcalls[i];
}

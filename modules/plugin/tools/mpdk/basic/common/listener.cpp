/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/**********************************************************************
*
* listener.cpp
*
* Implementation of the plugin stream listener class
*
***********************************************************************/

#include "xplat.h"
#include "listener.h"
#include "dbg.h"

extern PRUint32 gPluginObjectCount;

CPluginStreamListener::CPluginStreamListener(CPluginInstance* inst, const char* msgName) :
  fMessageName(msgName)
{
  gPluginObjectCount++;
  NS_INIT_REFCNT();
  dbgOut3("CPluginStreamListener::CPluginStreamListener, message '%s', gPluginObjectCount = %lu", 
          fMessageName, gPluginObjectCount);
}

CPluginStreamListener::~CPluginStreamListener()
{
  gPluginObjectCount--;
  dbgOut2("CPluginStreamListener::~CPluginStreamListener, gPluginObjectCount = %lu", gPluginObjectCount);
}

NS_IMPL_ISUPPORTS1(CPluginStreamListener, nsIPluginStreamListener)

NS_METHOD CPluginStreamListener::OnStartBinding(nsIPluginStreamInfo* streamInfo)
{
  dbgOut1("CPluginStreamListener::OnStartBinding");
  return NS_OK;
}

NS_METHOD CPluginStreamListener::OnDataAvailable(nsIPluginStreamInfo* streamInfo, 
                                                 nsIInputStream* inputStream, 
                                                 PRUint32 length)
{
  dbgOut1("CPluginStreamListener::OnDataAvailable");

  char* buffer = new char[length];

  if(buffer == nsnull) 
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint32 amountRead = 0;

  nsresult rv = inputStream->Read(buffer, length, &amountRead);

  if(rv == NS_OK) 
  {
    dbgOut2("\t\tReceived %lu bytes", length);
  }

  delete buffer;

  return rv;
}

NS_METHOD CPluginStreamListener::OnFileAvailable(nsIPluginStreamInfo* streamInfo, const char* fileName)
{
  dbgOut1("CPluginStreamListener::OnFileAvailable");

  return NS_OK;
}

NS_METHOD CPluginStreamListener::OnStopBinding(nsIPluginStreamInfo* streamInfo, nsresult status)
{
  dbgOut1("CPluginStreamListener::OnStopBinding");

  return NS_OK;
}

NS_METHOD CPluginStreamListener::OnNotify(const char* url, nsresult status)
{
  dbgOut1("CPluginStreamListener::OnNotify");

  return NS_OK;
}

NS_METHOD CPluginStreamListener::GetStreamType(nsPluginStreamType *result)
{
  dbgOut1("CPluginStreamListener::GetStreamType");

  *result = nsPluginStreamType_Normal;
  return NS_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 */

#ifndef _LISTENER_H__
#define _LISTENER_H__

#include "plugin.h"
#include "instbase.h"

class CPluginStreamListener : public nsIPluginStreamListener 
{
public:

NS_DECL_ISUPPORTS

  //This method is called at the beginning of a URL load
  NS_IMETHOD OnStartBinding(nsIPluginStreamInfo* streamInfo);

  //This method is called when data is available in the input stream.
  NS_IMETHOD OnDataAvailable(nsIPluginStreamInfo* streamInfo, nsIInputStream* inputStream, PRUint32 length);

  NS_IMETHOD OnFileAvailable(nsIPluginStreamInfo* streamInfo, const char* fileName);

  //This method is called when a URL has finished loading.
  NS_IMETHOD OnStopBinding(nsIPluginStreamInfo* streamInfo, nsresult status);

  NS_IMETHOD OnNotify(const char* url, nsresult status);

  NS_IMETHOD GetStreamType(nsPluginStreamType *result);

  CPluginStreamListener(CPluginInstance* inst, const char* url);
  virtual ~CPluginStreamListener();

protected:
  const char* fMessageName;
};

#endif // _LISTENER_H__

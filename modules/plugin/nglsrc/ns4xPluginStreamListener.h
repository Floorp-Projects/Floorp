/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef ns4xPluginStreamListener_h__
#define ns4xPluginStreamListener_h__

#include "nsIPluginStreamListener2.h"
#include "nsIPluginStreamInfo.h"

#define MAX_PLUGIN_NECKO_BUFFER 16384

class ns4xPluginStreamListener : public nsIPluginStreamListener2 
{
public:
  NS_DECL_ISUPPORTS

  // from nsIPluginStreamListener:
  NS_IMETHOD OnStartBinding(nsIPluginStreamInfo* pluginInfo);
  NS_IMETHOD OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length, PRUint32 offset);
  NS_IMETHOD OnFileAvailable( nsIPluginStreamInfo* pluginInfo, const char* fileName);
  NS_IMETHOD OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status);
  NS_IMETHOD GetStreamType(nsPluginStreamType *result);

  NS_IMETHOD OnDataAvailable(nsIPluginStreamInfo* pluginInfo,
                                          nsIInputStream* input,
                                          PRUint32 length);

  // ns4xPluginStreamListener specific methods:
  ns4xPluginStreamListener(nsIPluginInstance* inst, void* notifyData);
  virtual ~ns4xPluginStreamListener();
  PRBool IsStarted();
  nsresult CleanUpStream();

protected:
  void* mNotifyData;
  char* mStreamBuffer;
  ns4xPluginInstance* mInst;
  NPStream mNPStream;
  PRUint32 mPosition;
  PRUint32 mCurrentStreamOffset;
  nsPluginStreamType mStreamType;
  PRBool mStreamStarted;
  PRBool mStreamCleanedUp;

public:
  nsIPluginStreamInfo * mStreamInfo;
};

#endif

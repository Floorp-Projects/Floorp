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

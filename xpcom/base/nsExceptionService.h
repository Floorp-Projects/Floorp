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
 * ActiveState Tool Corp..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Contributor(s): Mark Hammond <MarkH@ActiveState.com>
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

#ifndef nsExceptionService_h__
#define nsExceptionService_h__

#include "nsVoidArray.h"
#include "nsIException.h"
#include "nsIExceptionService.h"
#include "nsIObserverService.h"
#include "nsHashtable.h"

class nsExceptionManager;

/** Exception Service definition **/
class nsExceptionService : public nsIExceptionService, public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTIONSERVICE
  NS_DECL_NSIEXCEPTIONMANAGER
  NS_DECL_NSIOBSERVER

  nsExceptionService();
  virtual ~nsExceptionService();

  /* additional members */
  nsresult DoGetExceptionFromProvider( nsresult errCode, nsIException **_richError );
  void Shutdown();


  /* thread management and cleanup */
  static void AddThread(nsExceptionManager *);
  static void DropThread(nsExceptionManager *);
  static void DoDropThread(nsExceptionManager *thread);

  static void DropAllThreads();
  static nsExceptionManager *firstThread;

  nsSupportsHashtable mProviders;

  /* single lock protects both providers hashtable
     and thread list */
  static PRLock* lock;

  static PRUintn tlsIndex;
  static void PR_CALLBACK ThreadDestruct( void *data );
#ifdef NS_DEBUG
  static PRInt32 totalInstances;
#endif
};


#endif // nsExceptionService_h__

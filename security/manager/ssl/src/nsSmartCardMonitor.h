/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Contributed by Red Hat Inc.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef _NSSMARTCARDMONITOR_
#define _NSSMARTCARDMONITOR_

#include "prthread.h"
#include "secmod.h"
#include "plhash.h"
#include "pkcs11t.h"

class SmartCardThreadEntry;
class SmartCardMonitoringThread;

//
// manage a group of SmartCardMonitoringThreads
//
class SmartCardThreadList {
public:
  SmartCardThreadList();
  ~SmartCardThreadList();
  void Remove(SECMODModule *module);
  nsresult Add(SmartCardMonitoringThread *thread);
private:
  SmartCardThreadEntry *head;
};

//
// monitor a Module for token insertion and removal
//
// NOTE: this provides the application the ability to dynamically add slots
// on the fly as necessary.
//
class SmartCardMonitoringThread
{
 public:
  SmartCardMonitoringThread(SECMODModule *module);
  ~SmartCardMonitoringThread();
  
  nsresult Start();
  void Stop();
  
  void Execute();
  void Interrupt();
  
  const SECMODModule *GetModule();

 private:

  static void LaunchExecute(void *arg);
  void SetTokenName(CK_SLOT_ID slotid, const char *tokenName, PRUint32 series);
  const char *GetTokenName(CK_SLOT_ID slotid);
  PRUint32 GetTokenSeries(CK_SLOT_ID slotid);
  nsresult SendEvent(const nsAString &type,const char *tokenName);
  
  
  SECMODModule *mModule;
  PLHashTable *mHash;
  PRThread* mThread;
};

#endif

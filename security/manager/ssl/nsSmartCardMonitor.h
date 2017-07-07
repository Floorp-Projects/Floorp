/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSmartCardMonitor_h
#define nsSmartCardMonitor_h

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
  void Remove(SECMODModule* module);
  nsresult Add(SmartCardMonitoringThread* thread);

private:
  SmartCardThreadEntry* head;
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
  explicit SmartCardMonitoringThread(SECMODModule* module);
  ~SmartCardMonitoringThread();

  nsresult Start();
  void Stop();

  void Execute();
  void Interrupt();

  const SECMODModule* GetModule();

 private:
  static void LaunchExecute(void* arg);
  void SetTokenName(CK_SLOT_ID slotid, const char* tokenName, uint32_t series);
  const char* GetTokenName(CK_SLOT_ID slotid);
  uint32_t GetTokenSeries(CK_SLOT_ID slotid);
  void SendEvent(const char* type, const char* tokenName);

  SECMODModule* mModule;
  PLHashTable* mHash;
  PRThread* mThread;
};

#endif // nsSmartCardMonitor_h

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSmartCardMonitor.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "GeckoProfiler.h"
#include "nspr.h"
#include "pk11func.h"

using namespace mozilla;

//
// The SmartCard monitoring thread should start up for each module we load
// that has removable tokens. This code calls an NSS function which waits
// until there is a change in the token state. NSS uses the
// C_WaitForSlotEvent() call in PKCS #11 if the module implements the call,
// otherwise NSS will poll the token in a loop with a delay of 'latency'
// between polls. Note that the C_WaitForSlotEvent() may wake up on any type
// of token event, so it's necessary to filter these events down to just the
// insertion and removal events we are looking for.
//
// Once the event is found, it is dispatched to the main thread to notify
// any window where window.crypto.enableSmartCardEvents is true.
// Additionally, all observers of the topics |kSmartcardInsert| and
// |kSmartcardRemove| are notified by the observer service of the appropriate
// event.
//

#define kSmartcardInsert "smartcard-insert"
#define kSmartcardRemove "smartcard-remove"

class nsTokenEventRunnable : public nsIRunnable {
public:
  nsTokenEventRunnable(const char* aType, const nsAString& aTokenName)
    : mType(aType)
    , mTokenName(aTokenName)
  {
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

private:
  virtual ~nsTokenEventRunnable() {}

  const char* mType;
  nsString mTokenName;
};

NS_IMPL_ISUPPORTS(nsTokenEventRunnable, nsIRunnable)

NS_IMETHODIMP
nsTokenEventRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }
  return observerService->NotifyObservers(nullptr, mType, mTokenName.get());
}

// self linking and removing double linked entry
// adopts the thread it is passed.
class SmartCardThreadEntry
{
public:
  friend class SmartCardThreadList;
  SmartCardThreadEntry(SmartCardMonitoringThread *thread,
                       SmartCardThreadEntry *next,
                       SmartCardThreadEntry *prev,
                       SmartCardThreadEntry **head)
    : next(next)
    , prev(prev)
    , head(head)
    , thread(thread)
  {
    if (prev) {
      prev->next = this;
    } else {
      *head = this;
    }
    if (next) {
      next->prev = this;
    }
  }

  ~SmartCardThreadEntry()
  {
    if (prev) {
      prev->next = next;
    } else {
      *head = next;
    }
    if (next) {
      next->prev = prev;
    }
    // NOTE: automatically stops the thread
    delete thread;
  }

private:
  SmartCardThreadEntry *next;
  SmartCardThreadEntry *prev;
  SmartCardThreadEntry **head;
  SmartCardMonitoringThread *thread;
};

//
// SmartCardThreadList is a class to help manage the running threads.
// That way new threads could be started and old ones terminated as we
// load and unload modules.
//
SmartCardThreadList::SmartCardThreadList() : head(0)
{
}

SmartCardThreadList::~SmartCardThreadList()
{
  // the head is self linking and unlinking, the following
  // loop removes all entries on the list.
  // it will also stop the thread if it happens to be running
  while (head) {
    delete head;
  }
}

void
SmartCardThreadList::Remove(SECMODModule *aModule)
{
  for (SmartCardThreadEntry* current = head; current;
       current = current->next) {
    if (current->thread->GetModule() == aModule) {
      // NOTE: automatically stops the thread and dequeues it from the list
      delete current;
      return;
    }
  }
}

// adopts the thread passed to it. Starts the thread as well
nsresult
SmartCardThreadList::Add(SmartCardMonitoringThread* thread)
{
  SmartCardThreadEntry* current = new SmartCardThreadEntry(thread, head,
                                                           nullptr, &head);
  // OK to forget current here, it's on the list.
  Unused << current;

  return thread->Start();
}


// We really should have a Unity PL Hash function...
static PLHashNumber
unity(const void* key) { return PLHashNumber(NS_PTR_TO_INT32(key)); }

SmartCardMonitoringThread::SmartCardMonitoringThread(SECMODModule* module_)
  : mThread(nullptr)
{
  mModule = SECMOD_ReferenceModule(module_);
  // simple hash functions, most modules have less than 3 slots, so 10 buckets
  // should be plenty
  mHash = PL_NewHashTable(10, unity, PL_CompareValues, PL_CompareStrings,
                          nullptr, 0);
}

//
// when we shutdown the thread, be sure to stop it first. If not, it just might
// crash when the mModule it is looking at disappears.
//
SmartCardMonitoringThread::~SmartCardMonitoringThread()
{
  Stop();
  SECMOD_DestroyModule(mModule);
  if (mHash) {
    PL_HashTableDestroy(mHash);
  }
}

nsresult
SmartCardMonitoringThread::Start()
{
  if (!mThread) {
    mThread = PR_CreateThread(PR_SYSTEM_THREAD, LaunchExecute, this,
                              PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                              PR_JOINABLE_THREAD, 0);
  }
  return mThread ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

//
// Should only stop if we are through with the module.
// CancelWait has the side effect of losing all the keys and
// current operations on the module!. (See the comment in
// SECMOD_CancelWait for why this is so..).
//
void SmartCardMonitoringThread::Stop()
{
  SECStatus rv;

  rv = SECMOD_CancelWait(mModule);
  if (rv != SECSuccess) {
    // we didn't wake up the Wait, so don't try to join the thread
    // otherwise we will hang forever...
    return;
  }

  // confused about the memory model here? NSPR owns the memory for
  // threads. non-joinable threads are freed when the thread dies.
  // joinable threads are freed after the call to PR_JoinThread.
  // That means if SECMOD_CancelWait fails, we'll leak the mThread
  // structure. this is considered preferable to hanging (which is
  // what will happen if we try to join a thread that blocked).
  if (mThread) {
    PR_JoinThread(mThread);
    mThread = 0;
  }
}

//
// remember the name and series of a token in a particular slot.
// This is important because the name is no longer available when
// the token is removed. If listeners depended on this information,
// They would be out of luck. It also is a handy way of making sure
// we don't generate spurious insertion and removal events as the slot
// cycles through various states.
//
void
SmartCardMonitoringThread::SetTokenName(CK_SLOT_ID slotid,
                                       const char* tokenName, uint32_t series)
{
  if (mHash) {
    if (tokenName) {
      int len = strlen(tokenName) + 1;
      /* this must match the allocator used in
       * PLHashAllocOps.freeEntry DefaultFreeEntry */
      char* entry = (char*)PR_Malloc(len + sizeof(uint32_t));

      if (entry) {
        memcpy(entry, &series, sizeof(uint32_t));
        memcpy(&entry[sizeof(uint32_t)], tokenName, len);

        PL_HashTableAdd(mHash, (void*)(uintptr_t)slotid, entry); /* adopt */
        return;
      }
    } else {
      // if tokenName was not provided, remove the old one (implicit delete)
      PL_HashTableRemove(mHash, (void*)(uintptr_t)slotid);
    }
  }
}

// retrieve the name saved above
const char*
SmartCardMonitoringThread::GetTokenName(CK_SLOT_ID slotid)
{
  const char* tokenName = nullptr;
  const char* entry;

  if (mHash) {
    entry = (const char*)PL_HashTableLookupConst(mHash,
                                                 (void*)(uintptr_t)slotid);
    if (entry) {
      tokenName = &entry[sizeof(uint32_t)];
    }
  }
  return tokenName;
}

// retrieve the series saved in SetTokenName above
uint32_t
SmartCardMonitoringThread::GetTokenSeries(CK_SLOT_ID slotid)
{
  uint32_t series = 0;
  const char* entry;

  if (mHash) {
    entry = (const char*)PL_HashTableLookupConst(mHash,
                                                 (void*)(uintptr_t)slotid);
    if (entry) {
      memcpy(&series, entry, sizeof(uint32_t));
    }
  }
  return series;
}

//
// helper function to pass the event off to nsNSSComponent.
//
void
SmartCardMonitoringThread::SendEvent(const char* eventType,
                                     const char* tokenName)
{
  // The token name should be UTF8, but it's not clear that this is enforced
  // by NSS. To be safe, we explicitly check here before converting it to
  // UTF16. If it isn't UTF8, we just use an empty string with the idea that
  // consumers of these events should at least be notified that something
  // happened.
  nsAutoString tokenNameUTF16(NS_LITERAL_STRING(""));
  if (IsUTF8(nsDependentCString(tokenName))) {
    tokenNameUTF16.Assign(NS_ConvertUTF8toUTF16(tokenName));
  }
  nsCOMPtr<nsIRunnable> runnable(new nsTokenEventRunnable(eventType,
                                                          tokenNameUTF16));
  NS_DispatchToMainThread(runnable);
}

//
// This is the main loop.
//
void SmartCardMonitoringThread::Execute()
{
  const char* tokenName;

  //
  // populate token names for already inserted tokens.
  //
  PK11SlotList* sl = PK11_FindSlotsByNames(mModule->dllName, nullptr, nullptr,
                                           true);

  PK11SlotListElement* sle;
  if (sl) {
    for (sle = PK11_GetFirstSafe(sl); sle;
         sle = PK11_GetNextSafe(sl, sle, false)) {
      SetTokenName(PK11_GetSlotID(sle->slot), PK11_GetTokenName(sle->slot),
                   PK11_GetSlotSeries(sle->slot));
    }
    PK11_FreeSlotList(sl);
  }

  // loop starts..
  do {
    UniquePK11SlotInfo slot(
      SECMOD_WaitForAnyTokenEvent(mModule, 0, PR_SecondsToInterval(1)));
    if (!slot) {
      break;
    }

    // now we have a potential insertion or removal event, see if the slot
    // is present to determine which it is...
    if (PK11_IsPresent(slot.get())) {
      // insertion
      CK_SLOT_ID slotID = PK11_GetSlotID(slot.get());
      uint32_t series = PK11_GetSlotSeries(slot.get());

      // skip spurious insertion events...
      if (series != GetTokenSeries(slotID)) {
        // if there's a token name, then we have not yet issued a remove
        // event for the previous token, do so now...
        tokenName = GetTokenName(slotID);
        if (tokenName) {
          SendEvent(kSmartcardRemove, tokenName);
        }
        tokenName = PK11_GetTokenName(slot.get());
        // save the token name and series
        SetTokenName(slotID, tokenName, series);
        SendEvent(kSmartcardInsert, tokenName);
      }
    } else {
      // retrieve token name
      CK_SLOT_ID slotID = PK11_GetSlotID(slot.get());
      tokenName = GetTokenName(slotID);
      // if there's not a token name, then the software isn't expecting
      // a (or another) remove event.
      if (tokenName) {
        SendEvent(kSmartcardRemove, tokenName);
        // clear the token name (after we send it)
        SetTokenName(slotID, nullptr, 0);
      }
    }
  } while (1);
}

// accessor to help searching active Monitoring threads
const SECMODModule* SmartCardMonitoringThread::GetModule()
{
  return mModule;
}

// C-like calling sequence to glue into PR_CreateThread.
void SmartCardMonitoringThread::LaunchExecute(void* arg)
{
  AutoProfilerRegisterThread registerThread("SmartCard");
  NS_SetCurrentThreadName("SmartCard");

  ((SmartCardMonitoringThread*)arg)->Execute();
}

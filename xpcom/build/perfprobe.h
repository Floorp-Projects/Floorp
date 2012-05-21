/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A mechanism for interacting with operating system-provided
 * debugging/profiling tools such as Microsoft EWT/Windows Performance Toolkit.
 */

#ifndef mozilla_perfprobe_h
#define mozilla_perfprobe_h

#if !defined(XP_WIN)
#error "For the moment, perfprobe.h is defined only for Windows platforms"
#endif

#include "nsError.h"
#include "nsStringGlue.h"
#include "prlog.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include <windows.h>
#undef GetStartupInfo //Prevent Windows from polluting global namespace
#include <wmistr.h>
#include <evntrace.h>

namespace mozilla {
namespace probes {

class ProbeManager;

/**
 * A data structure supporting a trigger operation that can be used to
 * send information to the operating system.
 */

class Probe
{
public:
  NS_INLINE_DECL_REFCOUNTING(Probe)

  /**
   * Trigger the event.
   *
   * Note: Can be called from any thread.
   */
  nsresult Trigger();
  ~Probe() {};

protected:
  Probe(const nsCID &aGUID,
        const nsACString &aName,
         ProbeManager *aManager);
  friend class ProbeManager;

protected:

  /**
   * The system GUID associated to this probe. See the documentation
   * of |ProbeManager::Make| for more details.
   */
  const GUID mGUID;

  /**
   * The name of this probe. See the documentation
   * of |ProbeManager::Make| for more details.
   */
  const nsCString mName;

  /**
   * The ProbeManager managing this probe.
   *
   * Note: This is a weak reference to avoid a useless cycle.
   */
  class ProbeManager *mManager;
};


/**
 * A manager for a group of probes.
 *
 * You can have several managers in one application, provided that they all
 * have distinct IDs and names. However, having more than 2 is considered a bad
 * practice.
 */
class ProbeManager
{
public:
  NS_INLINE_DECL_REFCOUNTING(ProbeManager)

  /**
   * Create a new probe manager.
   *
   * This constructor should be called from the main thread.
   *
   * @param uid The unique ID of the probe. Under Windows, this unique
   * ID must have been previously registered using an external tool.
   * See MyCategory on http://msdn.microsoft.com/en-us/library/aa364100.aspx
   * @param name A name for the probe. Currently used only for logging purposes.
   * In the future, may be attached to the data sent to the operating system.
   *
   * Note: If two ProbeManagers are constructed with the same uid and/or name,
   * behavior is unspecified.
   */
  ProbeManager(const nsCID &applicationUID,
               const nsACString &applicationName);

  /**
   * Acquire a probe.
   *
   * Note: Only probes acquired before the call to SetReady are taken into
   * account
   * Note: Can be called only from the main thread.
   *
   * @param eventUID The unique ID of the probe. Under Windows, this unique
   * ID must have been previously registered using an external tool.
   * See MyCategory on http://msdn.microsoft.com/en-us/library/aa364100.aspx
   * @param eventMame A name for the probe. Currently used only for logging
   * purposes. In the
   * future, may be attached to the data sent to the operating system.
   * @return Either |null| in case of error or a valid |Probe*|.
   *
   * Note: If this method is called twice with the same uid and/or name,
   * behavior is undefined.
   */
  already_AddRefed<Probe> GetProbe(const nsCID &eventUID,
                                   const nsACString &eventName);

  /**
   * Start/stop the measuring session.
   *
   * This method should be called from the main thread.
   *
   * Note that starting an already started probe manager has no effect,
   * nor does stopping an already stopped probe manager.
   */
  nsresult StartSession();
  nsresult StopSession();

  /**
   * @return true If measures are currently on, i.e. if triggering probes is any
   * is useful. You do not have to check this before triggering a probe, unless
   * this can avoid complex computations.
   */
  bool IsActive();

  ~ProbeManager();

protected:
  nsresult StartSession(nsTArray<nsRefPtr<Probe> > &probes);
  nsresult Init(const nsCID &applicationUID, const nsACString &applicationName);

protected:
  /**
   * `true` if a session is in activity, `false` otherwise.
   */
  bool mIsActive;

  /**
   * The UID of this manager.
   * See documentation above for registration steps that you
   * may have to take.
   */
  nsCID mApplicationUID;

  /**
   * The name of the application.
   */
  nsCString mApplicationName;

  /**
   * All the probes that have been created for this manager.
   */
  nsTArray<nsRefPtr<Probe> > mAllProbes;

  /**
   * Handle used for triggering events
   */
  TRACEHANDLE mSessionHandle;

  /**
   * Handle used for registration/unregistration
   */
  TRACEHANDLE mRegistrationHandle;

  /**
   * `true` if initialization has been performed, `false` until then.
   */
  bool mInitialized;

  friend class Probe;//Needs to access |mSessionHandle|
  friend ULONG WINAPI ControlCallback(
                                      WMIDPREQUESTCODE RequestCode,
                                      PVOID Context,
                                      ULONG *Reserved,
                                      PVOID Buffer
                                      );//Sets |mSessionHandle|
};
}
}

#endif //mozilla_perfprobe_h

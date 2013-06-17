/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SQLITEINTERPOSER_H_
#define SQLITEINTERPOSER_H_

#ifdef MOZ_ENABLE_PROFILER_SPS

#include "IOInterposer.h"
#include "mozilla/Atomics.h"

namespace mozilla {

/**
 * This class provides main thread I/O interposing for SQLite reads, writes,
 * and fsyncs. Most methods must be called from the main thread, except for:
 *   Enable
 *   OnRead
 *   OnWrite
 *   OnFSync
 */
class SQLiteInterposer MOZ_FINAL : public IOInterposerModule
{
public:
  static IOInterposerModule* GetInstance(IOInterposeObserver* aObserver, 
      IOInterposeObserver::Operation aOpsToInterpose);

  /**
   * This function must be called from the main thread, and furthermore
   * it must be called when no other threads are executing in OnRead, OnWrite,
   * or OnFSync. Effectivly this restricts us to calling it only after all other
   * threads have been stopped during shutdown.
   */
  static void ClearInstance();

  ~SQLiteInterposer();
  void Enable(bool aEnable);

  typedef void (*EventHandlerFn)(double&);
  static void OnRead(double& aDuration);
  static void OnWrite(double& aDuration);
  static void OnFSync(double& aDuration);

private:
  SQLiteInterposer();
  bool Init(IOInterposeObserver* aObserver,
            IOInterposeObserver::Operation aOpsToInterpose);

  IOInterposeObserver*            mObserver;
  IOInterposeObserver::Operation  mOps;
  Atomic<uint32_t,ReleaseAcquire> mEnabled;
};

} // namespace mozilla

#else // MOZ_ENABLE_PROFILER_SPS

namespace mozilla {

class SQLiteInterposer MOZ_FINAL
{
public:
  typedef void (*EventHandlerFn)(double&);
  static inline void OnRead(double& aDuration) {}
  static inline void OnWrite(double& aDuration) {}
  static inline void OnFSync(double& aDuration) {}
};

} // namespace mozilla

#endif // MOZ_ENABLE_PROFILER_SPS

#endif // SQLITEINTERPOSER_H_

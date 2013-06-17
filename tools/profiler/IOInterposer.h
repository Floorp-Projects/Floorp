/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IOINTERPOSER_H_
#define IOINTERPOSER_H_

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/XPCOM.h"

namespace mozilla {

/**
 * Interface for I/O interposer observers. This is separate from
 * IOInterposer itself because in the future we might use the I/O
 * Interposer for shutdown write poisioning.
 */
class IOInterposeObserver
{
public:
  enum Operation
  {
    OpNone = 0,
    OpRead = (1 << 0),
    OpWrite = (1 << 1),
    OpFSync = (1 << 2),
    OpWriteFSync = (OpWrite | OpFSync),
    OpAll = (OpRead | OpWrite | OpFSync)
  };
  virtual void Observe(Operation aOp, double& aDuration,
                       const char* aModuleInfo) = 0;
};

/**
 * Interface for I/O Interposer modules that actually generate the I/O
 * Interposer events. Some concrete examples include NSPR, SQLite, ETW
 */
class IOInterposerModule
{
public:
  virtual ~IOInterposerModule() {}
  virtual void Enable(bool aEnable) = 0;

protected:
  IOInterposerModule() {}

private:
  IOInterposerModule(const IOInterposerModule&);
  IOInterposerModule& operator=(const IOInterposerModule&);
};

/**
 * This class is reponsible for managing the state of I/O interposer
 * modules and ensuring that events are routed to the appropriate
 * observers. All methods except for Enable should be called from the
 * main thread.
 */
class IOInterposer MOZ_FINAL : public IOInterposeObserver
{
public:
  ~IOInterposer();

  /**
   * It is safe to call this method off of the main thread.
   *
   * @param aEnable true to enable all modules, false to disable all modules
   */
  void Enable(bool aEnable);

  void Register(IOInterposeObserver::Operation aOp,
                IOInterposeObserver* aObserver);
  void Deregister(IOInterposeObserver::Operation aOp,
                  IOInterposeObserver* aObserver);
  void Observe(IOInterposeObserver::Operation aOp, double& aDuration,
               const char* aModuleInfo);

  static IOInterposer* GetInstance();

  /**
   * This function must be called from the main thread, and furthermore
   * it must be called when no other threads are executing. Effectivly this
   * restricts us to calling it only during shutdown.
   */
  static void ClearInstance();

private:
  IOInterposer();
  bool Init();
  IOInterposer(const IOInterposer&);
  IOInterposer& operator=(const IOInterposer&);

  // mMutex guards access to mModules, particularly to provide
  // mutual exclusion during Enable()
  mozilla::Mutex  mMutex;
  nsTArray<IOInterposerModule*>   mModules;
  nsTArray<IOInterposeObserver*>  mReadObservers;
  nsTArray<IOInterposeObserver*>  mWriteObservers;
  nsTArray<IOInterposeObserver*>  mFSyncObservers;
};

} // namespace mozilla

#endif // IOINTERPOSER_H_


#ifndef CALL_SHARED_MODULE_THREAD_H_
#define CALL_SHARED_MODULE_THREAD_H_

#include "modules/utility/include/process_thread.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// A restricted way to share the module process thread across multiple instances
// of Call that are constructed on the same worker thread (which is what the
// peer connection factory guarantees).
// SharedModuleThread supports a callback that is issued when only one reference
// remains, which is used to indicate to the original owner that the thread may
// be discarded.
class SharedModuleThread final {
 public:
  // Allows injection of an externally created process thread.
  static rtc::scoped_refptr<SharedModuleThread> Create(
      std::unique_ptr<ProcessThread> process_thread,
      std::function<void()> on_one_ref_remaining);

  void EnsureStarted();

  ProcessThread* process_thread();

 private:
  friend class rtc::scoped_refptr<SharedModuleThread>;
  SharedModuleThread(std::unique_ptr<ProcessThread> process_thread,
                     std::function<void()> on_one_ref_remaining);
  ~SharedModuleThread();

  void AddRef() const;
  rtc::RefCountReleaseStatus Release() const;

  class Impl;
  mutable std::unique_ptr<Impl> impl_;
};

}  // namespace webrtc

#endif  // CALL_SHARED_MODULE_THREAD_H_

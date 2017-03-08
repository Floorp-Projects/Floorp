#ifndef nricectxhandler_h__
#define nricectxhandler_h__

#include "nricectx.h"

namespace mozilla {

class NrIceCtxHandler {
public:
  // TODO(ekr@rtfm.com): Too many bools here. Bug 1193437.
  static RefPtr<NrIceCtxHandler> Create(const std::string& name,
                                        bool allow_loopback = false,
                                        bool tcp_enabled = true,
                                        bool allow_link_local = false,
                                        NrIceCtx::Policy policy =
                                          NrIceCtx::ICE_POLICY_ALL);

  RefPtr<NrIceMediaStream> CreateStream(const std::string& name,
                                        int components);
  // CreateCtx is necessary so we can create and initialize the context
  // on main thread, but begin the ice restart mechanics on STS thread
  RefPtr<NrIceCtx> CreateCtx() const; // for test
  RefPtr<NrIceCtx> CreateCtx(const std::string& ufrag,
                             const std::string& pwd) const;

  RefPtr<NrIceCtx> ctx() { return current_ctx; }

  bool BeginIceRestart(RefPtr<NrIceCtx> new_ctx);
  bool IsRestarting() const { return (old_ctx != nullptr); }
  void FinalizeIceRestart();
  void RollbackIceRestart();

  NrIceStats Destroy();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NrIceCtxHandler)

private:
  NrIceCtxHandler(const std::string& name, NrIceCtx::Policy policy);
  NrIceCtxHandler() = delete;
  ~NrIceCtxHandler();
  DISALLOW_COPY_ASSIGN(NrIceCtxHandler);

  RefPtr<NrIceCtx> current_ctx;
  RefPtr<NrIceCtx> old_ctx; // for while restart is in progress
  int restart_count; // used to differentiate streams between restarted ctx
};

} // close namespace

#endif // nricectxhandler_h__

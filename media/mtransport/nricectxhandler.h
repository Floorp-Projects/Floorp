#ifndef nricectxhandler_h__
#define nricectxhandler_h__

#include "nricectx.h"

namespace mozilla {

class NrIceCtxHandler : public NrIceCtx {
public:
  // TODO(ekr@rtfm.com): Too many bools here. Bug 1193437.
  static RefPtr<NrIceCtxHandler> Create(const std::string& name,
                                 bool offerer,
                                 bool allow_loopback = false,
                                 bool tcp_enabled = true,
                                 bool allow_link_local = false,
                                 bool hide_non_default = false,
                                 Policy policy = ICE_POLICY_ALL);

  // Create a media stream
  RefPtr<NrIceMediaStream> CreateStream(const std::string& name,
                                        int components);

protected:
  NrIceCtxHandler(const std::string& name,
                  bool offerer,
                  Policy policy)
  : NrIceCtx(name, offerer, policy)
    {}

  NrIceCtxHandler(); // disable
  virtual ~NrIceCtxHandler();
};

} // close namespace

#endif // nricectxhandler_h__

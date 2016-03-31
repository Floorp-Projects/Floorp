#include <sstream>

// nICEr includes
extern "C" {
#include "nr_api.h"
#include "ice_ctx.h"
}

// Local includes
#include "nricectxhandler.h"
#include "nricemediastream.h"

namespace mozilla {

NrIceCtxHandler::~NrIceCtxHandler()
{
}


RefPtr<NrIceCtxHandler>
NrIceCtxHandler::Create(const std::string& name,
                        bool offerer,
                        bool allow_loopback,
                        bool tcp_enabled,
                        bool allow_link_local,
                        bool hide_non_default,
                        Policy policy)
{
  // InitializeCryptoAndLogging only executes once
  NrIceCtx::InitializeCryptoAndLogging(allow_loopback,
                                       tcp_enabled,
                                       allow_link_local);

  RefPtr<NrIceCtxHandler> ctx = new NrIceCtxHandler(name, offerer, policy);

  if (!NrIceCtx::Initialize(ctx,
                            hide_non_default)) {
    return nullptr;
  }

  return ctx;
}


RefPtr<NrIceMediaStream>
NrIceCtxHandler::CreateStream(const std::string& name, int components)
{
  return NrIceMediaStream::Create(this, name, components);
}


} // close namespace

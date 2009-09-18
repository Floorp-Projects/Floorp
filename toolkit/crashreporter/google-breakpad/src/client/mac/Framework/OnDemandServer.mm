// Copyright (c) 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#import "OnDemandServer.h"

#if DEBUG
  #define PRINT_MACH_RESULT(result_, message_) \
    printf(message_"%s (%d)\n", mach_error_string(result_), result_ );
#else
  #define PRINT_MACH_RESULT(result_, message_)
#endif

//==============================================================================
OnDemandServer *OnDemandServer::Create(const char *server_command,
                                       const char *service_name,
                                       bool unregister_on_cleanup,
                                       kern_return_t *out_result) {
  OnDemandServer *server = new OnDemandServer();

  if (!server) return NULL;

  kern_return_t result = server->Initialize(server_command,
                                            service_name,
                                            unregister_on_cleanup);

  if (out_result) {
    *out_result = result;
  }

  if (result == KERN_SUCCESS) {
    return server;
  }

  delete server;
  return NULL;
};

//==============================================================================
kern_return_t OnDemandServer::Initialize(const char *server_command,
                                         const char *service_name,
                                         bool unregister_on_cleanup) {
  unregister_on_cleanup_ = unregister_on_cleanup;

  kern_return_t kr =
    bootstrap_create_server(bootstrap_port,
                            const_cast<char*>(server_command),
                            geteuid(),       // server uid
                            true,
                            &server_port_);

  if (kr != KERN_SUCCESS) {
    PRINT_MACH_RESULT(kr, "bootstrap_create_server() : ");
    return kr;
  }

  strlcpy(service_name_, service_name, sizeof(service_name_));

  // Create a service called service_name, and return send rights to
  // that port in service_port_.
  kr = bootstrap_create_service(server_port_,
                                const_cast<char*>(service_name),
                                &service_port_);

  if (kr != KERN_SUCCESS) {
    PRINT_MACH_RESULT(kr, "bootstrap_create_service() : ");

    // perhaps the service has already been created - try to look it up
    kr = bootstrap_look_up(bootstrap_port, (char*)service_name, &service_port_);

    if (kr != KERN_SUCCESS) {
      PRINT_MACH_RESULT(kr, "bootstrap_look_up() : ");
      Unregister();  // clean up server port
      return kr;
    }
  }

  return KERN_SUCCESS;
}

//==============================================================================
OnDemandServer::~OnDemandServer() {
  if (unregister_on_cleanup_) {
    Unregister();
  }
}

//==============================================================================
void OnDemandServer::LaunchOnDemand() {
  // We need to do this, since the launched server is another process
  // and holding on to this port delays launching until the current process
  // exits!
  mach_port_deallocate(mach_task_self(), server_port_);
  server_port_ = NULL;

  // Now, the service is still registered and all we need to do is send
  // a mach message to the service port in order to launch the server.
}

//==============================================================================
void OnDemandServer::Unregister() {
  if (service_port_ != MACH_PORT_NULL) {
    mach_port_deallocate(mach_task_self(), service_port_);
    service_port_ = MACH_PORT_NULL;
  }

  if (server_port_ != MACH_PORT_NULL) {
    // unregister the service
    kern_return_t kr = bootstrap_register(server_port_,
                                          service_name_,
                                          MACH_PORT_NULL);

    if (kr != KERN_SUCCESS) {
      PRINT_MACH_RESULT(kr, "Breakpad UNREGISTER : bootstrap_register() : ");
    }

    mach_port_deallocate(mach_task_self(), server_port_);
    server_port_ = MACH_PORT_NULL;
  }
}

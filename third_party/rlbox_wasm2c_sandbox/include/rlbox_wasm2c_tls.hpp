#pragma once

#include <stdint.h>

namespace rlbox {

class rlbox_wasm2c_sandbox;

struct rlbox_wasm2c_sandbox_thread_data
{
  rlbox_wasm2c_sandbox* sandbox;
  uint32_t last_callback_invoked;
};

#ifdef RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES

rlbox_wasm2c_sandbox_thread_data* get_rlbox_wasm2c_sandbox_thread_data();

#  define RLBOX_WASM2C_SANDBOX_STATIC_VARIABLES()                              \
    thread_local rlbox::rlbox_wasm2c_sandbox_thread_data                       \
      rlbox_wasm2c_sandbox_thread_info{ 0, 0 };                                \
                                                                               \
    namespace rlbox {                                                          \
      rlbox_wasm2c_sandbox_thread_data* get_rlbox_wasm2c_sandbox_thread_data() \
      {                                                                        \
        return &rlbox_wasm2c_sandbox_thread_info;                              \
      }                                                                        \
    }                                                                          \
    static_assert(true, "Enforce semi-colon")

#endif

} // namespace rlbox

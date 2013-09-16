/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#if !defined(CUBEB_INTERNAL_0eb56756_4e20_4404_a76d_42bf88cd15a5)
#define CUBEB_INTERNAL_0eb56756_4e20_4404_a76d_42bf88cd15a5

#include "cubeb/cubeb.h"

struct cubeb_ops {
  int (* init)(cubeb ** context, char const * context_name);
  char const * (* get_backend_id)(cubeb * context);
  int (* get_max_channel_count)(cubeb * context, uint32_t * max_channels);
  void (* destroy)(cubeb * context);
  int (* stream_init)(cubeb * context, cubeb_stream ** stream, char const * stream_name,
                      cubeb_stream_params stream_params, unsigned int latency,
                      cubeb_data_callback data_callback,
                      cubeb_state_callback state_callback,
                      void * user_ptr);
  void (* stream_destroy)(cubeb_stream * stream);
  int (* stream_start)(cubeb_stream * stream);
  int (* stream_stop)(cubeb_stream * stream);
  int (* stream_get_position)(cubeb_stream * stream, uint64_t * position);
};

#endif /* CUBEB_INTERNAL_0eb56756_4e20_4404_a76d_42bf88cd15a5 */

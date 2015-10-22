// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef _MP4PARSE_RUST_H
#define _MP4PARSE_RUST_H

#ifdef __cplusplus
extern "C" {
#endif

struct mp4parse_state;

struct mp4parse_state* mp4parse_new(void);
void mp4parse_free(struct mp4parse_state* state);

int32_t mp4parse_read(struct mp4parse_state* state, uint8_t *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif

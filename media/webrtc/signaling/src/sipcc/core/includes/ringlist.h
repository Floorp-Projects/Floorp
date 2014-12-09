/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RINGLIST_H
#define RINGLIST_H

#define MAX_RINGS              56
#define RING_LIST_BUFFER_SIZE  (MAX_RINGS*64)

extern uint16_t rng_get_num_ringers(void);
extern int8_t *rng_get_ring_name(uint16_t);
extern void rng_play_ring(uint16_t);
extern void rng_set_ringer(void);
extern boolean rng_select_ringer(uint16_t);
extern uint16_t rng_get_selected_ringnum(void);
extern int16_t handle_ringlist(int16_t, void *);
extern void rng_request_ringlist(void);
extern void rng_init(void);
extern void rng_cancel_ring(void);

#endif

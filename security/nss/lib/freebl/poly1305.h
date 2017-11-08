/*
 * poly1305.h - header file for Poly1305 implementation.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FREEBL_POLY1305_H_
#define FREEBL_POLY1305_H_

typedef unsigned char poly1305_state[512];

/* Poly1305Init sets up |state| so that it can be used to calculate an
 * authentication tag with the one-time key |key|. Note that |key| is a
 * one-time key and therefore there is no `reset' method because that would
 * enable several messages to be authenticated with the same key. */
extern void Poly1305Init(poly1305_state* state, const unsigned char key[32]);

/* Poly1305Update processes |in_len| bytes from |in|. It can be called zero or
 * more times after poly1305_init. */
extern void Poly1305Update(poly1305_state* state, const unsigned char* in,
                           size_t inLen);

/* Poly1305Finish completes the poly1305 calculation and writes a 16 byte
 * authentication tag to |mac|. */
extern void Poly1305Finish(poly1305_state* state, unsigned char mac[16]);

#endif /* FREEBL_POLY1305_H_ */

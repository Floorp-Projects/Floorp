/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "mparray.h"
#include "prg.h"
#include "share.h"

/*
 * The PrioPacketClient object holds the encoded client data.
 * The client sends one packet to server A and one packet to
 * server B. The `for_server` parameter determines which server
 * the packet is for.
 */
typedef struct prio_packet_client* PrioPacketClient;
typedef const struct prio_packet_client* const_PrioPacketClient;

struct server_a_data
{
  // These values are only set for server A.
  MPArray data_shares;
  MPArray h_points;
};

struct server_b_data
{
  // This value is only used for server B.
  //
  // We use a pseudo-random generator to compress the secret-shared data
  // values. See Appendix I of the Prio paper (the paragraph starting
  // "Optimization: PRG secret sharing.") for details on this.
  PrioPRGSeed seed;
};

/*
 * The data that a Prio client sends to each server.
 */
struct prio_packet_client
{
  // TODO: Can also use a PRG to avoid need for sending Beaver triple shares.
  // Since this optimization only saves ~30 bytes of communication, we haven't
  // bothered implementing it yet.
  BeaverTriple triple;

  mp_int f0_share, g0_share, h0_share;
  PrioServerId for_server;

  union
  {
    struct server_a_data A;
    struct server_b_data B;
  } shares;
};

PrioPacketClient PrioPacketClient_new(const_PrioConfig cfg,
                                      PrioServerId for_server);
void PrioPacketClient_clear(PrioPacketClient p);
SECStatus PrioPacketClient_set_data(const_PrioConfig cfg, const bool* data_in,
                                    PrioPacketClient for_server_a,
                                    PrioPacketClient for_server_b);

SECStatus PrioPacketClient_decrypt(PrioPacketClient p, const_PrioConfig cfg,
                                   PrivateKey server_priv,
                                   const unsigned char* data_in,
                                   unsigned int data_len);

bool PrioPacketClient_areEqual(const_PrioPacketClient p1,
                               const_PrioPacketClient p2);

#endif /* __CLIENT_H__ */

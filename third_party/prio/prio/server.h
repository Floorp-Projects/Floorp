/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __SERVER_H__
#define __SERVER_H__

#include "mparray.h"
#include "prg.h"
#include "share.h"

struct prio_total_share
{
  PrioServerId idx;
  MPArray data_shares;
};

struct prio_server
{
  const_PrioConfig cfg;
  PrioServerId idx;

  // Sever's private decryption key
  PrivateKey priv_key;

  // The accumulated data values from the clients.
  MPArray data_shares;

  // PRG used to generate randomness for checking the client
  // data packets. Both servers initialize this PRG with the
  // same shared seed.
  PRG prg;
};

struct prio_verifier
{
  PrioServer s;

  PrioPacketClient clientp;
  MPArray data_sharesB;
  MPArray h_pointsB;

  mp_int share_fR;
  mp_int share_gR;
  mp_int share_hR;
  mp_int share_out;
};

struct prio_packet_verify1
{
  mp_int share_d;
  mp_int share_e;
};

struct prio_packet_verify2
{
  mp_int share_out;
};

#endif /* __SERVER_H__ */

/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mpi.h>
#include <mprio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "config.h"
#include "encrypt.h"
#include "poly.h"
#include "rand.h"
#include "serial.h"
#include "share.h"
#include "util.h"

// Let the points of data_in be [x1, x2, x3, ... ].
// We construct the polynomial f such that
// (a)    f(0) = random,
// (b)    f(i) = x_i  for all i >= 1,
// (c)    degree(f)+1 is a power of two.
// We then evaluate f at the 2N-th roots of unity
// and we return these evaluations as `evals_out`
// and we return f(0) as `const_term`.
static SECStatus
data_polynomial_evals(const_PrioConfig cfg, const_MPArray data_in,
                      MPArray evals_out, mp_int* const_term)
{
  SECStatus rv = SECSuccess;
  const mp_int* mod = &cfg->modulus;
  MPArray points_f = NULL;
  MPArray poly_f = NULL;

  // Number of multiplication gates in the Valid() circuit.
  const int mul_gates = cfg->num_data_fields;

  // Little n is the number of points on the polynomials.
  // The constant term is randomized, so it's (mul_gates + 1).
  const int n = mul_gates + 1;

  // Big N is n rounded up to a power of two.
  const int N = next_power_of_two(n);

  P_CHECKA(points_f = MPArray_new(N));
  P_CHECKA(poly_f = MPArray_new(N));

  // Set constant term f(0) to random
  P_CHECKC(rand_int(&points_f->data[0], mod));
  MP_CHECKC(mp_copy(&points_f->data[0], const_term));

  // Set other values of f(x)
  for (int i = 1; i < n; i++) {
    MP_CHECKC(mp_copy(&data_in->data[i - 1], &points_f->data[i]));
  }

  // Interpolate through the Nth roots of unity
  P_CHECKC(poly_fft(poly_f, points_f, cfg, true));

  // Evaluate at all 2N-th roots of unity.
  // To do so, first resize the eval arrays and fill upper
  // values with zeros.
  P_CHECKC(MPArray_resize(poly_f, 2 * N));
  P_CHECKC(MPArray_resize(evals_out, 2 * N));

  // Evaluate at the 2N-th roots of unity
  P_CHECKC(poly_fft(evals_out, poly_f, cfg, false));

cleanup:
  MPArray_clear(points_f);
  MPArray_clear(poly_f);

  return rv;
}

static SECStatus
share_polynomials(const_PrioConfig cfg, const_MPArray data_in,
                  PrioPacketClient pA, PrioPacketClient pB, PRG prgB)
{
  SECStatus rv = SECSuccess;
  const mp_int* mod = &cfg->modulus;
  const_MPArray points_f = data_in;

  mp_int f0, g0;
  MP_DIGITS(&f0) = NULL;
  MP_DIGITS(&g0) = NULL;

  MPArray points_g = NULL;
  MPArray evals_f_2N = NULL;
  MPArray evals_g_2N = NULL;

  P_CHECKA(points_g = MPArray_dup(points_f));
  P_CHECKA(evals_f_2N = MPArray_new(0));
  P_CHECKA(evals_g_2N = MPArray_new(0));
  MP_CHECKC(mp_init(&f0));
  MP_CHECKC(mp_init(&g0));

  for (int i = 0; i < points_f->len; i++) {
    // For each input value x_i, we compute x_i * (x_i-1).
    //    f(i) = x_i
    //    g(i) = x_i - 1
    MP_CHECKC(mp_sub_d(&points_g->data[i], 1, &points_g->data[i]));
    MP_CHECKC(mp_mod(&points_g->data[i], mod, &points_g->data[i]));
  }

  P_CHECKC(data_polynomial_evals(cfg, points_f, evals_f_2N, &f0));
  P_CHECKC(data_polynomial_evals(cfg, points_g, evals_g_2N, &g0));

  // The values f(0) and g(0) are set to random values.
  // We must send to each server a share of the points
  //    f(0),   g(0),   and   h(0) = f(0)*g(0)
  P_CHECKC(share_int(cfg, &f0, &pA->f0_share, &pB->f0_share));
  P_CHECKC(share_int(cfg, &g0, &pA->g0_share, &pB->g0_share));

  // Compute h(0) = f(0)*g(0).
  MP_CHECKC(mp_mulmod(&f0, &g0, mod, &f0));
  // Give one share of h(0) to each server.
  P_CHECKC(share_int(cfg, &f0, &pA->h0_share, &pB->h0_share));

  // const int lenN = (evals_f_2N->len/2);
  // P_CHECKC (MPArray_resize (pA->shares.A.h_points, lenN));

  // We need to send to the servers the evaluations of
  //   f(r) * g(r)
  // for all 2N-th roots of unity r that are not also
  // N-th roots of unity.
  //
  // For each such root r, compute h(r) = f(r)*g(r) and
  // send a share of this value to each server.
  int j = 0;
  for (int i = 1; i < evals_f_2N->len; i += 2) {
    MP_CHECKC(mp_mulmod(&evals_f_2N->data[i], &evals_g_2N->data[i], mod, &f0));
    P_CHECKC(PRG_share_int(prgB, &pA->shares.A.h_points->data[j], &f0, cfg));
    j++;
  }

cleanup:
  MPArray_clear(evals_f_2N);
  MPArray_clear(evals_g_2N);
  MPArray_clear(points_g);
  mp_clear(&f0);
  mp_clear(&g0);
  return rv;
}

PrioPacketClient
PrioPacketClient_new(const_PrioConfig cfg, PrioServerId for_server)
{
  SECStatus rv = SECSuccess;
  const int data_len = cfg->num_data_fields;
  PrioPacketClient p = NULL;
  p = malloc(sizeof(*p));
  if (!p)
    return NULL;

  p->for_server = for_server;
  p->triple = NULL;
  MP_DIGITS(&p->f0_share) = NULL;
  MP_DIGITS(&p->g0_share) = NULL;
  MP_DIGITS(&p->h0_share) = NULL;

  switch (p->for_server) {
    case PRIO_SERVER_A:
      p->shares.A.data_shares = NULL;
      p->shares.A.h_points = NULL;
      break;
    case PRIO_SERVER_B:
      memset(p->shares.B.seed, 0, PRG_SEED_LENGTH);
      break;
    default:
      // Should never get here
      rv = SECFailure;
      goto cleanup;
  }

  MP_CHECKC(mp_init(&p->f0_share));
  MP_CHECKC(mp_init(&p->g0_share));
  MP_CHECKC(mp_init(&p->h0_share));
  P_CHECKA(p->triple = BeaverTriple_new());

  if (p->for_server == PRIO_SERVER_A) {
    const int num_h_points = PrioConfig_hPoints(cfg);
    P_CHECKA(p->shares.A.data_shares = MPArray_new(data_len));
    P_CHECKA(p->shares.A.h_points = MPArray_new(num_h_points));
  }

cleanup:
  if (rv != SECSuccess) {
    PrioPacketClient_clear(p);
    return NULL;
  }

  return p;
}

SECStatus
PrioPacketClient_set_data(const_PrioConfig cfg, const bool* data_in,
                          PrioPacketClient pA, PrioPacketClient pB)
{
  MPArray client_data = NULL;
  PRG prgB = NULL;
  SECStatus rv = SECSuccess;
  const int data_len = cfg->num_data_fields;

  if (!data_in)
    return SECFailure;

  P_CHECKC(PrioPRGSeed_randomize(&pB->shares.B.seed));
  P_CHECKA(prgB = PRG_new(pB->shares.B.seed));

  P_CHECKC(BeaverTriple_set_rand(cfg, pA->triple, pB->triple));
  P_CHECKA(client_data = MPArray_new_bool(data_len, data_in));
  P_CHECKC(PRG_share_array(prgB, pA->shares.A.data_shares, client_data, cfg));
  P_CHECKC(share_polynomials(cfg, client_data, pA, pB, prgB));

cleanup:
  MPArray_clear(client_data);
  PRG_clear(prgB);

  return rv;
}

void
PrioPacketClient_clear(PrioPacketClient p)
{
  if (p == NULL)
    return;

  if (p->for_server == PRIO_SERVER_A) {
    MPArray_clear(p->shares.A.h_points);
    MPArray_clear(p->shares.A.data_shares);
  }

  BeaverTriple_clear(p->triple);
  mp_clear(&p->f0_share);
  mp_clear(&p->g0_share);
  mp_clear(&p->h0_share);
  free(p);
}

bool
PrioPacketClient_areEqual(const_PrioPacketClient p1, const_PrioPacketClient p2)
{
  if (!BeaverTriple_areEqual(p1->triple, p2->triple))
    return false;
  if (mp_cmp(&p1->f0_share, &p2->f0_share))
    return false;
  if (mp_cmp(&p1->g0_share, &p2->g0_share))
    return false;
  if (mp_cmp(&p1->h0_share, &p2->h0_share))
    return false;
  if (p1->for_server != p2->for_server)
    return false;

  switch (p1->for_server) {
    case PRIO_SERVER_A:
      if (!MPArray_areEqual(p1->shares.A.data_shares, p2->shares.A.data_shares))
        return false;
      if (!MPArray_areEqual(p1->shares.A.h_points, p2->shares.A.h_points))
        return false;
      break;
    case PRIO_SERVER_B:
      if (memcmp(p1->shares.B.seed, p2->shares.B.seed, PRG_SEED_LENGTH))
        return false;
      break;
    default:
      // Should never get here.
      return false;
  }

  return true;
}

SECStatus
PrioClient_encode(const_PrioConfig cfg, const bool* data_in,
                  unsigned char** for_server_a, unsigned int* aLen,
                  unsigned char** for_server_b, unsigned int* bLen)
{
  SECStatus rv = SECSuccess;
  PrioPacketClient pA = NULL;
  PrioPacketClient pB = NULL;
  *for_server_a = NULL;
  *for_server_b = NULL;

  P_CHECKA(pA = PrioPacketClient_new(cfg, PRIO_SERVER_A));
  P_CHECKA(pB = PrioPacketClient_new(cfg, PRIO_SERVER_B));

  msgpack_sbuffer sbufA, sbufB;
  msgpack_packer packerA, packerB;

  msgpack_sbuffer_init(&sbufA);
  msgpack_sbuffer_init(&sbufB);
  msgpack_packer_init(&packerA, &sbufA, msgpack_sbuffer_write);
  msgpack_packer_init(&packerB, &sbufB, msgpack_sbuffer_write);

  P_CHECKC(PrioPacketClient_set_data(cfg, data_in, pA, pB));
  P_CHECKC(serial_write_packet_client(&packerA, pA, cfg));
  P_CHECKC(serial_write_packet_client(&packerB, pB, cfg));

  P_CHECKC(PublicKey_encryptSize(sbufA.size, aLen));
  P_CHECKC(PublicKey_encryptSize(sbufB.size, bLen));

  P_CHECKA(*for_server_a = malloc(*aLen));
  P_CHECKA(*for_server_b = malloc(*bLen));

  unsigned int writtenA;
  unsigned int writtenB;
  P_CHECKC(PublicKey_encrypt(cfg->server_a_pub, *for_server_a, &writtenA, *aLen,
                             (unsigned char*)sbufA.data, sbufA.size));
  P_CHECKC(PublicKey_encrypt(cfg->server_b_pub, *for_server_b, &writtenB, *bLen,
                             (unsigned char*)sbufB.data, sbufB.size));

  P_CHECKCB(writtenA == *aLen);
  P_CHECKCB(writtenB == *bLen);

cleanup:
  if (rv != SECSuccess) {
    if (*for_server_a)
      free(*for_server_a);
    if (*for_server_b)
      free(*for_server_b);
    *for_server_a = NULL;
    *for_server_b = NULL;
  }

  PrioPacketClient_clear(pA);
  PrioPacketClient_clear(pB);
  msgpack_sbuffer_destroy(&sbufA);
  msgpack_sbuffer_destroy(&sbufB);

  return rv;
}

SECStatus
PrioPacketClient_decrypt(PrioPacketClient p, const_PrioConfig cfg,
                         PrivateKey server_priv, const unsigned char* data_in,
                         unsigned int data_len)
{
  SECStatus rv = SECSuccess;
  msgpack_unpacker upk;
  P_CHECKCB(msgpack_unpacker_init(&upk, data_len));

  // Decrypt the ciphertext into dec_buf
  unsigned int bytes_decrypted;
  P_CHECKC(PrivateKey_decrypt(server_priv,
                              (unsigned char*)msgpack_unpacker_buffer(&upk),
                              &bytes_decrypted, data_len, data_in, data_len));
  msgpack_unpacker_buffer_consumed(&upk, bytes_decrypted);

  P_CHECKC(serial_read_packet_client(&upk, p, cfg));

cleanup:
  msgpack_unpacker_destroy(&upk);
  return rv;
}

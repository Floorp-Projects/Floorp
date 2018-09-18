/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mpi.h>
#include <mprio.h>
#include <stdio.h>
#include <stdlib.h>

#include "client.h"
#include "mparray.h"
#include "poly.h"
#include "prg.h"
#include "server.h"
#include "util.h"

PrioServer
PrioServer_new(const_PrioConfig cfg, PrioServerId server_idx,
               PrivateKey server_priv, const PrioPRGSeed seed)
{
  SECStatus rv = SECSuccess;
  PrioServer s = malloc(sizeof(*s));
  if (!s)
    return NULL;
  s->cfg = cfg;
  s->idx = server_idx;
  s->priv_key = server_priv;
  s->data_shares = NULL;
  s->prg = NULL;

  P_CHECKA(s->data_shares = MPArray_new(s->cfg->num_data_fields));
  P_CHECKA(s->prg = PRG_new(seed));

cleanup:
  if (rv != SECSuccess) {
    PrioServer_clear(s);
    return NULL;
  }

  return s;
}

void
PrioServer_clear(PrioServer s)
{
  if (!s)
    return;

  PRG_clear(s->prg);
  MPArray_clear(s->data_shares);
  free(s);
}

SECStatus
PrioServer_aggregate(PrioServer s, PrioVerifier v)
{
  MPArray arr = NULL;
  switch (s->idx) {
    case PRIO_SERVER_A:
      arr = v->clientp->shares.A.data_shares;
      break;
    case PRIO_SERVER_B:
      arr = v->data_sharesB;
      break;
    default:
      // Should never get here
      return SECFailure;
  }

  return MPArray_addmod(s->data_shares, arr, &s->cfg->modulus);
}

PrioTotalShare
PrioTotalShare_new(void)
{
  PrioTotalShare t = malloc(sizeof(*t));
  if (!t)
    return NULL;

  t->data_shares = MPArray_new(0);
  if (!t->data_shares) {
    free(t);
    return NULL;
  }

  return t;
}

void
PrioTotalShare_clear(PrioTotalShare t)
{
  if (!t)
    return;
  MPArray_clear(t->data_shares);
  free(t);
}

SECStatus
PrioTotalShare_set_data(PrioTotalShare t, const_PrioServer s)
{
  t->idx = s->idx;
  SECStatus rv = SECSuccess;

  P_CHECK(MPArray_resize(t->data_shares, s->data_shares->len));
  P_CHECK(MPArray_copy(t->data_shares, s->data_shares));

  return rv;
}

SECStatus
PrioTotalShare_final(const_PrioConfig cfg, unsigned long* output,
                     const_PrioTotalShare tA, const_PrioTotalShare tB)
{
  if (tA->data_shares->len != cfg->num_data_fields)
    return SECFailure;
  if (tA->data_shares->len != tB->data_shares->len)
    return SECFailure;
  if (tA->idx != PRIO_SERVER_A || tB->idx != PRIO_SERVER_B)
    return SECFailure;

  SECStatus rv = SECSuccess;

  mp_int tmp;
  MP_DIGITS(&tmp) = NULL;
  MP_CHECKC(mp_init(&tmp));

  for (int i = 0; i < cfg->num_data_fields; i++) {
    MP_CHECKC(mp_addmod(&tA->data_shares->data[i], &tB->data_shares->data[i],
                        &cfg->modulus, &tmp));

    output[i] = tmp.dp[0];
  }

cleanup:
  mp_clear(&tmp);
  return rv;
}

inline static mp_int*
get_data_share(const_PrioVerifier v, int i)
{
  switch (v->s->idx) {
    case PRIO_SERVER_A:
      return &v->clientp->shares.A.data_shares->data[i];
    case PRIO_SERVER_B:
      return &v->data_sharesB->data[i];
  }
  // Should never get here
  return NULL;
}

inline static mp_int*
get_h_share(const_PrioVerifier v, int i)
{
  switch (v->s->idx) {
    case PRIO_SERVER_A:
      return &v->clientp->shares.A.h_points->data[i];
    case PRIO_SERVER_B:
      return &v->h_pointsB->data[i];
  }
  // Should never get here
  return NULL;
}

/*
 * Build shares of the polynomials f, g, and h used in the Prio verification
 * routine and evalute these polynomials at a random point determined
 * by the shared secret. Store the evaluations in the verifier object.
 */
static SECStatus
compute_shares(PrioVerifier v, const_PrioPacketClient p)
{
  SECStatus rv;
  const int n = v->s->cfg->num_data_fields + 1;
  const int N = next_power_of_two(n);
  mp_int eval_at;
  MP_DIGITS(&eval_at) = NULL;

  MPArray points_f = NULL;
  MPArray points_g = NULL;
  MPArray points_h = NULL;

  MP_CHECKC(mp_init(&eval_at));
  P_CHECKA(points_f = MPArray_new(N));
  P_CHECKA(points_g = MPArray_new(N));
  P_CHECKA(points_h = MPArray_new(2 * N));

  // Use PRG to generate random point
  MP_CHECKC(PRG_get_int(v->s->prg, &eval_at, &v->s->cfg->modulus));

  // Reduce value into the field we're using. This
  // doesn't yield exactly a uniformly random point,
  // but for values this large, it will be close
  // enough.
  MP_CHECKC(mp_mod(&eval_at, &v->s->cfg->modulus, &eval_at));

  // Client sends us the values of f(0) and g(0)
  MP_CHECKC(mp_copy(&p->f0_share, &points_f->data[0]));
  MP_CHECKC(mp_copy(&p->g0_share, &points_g->data[0]));
  MP_CHECKC(mp_copy(&p->h0_share, &points_h->data[0]));

  for (int i = 1; i < n; i++) {
    // [f](i) = i-th data share
    const mp_int* data_i_minus_1 = get_data_share(v, i - 1);
    MP_CHECKC(mp_copy(data_i_minus_1, &points_f->data[i]));

    // [g](i) = i-th data share minus 1
    // Only need to shift the share for 0-th server
    MP_CHECKC(mp_copy(&points_f->data[i], &points_g->data[i]));
    if (!v->s->idx) {
      MP_CHECKC(mp_sub_d(&points_g->data[i], 1, &points_g->data[i]));
      MP_CHECKC(
        mp_mod(&points_g->data[i], &v->s->cfg->modulus, &points_g->data[i]));
    }
  }

  int j = 0;
  for (int i = 1; i < 2 * N; i += 2) {
    const mp_int* h_point_j = get_h_share(v, j++);
    MP_CHECKC(mp_copy(h_point_j, &points_h->data[i]));
  }

  P_CHECKC(poly_interp_evaluate(&v->share_fR, points_f, &eval_at, v->s->cfg));
  P_CHECKC(poly_interp_evaluate(&v->share_gR, points_g, &eval_at, v->s->cfg));
  P_CHECKC(poly_interp_evaluate(&v->share_hR, points_h, &eval_at, v->s->cfg));

cleanup:
  MPArray_clear(points_f);
  MPArray_clear(points_g);
  MPArray_clear(points_h);
  mp_clear(&eval_at);
  return rv;
}

PrioVerifier
PrioVerifier_new(PrioServer s)
{
  SECStatus rv = SECSuccess;
  PrioVerifier v = malloc(sizeof *v);
  if (!v)
    return NULL;

  v->s = s;
  v->clientp = NULL;
  v->data_sharesB = NULL;
  v->h_pointsB = NULL;

  MP_DIGITS(&v->share_fR) = NULL;
  MP_DIGITS(&v->share_gR) = NULL;
  MP_DIGITS(&v->share_hR) = NULL;

  MP_CHECKC(mp_init(&v->share_fR));
  MP_CHECKC(mp_init(&v->share_gR));
  MP_CHECKC(mp_init(&v->share_hR));

  P_CHECKA(v->clientp = PrioPacketClient_new(s->cfg, s->idx));

  const int N = next_power_of_two(s->cfg->num_data_fields + 1);
  if (v->s->idx == PRIO_SERVER_B) {
    P_CHECKA(v->data_sharesB = MPArray_new(v->s->cfg->num_data_fields));
    P_CHECKA(v->h_pointsB = MPArray_new(N));
  }

cleanup:
  if (rv != SECSuccess) {
    PrioVerifier_clear(v);
    return NULL;
  }

  return v;
}

SECStatus
PrioVerifier_set_data(PrioVerifier v, unsigned char* data,
                      unsigned int data_len)
{
  SECStatus rv = SECSuccess;
  PRG prgB = NULL;
  P_CHECKC(PrioPacketClient_decrypt(v->clientp, v->s->cfg, v->s->priv_key, data,
                                    data_len));

  PrioPacketClient p = v->clientp;
  if (p->for_server != v->s->idx)
    return SECFailure;

  const int N = next_power_of_two(v->s->cfg->num_data_fields + 1);
  if (v->s->idx == PRIO_SERVER_A) {
    // Check that packet has the correct number of data fields
    if (p->shares.A.data_shares->len != v->s->cfg->num_data_fields)
      return SECFailure;
    if (p->shares.A.h_points->len != N)
      return SECFailure;
  }

  if (v->s->idx == PRIO_SERVER_B) {
    P_CHECKA(prgB = PRG_new(v->clientp->shares.B.seed));
    P_CHECKC(PRG_get_array(prgB, v->data_sharesB, &v->s->cfg->modulus));
    P_CHECKC(PRG_get_array(prgB, v->h_pointsB, &v->s->cfg->modulus));
  }

  // TODO: This can be done much faster by using the combined
  // interpolate-and-evaluate optimization described in the
  // Prio paper.
  //
  // Compute share of f(r), g(r), h(r)
  P_CHECKC(compute_shares(v, p));

cleanup:

  PRG_clear(prgB);
  return rv;
}

void
PrioVerifier_clear(PrioVerifier v)
{
  if (v == NULL)
    return;
  PrioPacketClient_clear(v->clientp);
  MPArray_clear(v->data_sharesB);
  MPArray_clear(v->h_pointsB);
  mp_clear(&v->share_fR);
  mp_clear(&v->share_gR);
  mp_clear(&v->share_hR);
  free(v);
}

PrioPacketVerify1
PrioPacketVerify1_new(void)
{
  SECStatus rv = SECSuccess;
  PrioPacketVerify1 p = malloc(sizeof *p);
  if (!p)
    return NULL;

  MP_DIGITS(&p->share_d) = NULL;
  MP_DIGITS(&p->share_e) = NULL;

  MP_CHECKC(mp_init(&p->share_d));
  MP_CHECKC(mp_init(&p->share_e));

cleanup:
  if (rv != SECSuccess) {
    PrioPacketVerify1_clear(p);
    return NULL;
  }

  return p;
}

void
PrioPacketVerify1_clear(PrioPacketVerify1 p)
{
  if (!p)
    return;
  mp_clear(&p->share_d);
  mp_clear(&p->share_e);
  free(p);
}

SECStatus
PrioPacketVerify1_set_data(PrioPacketVerify1 p1, const_PrioVerifier v)
{
  // See the Prio paper for details on how this works.
  // Appendix C descrives the MPC protocol used here.

  SECStatus rv = SECSuccess;

  // Compute corrections.
  //   [d] = [f(r)] - [a]
  MP_CHECK(mp_sub(&v->share_fR, &v->clientp->triple->a, &p1->share_d));
  MP_CHECK(mp_mod(&p1->share_d, &v->s->cfg->modulus, &p1->share_d));

  //   [e] = [g(r)] - [b]
  MP_CHECK(mp_sub(&v->share_gR, &v->clientp->triple->b, &p1->share_e));
  MP_CHECK(mp_mod(&p1->share_e, &v->s->cfg->modulus, &p1->share_e));

  return rv;
}

PrioPacketVerify2
PrioPacketVerify2_new(void)
{
  SECStatus rv = SECSuccess;
  PrioPacketVerify2 p = malloc(sizeof *p);
  if (!p)
    return NULL;

  MP_DIGITS(&p->share_out) = NULL;
  MP_CHECKC(mp_init(&p->share_out));

cleanup:
  if (rv != SECSuccess) {
    PrioPacketVerify2_clear(p);
    return NULL;
  }
  return p;
}

void
PrioPacketVerify2_clear(PrioPacketVerify2 p)
{
  if (!p)
    return;
  mp_clear(&p->share_out);
  free(p);
}

SECStatus
PrioPacketVerify2_set_data(PrioPacketVerify2 p2, const_PrioVerifier v,
                           const_PrioPacketVerify1 p1A,
                           const_PrioPacketVerify1 p1B)
{
  SECStatus rv = SECSuccess;

  mp_int d, e, tmp;
  MP_DIGITS(&d) = NULL;
  MP_DIGITS(&e) = NULL;
  MP_DIGITS(&tmp) = NULL;

  MP_CHECKC(mp_init(&d));
  MP_CHECKC(mp_init(&e));
  MP_CHECKC(mp_init(&tmp));

  const mp_int* mod = &v->s->cfg->modulus;

  // Compute share of f(r)*g(r)
  //    [f(r)*g(r)] = [d*e/2] + d[b] + e[a] + [c]

  // Compute d
  MP_CHECKC(mp_addmod(&p1A->share_d, &p1B->share_d, mod, &d));
  // Compute e
  MP_CHECKC(mp_addmod(&p1A->share_e, &p1B->share_e, mod, &e));

  // Compute d*e
  MP_CHECKC(mp_mulmod(&d, &e, mod, &p2->share_out));
  // out = d*e/2
  MP_CHECKC(mp_mulmod(&p2->share_out, &v->s->cfg->inv2, mod, &p2->share_out));

  // Compute d[b]
  MP_CHECKC(mp_mulmod(&d, &v->clientp->triple->b, mod, &tmp));
  // out = d*e/2 + d[b]
  MP_CHECKC(mp_addmod(&p2->share_out, &tmp, mod, &p2->share_out));

  // Compute e[a]
  MP_CHECKC(mp_mulmod(&e, &v->clientp->triple->a, mod, &tmp));
  // out = d*e/2 + d[b] + e[a]
  MP_CHECKC(mp_addmod(&p2->share_out, &tmp, mod, &p2->share_out));

  // out = d*e/2 + d[b] + e[a] + [c]
  MP_CHECKC(
    mp_addmod(&p2->share_out, &v->clientp->triple->c, mod, &p2->share_out));

  // We want to compute f(r)*g(r) - h(r),
  // so subtract off [h(r)]:
  //    out = d*e/2 + d[b] + e[a] + [c] - [h(r)]
  MP_CHECKC(mp_sub(&p2->share_out, &v->share_hR, &p2->share_out));
  MP_CHECKC(mp_mod(&p2->share_out, mod, &p2->share_out));

cleanup:
  mp_clear(&d);
  mp_clear(&e);
  mp_clear(&tmp);
  return rv;
}

int
PrioVerifier_isValid(const_PrioVerifier v, const_PrioPacketVerify2 pA,
                     const_PrioPacketVerify2 pB)
{
  SECStatus rv = SECSuccess;
  mp_int res;
  MP_DIGITS(&res) = NULL;
  MP_CHECKC(mp_init(&res));

  // Add up the shares of the output wire value and
  // ensure that the sum is equal to zero, which indicates
  // that
  //      f(r) * g(r) == h(r).
  MP_CHECKC(
    mp_addmod(&pA->share_out, &pB->share_out, &v->s->cfg->modulus, &res));

  rv = (mp_cmp_d(&res, 0) == 0) ? SECSuccess : SECFailure;

cleanup:
  mp_clear(&res);
  return rv;
}

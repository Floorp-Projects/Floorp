/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */

#include <mprio.h>
#include <msgpack.h>
#include <string.h>

#include "mutest.h"
#include "prio/client.h"
#include "prio/config.h"
#include "prio/serial.h"
#include "prio/server.h"
#include "prio/util.h"

SECStatus
gen_client_packets (const_PrioConfig cfg, PrioPacketClient pA, PrioPacketClient pB)
{
  SECStatus rv = SECSuccess;

  const int ndata = cfg->num_data_fields;
  bool data_items[ndata];

  for (int i=0; i < ndata; i++) {
    data_items[i] = (i % 3 == 1) || (i % 5 == 3);
  }

  P_CHECKC (PrioPacketClient_set_data (cfg, data_items, pA, pB));

cleanup:
  return rv;
}


void serial_client (int bad)
{
  SECStatus rv = SECSuccess;
  PrioConfig cfg = NULL;
  PrioConfig cfg2 = NULL;
  PrioPacketClient pA = NULL;
  PrioPacketClient pB = NULL;
  PrioPacketClient qA = NULL;
  PrioPacketClient qB = NULL;

  const unsigned char *batch_id1 = (unsigned char *)"my_test_prio_batch1";
  const unsigned char *batch_id2 = (unsigned char *)"my_test_prio_batch2";
  const unsigned int batch_id_len = strlen ((char *)batch_id1);

  msgpack_sbuffer sbufA, sbufB;
  msgpack_packer pkA, pkB;
  msgpack_unpacker upkA, upkB;

  msgpack_sbuffer_init (&sbufA); 
  msgpack_packer_init (&pkA, &sbufA, msgpack_sbuffer_write);

  msgpack_sbuffer_init (&sbufB); 
  msgpack_packer_init (&pkB, &sbufB, msgpack_sbuffer_write);

  P_CHECKA (cfg = PrioConfig_new (100, NULL, NULL, batch_id1, batch_id_len));
  P_CHECKA (cfg2 = PrioConfig_new (100, NULL, NULL, batch_id2, batch_id_len));
  P_CHECKA (pA = PrioPacketClient_new (cfg, PRIO_SERVER_A));
  P_CHECKA (pB = PrioPacketClient_new (cfg, PRIO_SERVER_B));
  P_CHECKA (qA = PrioPacketClient_new (cfg, PRIO_SERVER_A));
  P_CHECKA (qB = PrioPacketClient_new (cfg, PRIO_SERVER_B));

  P_CHECKC (gen_client_packets (cfg, pA, pB));

  P_CHECKC (serial_write_packet_client (&pkA, pA, cfg));
  P_CHECKC (serial_write_packet_client (&pkB, pB, cfg));

  if (bad == 1) {
    sbufA.size = 1;
  }

  if (bad == 2) {
    memset (sbufA.data, 0, sbufA.size);
  }

  const int size_a = sbufA.size;
  const int size_b = sbufB.size;

  P_CHECKCB (msgpack_unpacker_init (&upkA, 0));
  P_CHECKCB (msgpack_unpacker_init (&upkB, 0));

  P_CHECKCB (msgpack_unpacker_reserve_buffer (&upkA, size_a));
  P_CHECKCB (msgpack_unpacker_reserve_buffer (&upkB, size_b));

  memcpy (msgpack_unpacker_buffer (&upkA), sbufA.data, size_a);
  memcpy (msgpack_unpacker_buffer (&upkB), sbufB.data, size_b);

  msgpack_unpacker_buffer_consumed (&upkA, size_a);
  msgpack_unpacker_buffer_consumed (&upkB, size_b);

  P_CHECKC (serial_read_packet_client (&upkA, qA, cfg));
  P_CHECKC (serial_read_packet_client (&upkB, qB, (bad == 3) ? cfg2 : cfg));

  if (!bad) {
    mu_check (PrioPacketClient_areEqual (pA, qA));
    mu_check (PrioPacketClient_areEqual (pB, qB));
    mu_check (!PrioPacketClient_areEqual (pB, qA));
    mu_check (!PrioPacketClient_areEqual (pA, qB));
  }

cleanup: 
  PrioPacketClient_clear (pA);
  PrioPacketClient_clear (pB);
  PrioPacketClient_clear (qA);
  PrioPacketClient_clear (qB);
  PrioConfig_clear (cfg);
  PrioConfig_clear (cfg2);
  msgpack_sbuffer_destroy (&sbufA);
  msgpack_sbuffer_destroy (&sbufB);
  msgpack_unpacker_destroy (&upkA);
  msgpack_unpacker_destroy (&upkB);
  mu_check (bad ? rv == SECFailure : rv == SECSuccess);
}


void mu_test__serial_client (void) 
{
  serial_client (0);
}

void mu_test__serial_client_bad1 (void) 
{
  serial_client (1);
}

void mu_test__serial_client_bad2 (void) 
{
  serial_client (2);
}

void mu_test__serial_client_bad3 (void) 
{
  serial_client (3);
}

void test_verify1 (int bad)
{
  SECStatus rv = SECSuccess;
  PrioPacketVerify1 v1 = NULL;
  PrioPacketVerify1 v2 = NULL;
  PrioConfig cfg = NULL;
  
  P_CHECKA (cfg = PrioConfig_newTest (1));
  P_CHECKA (v1 = PrioPacketVerify1_new());
  P_CHECKA (v2 = PrioPacketVerify1_new());
  mp_set (&v1->share_d, 4);
  mp_set (&v1->share_e, 10);

  msgpack_sbuffer sbuf;
  msgpack_packer pk;
  msgpack_unpacker upk;

  msgpack_sbuffer_init (&sbuf); 
  msgpack_packer_init (&pk, &sbuf, msgpack_sbuffer_write);

  P_CHECKC (PrioPacketVerify1_write (v1, &pk));

  if (bad == 1) {
    mp_set (&cfg->modulus, 6);
  }

  P_CHECKCB (msgpack_unpacker_init (&upk, 0));
  P_CHECKCB (msgpack_unpacker_reserve_buffer (&upk, sbuf.size));
  memcpy (msgpack_unpacker_buffer (&upk), sbuf.data, sbuf.size);
  msgpack_unpacker_buffer_consumed (&upk, sbuf.size);

  P_CHECKC (PrioPacketVerify1_read (v2, &upk, cfg));

  mu_check (!mp_cmp (&v1->share_d, &v2->share_d));
  mu_check (!mp_cmp (&v1->share_e, &v2->share_e));
  mu_check (!mp_cmp_d (&v2->share_d, 4));
  mu_check (!mp_cmp_d (&v2->share_e, 10));

cleanup:
  mu_check (bad ? rv == SECFailure : rv == SECSuccess);
  PrioConfig_clear (cfg);
  PrioPacketVerify1_clear (v1);
  PrioPacketVerify1_clear (v2);
  msgpack_unpacker_destroy (&upk);
  msgpack_sbuffer_destroy (&sbuf);
}

void mu_test_verify1_good (void)
{
  test_verify1 (0);
}

void mu_test_verify1_bad (void)
{
  test_verify1 (1);
}

void test_verify2 (int bad)
{
  SECStatus rv = SECSuccess;
  PrioPacketVerify2 v1 = NULL;
  PrioPacketVerify2 v2 = NULL;
  PrioConfig cfg = NULL;
  
  P_CHECKA (cfg = PrioConfig_newTest (1));
  P_CHECKA (v1 = PrioPacketVerify2_new());
  P_CHECKA (v2 = PrioPacketVerify2_new());
  mp_set (&v1->share_out, 4);

  msgpack_sbuffer sbuf;
  msgpack_packer pk;
  msgpack_unpacker upk;

  msgpack_sbuffer_init (&sbuf); 
  msgpack_packer_init (&pk, &sbuf, msgpack_sbuffer_write);

  P_CHECKC (PrioPacketVerify2_write (v1, &pk));

  if (bad == 1) {
    mp_set (&cfg->modulus, 4);
  }

  P_CHECKCB (msgpack_unpacker_init (&upk, 0));
  P_CHECKCB (msgpack_unpacker_reserve_buffer (&upk, sbuf.size));
  memcpy (msgpack_unpacker_buffer (&upk), sbuf.data, sbuf.size);
  msgpack_unpacker_buffer_consumed (&upk, sbuf.size);

  P_CHECKC (PrioPacketVerify2_read (v2, &upk, cfg));

  mu_check (!mp_cmp (&v1->share_out, &v2->share_out));
  mu_check (!mp_cmp_d (&v2->share_out, 4));

cleanup:
  mu_check (bad ? rv == SECFailure : rv == SECSuccess);
  PrioConfig_clear (cfg);
  PrioPacketVerify2_clear (v1);
  PrioPacketVerify2_clear (v2);
  msgpack_unpacker_destroy (&upk);
  msgpack_sbuffer_destroy (&sbuf);
}

void mu_test_verify2_good (void)
{
  test_verify2 (0);
}

void mu_test_verify2_bad (void)
{
  test_verify2 (1);
}


void test_total_share (int bad)
{
  SECStatus rv = SECSuccess;
  PrioTotalShare t1 = NULL;
  PrioTotalShare t2 = NULL;
  PrioConfig cfg = NULL;
  
  P_CHECKA (cfg = PrioConfig_newTest ((bad == 2 ? 4 : 3)));
  P_CHECKA (t1 = PrioTotalShare_new ());
  P_CHECKA (t2 = PrioTotalShare_new ());

  t1->idx = PRIO_SERVER_A;
  P_CHECKC (MPArray_resize (t1->data_shares, 3));

  mp_set (&t1->data_shares->data[0], 10);
  mp_set (&t1->data_shares->data[1], 20);
  mp_set (&t1->data_shares->data[2], 30);

  msgpack_sbuffer sbuf;
  msgpack_packer pk;
  msgpack_unpacker upk;

  msgpack_sbuffer_init (&sbuf); 
  msgpack_packer_init (&pk, &sbuf, msgpack_sbuffer_write);

  P_CHECKC (PrioTotalShare_write (t1, &pk));

  if (bad == 1) {
    mp_set (&cfg->modulus, 4);
  }

  P_CHECKCB (msgpack_unpacker_init (&upk, 0));
  P_CHECKCB (msgpack_unpacker_reserve_buffer (&upk, sbuf.size));
  memcpy (msgpack_unpacker_buffer (&upk), sbuf.data, sbuf.size);
  msgpack_unpacker_buffer_consumed (&upk, sbuf.size);

  P_CHECKC (PrioTotalShare_read (t2, &upk, cfg));

  mu_check (t1->idx == t2->idx);
  mu_check (MPArray_areEqual (t1->data_shares, t2->data_shares));

cleanup:
  mu_check (bad ? rv == SECFailure : rv == SECSuccess);
  PrioConfig_clear (cfg);
  PrioTotalShare_clear (t1);
  PrioTotalShare_clear (t2);
  msgpack_unpacker_destroy (&upk);
  msgpack_sbuffer_destroy (&sbuf);
}

void mu_test_total_good (void)
{
  test_total_share (0);
}

void mu_test_total_bad1 (void)
{
  test_total_share (1);
}

void mu_test_total_bad2 (void)
{
  test_total_share (2);
}


/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define _GNU_SOURCE

#include <mprio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "prio/encrypt.h"
#include "prio/util.h"

static void 
init_data (bool *data, int datalen) 
{
  // The client's data submission is an arbitrary boolean vector.
  for (int i=0; i < datalen; i++) {
    // Arbitrary data
    data[i] = rand () % 2;
  }
}

static SECStatus
read_string_from_hex (unsigned char **str_out, unsigned int *strLen,
    const char *input, const char **new_input)
{
  SECStatus rv = SECSuccess;
  *strLen = 0;
  int read = 0;
  int outCount = 0;
  const char *inp = input;

  while (true) {
    unsigned char byte = '\0';
    const int retval = sscanf(inp, "%02hhx,%n", &byte, &read);
    if (retval < 1 || read != 3) {
      break;
    }

    inp += read;
    (*str_out)[outCount] = byte;
    outCount++;
    *strLen = *strLen + 1;
  }

  if (new_input)
    *new_input = inp + 1;

  return rv;
}

static SECStatus
read_browser_reply (FILE *infile,
    unsigned char **for_server_a, unsigned int *aLen,
    unsigned char **for_server_b, unsigned int *bLen)
{
  SECStatus rv = SECFailure;
  char *raw_input = NULL;
  size_t rawLen = 0;

  puts ("Getting line of input.");
  P_CHECKCB (getline (&raw_input, &rawLen, infile) > 0);
  puts ("Got line of input.");

  P_CHECKA (*for_server_a = malloc (rawLen * sizeof (unsigned char)));
  P_CHECKA (*for_server_b = malloc (rawLen * sizeof (unsigned char)));

  *aLen = 0;
  *bLen = 0;

  P_CHECKCB (rawLen > 14);

  // Header is 14 chars long
  const char *new_input;
  puts ("Reading string A");
  P_CHECKC (read_string_from_hex (for_server_a, aLen, raw_input + 14, &new_input));
  puts ("Read string A");

  // Skip over for_server_a string and one-char delimeter
  puts ("Reading string B");
  P_CHECKC (read_string_from_hex (for_server_b, bLen, new_input, NULL));
  puts ("Read string B");

cleanup:
  if (raw_input) free (raw_input);
  return rv;
}

static int
verify_full (const char *path_to_xpcshell, int pathlen)
{
  SECStatus rv = SECSuccess;

  PublicKey pkA = NULL;
  PublicKey pkB = NULL;
  PrivateKey skA = NULL;
  PrivateKey skB = NULL;

  PrioConfig cfg = NULL;
  PrioServer sA = NULL;
  PrioServer sB = NULL;
  PrioVerifier vA = NULL;
  PrioVerifier vB = NULL;
  PrioPacketVerify1 p1A = NULL;
  PrioPacketVerify1 p1B = NULL;
  PrioPacketVerify2 p2A = NULL;
  PrioPacketVerify2 p2B = NULL;
  PrioTotalShare tA = NULL;
  PrioTotalShare tB = NULL;

  FILE *shell = NULL;
  int cmdlen = pathlen + 2*CURVE25519_KEY_LEN_HEX + 128;
  char cmd[cmdlen];
  memset (cmd, 0, cmdlen);

  unsigned char *for_server_a = NULL;
  unsigned char *for_server_b = NULL;

  const int seed = time (NULL);
  srand (seed);
  printf ("Using srand seed %d\n", seed);

  // Number of different boolean data fields we collect.
  const int ndata = 3;

  //unsigned char batch_id_str[] = "abcde";
  unsigned char batch_id_str[32];
  memset (batch_id_str, 0, sizeof batch_id_str);
  snprintf ((char *)batch_id_str, sizeof batch_id_str, "%d", rand());
  
  bool data_items[ndata];
  unsigned long output[ndata];
  init_data (data_items, ndata);

  // Initialize NSS random number generator.
  P_CHECKC (Prio_init ());

  // Generate keypairs for servers
  P_CHECKC (Keypair_new (&skA, &pkA));
  P_CHECKC (Keypair_new (&skB, &pkB));

  // Export public keys to hex and print to stdout
  unsigned char pk_hexA[CURVE25519_KEY_LEN_HEX+1];
  unsigned char pk_hexB[CURVE25519_KEY_LEN_HEX+1];
  P_CHECKC (PublicKey_export_hex (pkA, pk_hexA));
  P_CHECKC (PublicKey_export_hex (pkB, pk_hexB));

  snprintf (cmd, cmdlen, "%s %s %s %s %s %d %d %d", 
      path_to_xpcshell, "encode-once.js", 
      pk_hexA, pk_hexB, batch_id_str, 
      data_items[0], data_items[1], data_items[2]);

  printf ("> %s\n", cmd);
  P_CHECKA (shell = popen(cmd, "r"));
  puts("Ran command.");

  // Use the default configuration parameters.
  P_CHECKA (cfg = PrioConfig_new (ndata, pkA, pkB, batch_id_str, 
        strlen ((char *)batch_id_str)));

  PrioPRGSeed server_secret;
  P_CHECKC (PrioPRGSeed_randomize (&server_secret));

  // Initialize two server objects. The role of the servers need not
  // be symmetric. In a deployment, we envision that:
  //   * Server A is the main telemetry server that is always online. 
  //     Clients send their encrypted data packets to Server A and
  //     Server A stores them.
  //   * Server B only comes online when the two servers want to compute
  //     the final aggregate statistics.
  P_CHECKA (sA = PrioServer_new (cfg, PRIO_SERVER_A, skA, server_secret));
  P_CHECKA (sB = PrioServer_new (cfg, PRIO_SERVER_B, skB, server_secret));

  // Initialize empty verifier objects
  P_CHECKA (vA = PrioVerifier_new (sA)); 
  P_CHECKA (vB = PrioVerifier_new (sB));

  // Initialize shares of final aggregate statistics
  P_CHECKA (tA = PrioTotalShare_new ());
  P_CHECKA (tB = PrioTotalShare_new ());

  // Initialize shares of verification packets
  P_CHECKA (p1A = PrioPacketVerify1_new ());
  P_CHECKA (p1B = PrioPacketVerify1_new ());
  P_CHECKA (p2A = PrioPacketVerify2_new ());
  P_CHECKA (p2B = PrioPacketVerify2_new ());

  // I. CLIENT DATA SUBMISSION.
  //
  // Read in the client data packets
  unsigned int aLen = 0, bLen = 0;

  puts ("Reading...");
  P_CHECKC (read_browser_reply (shell, &for_server_a, &aLen, &for_server_b, &bLen));
  printf ("Read reply from browser. LenA: %u, LenB: %u\n", aLen, bLen);

  // II. VALIDATION PROTOCOL. (at servers)
  //
  // The servers now run a short 2-step protocol to check each 
  // client's packet:
  //    1) Servers A and B broadcast one message (PrioPacketVerify1) 
  //       to each other.
  //    2) Servers A and B broadcast another message (PrioPacketVerify2)
  //       to each other.
  //    3) Servers A and B can both determine whether the client's data
  //       submission is well-formed (in which case they add it to their
  //       running total of aggregate statistics) or ill-formed
  //       (in which case they ignore it).
  // These messages must be sent over an authenticated channel, so
  // that each server is assured that every received message came 
  // from its peer.

  // Set up a Prio verifier object.
  P_CHECKC (PrioVerifier_set_data (vA, for_server_a, aLen));
  P_CHECKC (PrioVerifier_set_data (vB, for_server_b, bLen));
  puts("Imported data.");

  // Both servers produce a packet1. Server A sends p1A to Server B
  // and vice versa.
  P_CHECKC (PrioPacketVerify1_set_data (p1A, vA));
  P_CHECKC (PrioPacketVerify1_set_data (p1B, vB));
  puts("Set data.");

  // Both servers produce a packet2. Server A sends p2A to Server B
  // and vice versa.
  P_CHECKC (PrioPacketVerify2_set_data(p2A, vA, p1A, p1B));
  P_CHECKC (PrioPacketVerify2_set_data(p2B, vB, p1A, p1B));

  // Using p2A and p2B, the servers can determine whether the request
  // is valid. (In fact, only Server A needs to perform this 
  // check, since Server A can just tell Server B whether the check 
  // succeeded or failed.) 
  puts ("Checking validity.");
  P_CHECKC (PrioVerifier_isValid (vA, p2A, p2B)); 
  P_CHECKC (PrioVerifier_isValid (vB, p2A, p2B)); 
  puts ("Are valid.");

  // If we get here, the client packet is valid, so add it to the aggregate
  // statistic counter for both servers.
  P_CHECKC (PrioServer_aggregate (sA, vA));
  P_CHECKC (PrioServer_aggregate (sB, vB));

  // The servers repeat the steps above for each client submission.

  // III. PRODUCTION OF AGGREGATE STATISTICS.
  //
  // After collecting aggregates from MANY clients, the servers can compute
  // their shares of the aggregate statistics. 
  //
  // Server B can send tB to Server A.
  P_CHECKC (PrioTotalShare_set_data (tA, sA));
  P_CHECKC (PrioTotalShare_set_data (tB, sB));

  // Once Server A has tA and tB, it can learn the aggregate statistics
  // in the clear.
  P_CHECKC (PrioTotalShare_final (cfg, output, tA, tB));
 
  for (int i=0; i < ndata; i++) {
    //printf("output[%d] = %lu\n", i, output[i]);
    //printf("data[%d] = %d\n", i, (int)data_items[i]);
    P_CHECKCB (output[i] == data_items[i]);
  }

  puts ("Success!");

cleanup:
  if (rv != SECSuccess) {
    fprintf (stderr, "Warning: unexpected failure.\n");
  }

  if (for_server_a) free (for_server_a);
  if (for_server_b) free (for_server_b);

  PrioTotalShare_clear (tA);
  PrioTotalShare_clear (tB);

  PrioPacketVerify2_clear (p2A);
  PrioPacketVerify2_clear (p2B);

  PrioPacketVerify1_clear (p1A);
  PrioPacketVerify1_clear (p1B);

  PrioVerifier_clear (vA);
  PrioVerifier_clear (vB);

  PrioServer_clear (sA);
  PrioServer_clear (sB);
  PrioConfig_clear (cfg);

  PublicKey_clear (pkA);
  PublicKey_clear (pkB);

  PrivateKey_clear (skA);
  PrivateKey_clear (skB);

  Prio_clear ();

  return !(rv == SECSuccess);
}

int 
main (int argc, char **argv)
{
  puts("== Prio browser test utility. ==");
  puts("(Note: Expects to be run in the same directory as encode-once.js.)");
  if (argc != 2) {
    fprintf (stderr, "Usage ./%s <path_to_xpcshell>\n", argv[0]);
    return 1;
  }

  return verify_full (argv[1], strlen (argv[1]));
}


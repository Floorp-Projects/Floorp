/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mprio.h>
#include <stdio.h>
#include <stdlib.h>

#include "prio/util.h"

int
verify_full (void)
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

  unsigned char *for_server_a = NULL;
  unsigned char *for_server_b = NULL;

  const unsigned char *batch_id = (unsigned char *)"prio_batch_2018-04-17";
  const unsigned int batch_id_len = strlen ((char *)batch_id);

  // Initialize NSS random number generator.
  P_CHECKC (Prio_init ());

  // Number of different boolean data fields we collect.
  const int ndata = 100;

  // Number of clients to simulate.
  const int nclients = 10;


  // New scope to avoid goto weirdness
  {
    bool data_items[ndata];

    // Generate keypairs for servers
    P_CHECKC (Keypair_new (&skA, &pkA));
    P_CHECKC (Keypair_new (&skB, &pkB));

    // Use the default configuration parameters.
    P_CHECKA (cfg = PrioConfig_new (ndata, pkA, pkB, batch_id, batch_id_len));

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

    // Generate client data packets.
    for (int c=0; c < nclients; c++) {

      // The client's data submission is an arbitrary boolean vector.
      for (int i=0; i < ndata; i++) {
        // Arbitrary data
        data_items[i] = (i % 3 == 1) || (c % 5 == 3);
      }

      // I. CLIENT DATA SUBMISSION.
      //
      // Construct the client data packets.
      unsigned int aLen, bLen;
      P_CHECKC (PrioClient_encode (cfg, data_items, 
            &for_server_a, &aLen, &for_server_b, &bLen));

      // The Prio servers A and B can come online later (e.g., at the end of
      // each day) to download the encrypted telemetry packets from the
      // telemetry server and run the protocol that computes the aggregate
      // statistics. In this way, the client only needs to send a 
      // single message (the pair of encrypted ClientPacketData packets) 
      // to a single server (the telemetry-data-collection server).

      // THE CLIENT'S JOB IS DONE. The rest of the processing just takes place
      // between the two servers A and B.


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

      // Both servers produce a packet1. Server A sends p1A to Server B
      // and vice versa.
      P_CHECKC (PrioPacketVerify1_set_data (p1A, vA));
      P_CHECKC (PrioPacketVerify1_set_data (p1B, vB));

      // Both servers produce a packet2. Server A sends p2A to Server B
      // and vice versa.
      P_CHECKC (PrioPacketVerify2_set_data(p2A, vA, p1A, p1B));
      P_CHECKC (PrioPacketVerify2_set_data(p2B, vB, p1A, p1B));

      // Using p2A and p2B, the servers can determine whether the request
      // is valid. (In fact, only Server A needs to perform this 
      // check, since Server A can just tell Server B whether the check 
      // succeeded or failed.) 
      P_CHECKC (PrioVerifier_isValid (vA, p2A, p2B)); 
      P_CHECKC (PrioVerifier_isValid (vB, p2A, p2B)); 

      // If we get here, the client packet is valid, so add it to the aggregate
      // statistic counter for both servers.
      P_CHECKC (PrioServer_aggregate (sA, vA));
      P_CHECKC (PrioServer_aggregate (sB, vB));

      free (for_server_a);
      free (for_server_b);
      for_server_a = NULL;
      for_server_b = NULL;
    }

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
    unsigned long output[ndata];
    P_CHECKC (PrioTotalShare_final (cfg, output, tA, tB));
    
    // Now the output[i] contains a counter that indicates how many clients
    // submitted TRUE for data value i.  We print out this data.
    for (int i=0; i < ndata; i++) 
      printf("output[%d] = %lu\n", i, output[i]);
  }

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
main (void)
{
  puts ("This utility demonstrates how to invoke the Prio API.");
  return verify_full ();
}


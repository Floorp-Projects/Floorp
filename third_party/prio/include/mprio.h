/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PRIO_H__
#define __PRIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <blapit.h>
#include <msgpack.h>
#include <pk11pub.h>
#include <seccomon.h>
#include <stdbool.h>
#include <stddef.h>

/* Seed for a pseudo-random generator (PRG). */
#define PRG_SEED_LENGTH AES_128_KEY_LENGTH
typedef unsigned char PrioPRGSeed[PRG_SEED_LENGTH];

/* Length of a raw curve25519 public key, in bytes. */
#define CURVE25519_KEY_LEN 32

/* Length of a hex-encoded curve25519 public key, in bytes. */
#define CURVE25519_KEY_LEN_HEX 64

/*
 * Type for each of the two servers.
 */
typedef enum { PRIO_SERVER_A, PRIO_SERVER_B } PrioServerId;

/*
 * Opaque types
 */
typedef struct prio_config* PrioConfig;
typedef const struct prio_config* const_PrioConfig;

typedef struct prio_server* PrioServer;
typedef const struct prio_server* const_PrioServer;

typedef struct prio_verifier* PrioVerifier;
typedef const struct prio_verifier* const_PrioVerifier;

typedef struct prio_packet_verify1* PrioPacketVerify1;
typedef const struct prio_packet_verify1* const_PrioPacketVerify1;

typedef struct prio_packet_verify2* PrioPacketVerify2;
typedef const struct prio_packet_verify2* const_PrioPacketVerify2;

typedef struct prio_total_share* PrioTotalShare;
typedef const struct prio_total_share* const_PrioTotalShare;

typedef SECKEYPublicKey* PublicKey;
typedef const SECKEYPublicKey* const_PublicKey;

typedef SECKEYPrivateKey* PrivateKey;
typedef const SECKEYPrivateKey* const_PrivateKey;

/*
 * Initialize and clear random number generator state.
 * You must call Prio_init() before using the library.
 * To avoid memory leaks, call Prio_clear() afterwards.
 */
SECStatus Prio_init();
void Prio_clear();

/*
 * PrioConfig holds the system parameters. The two relevant things determined
 * by the config object are:
 *    (1) the number of data fields we are collecting, and
 *    (2) the modulus we use for modular arithmetic.
 * The default configuration uses an 87-bit modulus.
 *
 * The `batch_id` field specifies which "batch" of aggregate statistics we are
 * computing. For example, if the aggregate statistics are computed every 24
 * hours, the `batch_id` might be set to an encoding of the date. The clients
 * and servers must all use the same `batch_id` for each run of the protocol.
 * Each set of aggregate statistics should use a different `batch_id`.
 *
 * `PrioConfig_new` does not keep a pointer to the `batch_id` string that the
 * caller passes in, so you may free the `batch_id` string as soon as
 * `PrioConfig_new` returns.
 */
PrioConfig PrioConfig_new(int n_fields, PublicKey server_a, PublicKey server_b,
                          const unsigned char* batch_id,
                          unsigned int batch_id_len);
void PrioConfig_clear(PrioConfig cfg);
int PrioConfig_numDataFields(const_PrioConfig cfg);

/*
 * Create a PrioConfig object with no encryption keys.  This routine is
 * useful for testing, but PrioClient_encode() will always fail when used with
 * this config.
 */
PrioConfig PrioConfig_newTest(int n_fields);

/*
 * We use the PublicKey and PrivateKey objects for public-key encryption. Each
 * Prio server has an associated public key, and the clients use these keys to
 * encrypt messages to the servers.
 */
SECStatus Keypair_new(PrivateKey* pvtkey, PublicKey* pubkey);

/*
 * Import a new curve25519 public key from the raw bytes given. The key passed
 * in
 * as `data` should be of length `CURVE25519_KEY_LEN`. This function allocates
 * a new PublicKey object, which the caller must free using `PublicKey_clear`.
 */
SECStatus PublicKey_import(PublicKey* pk, const unsigned char* data,
                           unsigned int dataLen);

/*
 * Import a new curve25519 public key from a hex string that contains only the
 * characters 0-9a-fA-F. The hex string passed in as `hex_data` should be of
 * length `CURVE25519_KEY_LEN_HEX`. This function allocates a new PublicKey
 * object, which the caller must free using `PublicKey_clear`.
 */
SECStatus PublicKey_import_hex(PublicKey* pk, const unsigned char* hex_data,
                               unsigned int dataLen);

/*
 * Export a curve25519 public key as a raw byte-array.
 */
SECStatus PublicKey_export(const_PublicKey pk,
                           unsigned char data[CURVE25519_KEY_LEN]);

/*
 * Export a curve25519 public key as a NULL-terminated hex string.
 */
SECStatus PublicKey_export_hex(const_PublicKey pk,
                               unsigned char data[CURVE25519_KEY_LEN_HEX + 1]);

void PublicKey_clear(PublicKey pubkey);
void PrivateKey_clear(PrivateKey pvtkey);

/*
 *  PrioPacketClient_encode
 *
 * Takes as input a pointer to an array (`data_in`) of boolean values
 * whose length is equal to the number of data fields specified in
 * the config. It then encodes the data for servers A and B into a
 * string.
 *
 * NOTE: The caller must free() the strings `for_server_a` and
 * `for_server_b` to avoid memory leaks.
 */
SECStatus PrioClient_encode(const_PrioConfig cfg, const bool* data_in,
                            unsigned char** for_server_a, unsigned int* aLen,
                            unsigned char** for_server_b, unsigned int* bLen);

/*
 * Generate a new PRG seed using the NSS global randomness source.
 * Use this routine to initialize the secret that the two Prio servers
 * share.
 */
SECStatus PrioPRGSeed_randomize(PrioPRGSeed* seed);

/*
 * The PrioServer object holds the state of the Prio servers.
 * Pass in the _same_ secret PRGSeed when initializing the two servers.
 * The PRGSeed must remain secret to the two servers.
 */
PrioServer PrioServer_new(const_PrioConfig cfg, PrioServerId server_idx,
                          PrivateKey server_priv,
                          const PrioPRGSeed server_shared_secret);
void PrioServer_clear(PrioServer s);

/*
 * After receiving a client packet, each of the servers generate
 * a PrioVerifier object that they use to check whether the client's
 * encoded packet is well formed.
 */
PrioVerifier PrioVerifier_new(PrioServer s);
void PrioVerifier_clear(PrioVerifier v);

/*
 * Read in encrypted data from the client, decrypt it, and prepare to check the
 * request for validity.
 */
SECStatus PrioVerifier_set_data(PrioVerifier v, unsigned char* data,
                                unsigned int dataLen);

/*
 * Generate the first packet that servers need to exchange to verify the
 * client's submission. This should be sent over a TLS connection between the
 * servers.
 */
PrioPacketVerify1 PrioPacketVerify1_new(void);
void PrioPacketVerify1_clear(PrioPacketVerify1 p1);

SECStatus PrioPacketVerify1_set_data(PrioPacketVerify1 p1,
                                     const_PrioVerifier v);

SECStatus PrioPacketVerify1_write(const_PrioPacketVerify1 p,
                                  msgpack_packer* pk);
SECStatus PrioPacketVerify1_read(PrioPacketVerify1 p, msgpack_unpacker* upk,
                                 const_PrioConfig cfg);

/*
 * Generate the second packet that the servers need to exchange to verify the
 * client's submission. The routine takes as input the PrioPacketVerify1
 * packets from both server A and server B.
 *
 * This should be sent over a TLS connection between the servers.
 */
PrioPacketVerify2 PrioPacketVerify2_new(void);
void PrioPacketVerify2_clear(PrioPacketVerify2 p);

SECStatus PrioPacketVerify2_set_data(PrioPacketVerify2 p2, const_PrioVerifier v,
                                     const_PrioPacketVerify1 p1A,
                                     const_PrioPacketVerify1 p1B);

SECStatus PrioPacketVerify2_write(const_PrioPacketVerify2 p,
                                  msgpack_packer* pk);
SECStatus PrioPacketVerify2_read(PrioPacketVerify2 p, msgpack_unpacker* upk,
                                 const_PrioConfig cfg);

/*
 * Use the PrioPacketVerify2s from both servers to check whether
 * the client's submission is well formed.
 */
SECStatus PrioVerifier_isValid(const_PrioVerifier v, const_PrioPacketVerify2 pA,
                               const_PrioPacketVerify2 pB);

/*
 * Each of the two servers calls this routine to aggregate the data
 * submission from a client that is included in the PrioVerifier object.
 *
 * IMPORTANT: This routine does *not* check the validity of the client's
 * data packet. The servers must execute the verification checks
 * above before aggregating any client data.
 */
SECStatus PrioServer_aggregate(PrioServer s, PrioVerifier v);

/*
 * After the servers have aggregated data packets from "enough" clients
 * (this determines the anonymity set size), each server runs this routine
 * to get a share of the aggregate statistics.
 */
PrioTotalShare PrioTotalShare_new(void);
void PrioTotalShare_clear(PrioTotalShare t);

SECStatus PrioTotalShare_set_data(PrioTotalShare t, const_PrioServer s);

SECStatus PrioTotalShare_write(const_PrioTotalShare t, msgpack_packer* pk);
SECStatus PrioTotalShare_read(PrioTotalShare t, msgpack_unpacker* upk,
                              const_PrioConfig cfg);

/*
 * Read the output data into an array of unsigned longs. You should
 * be sure that each data value can fit into a single long and that
 * the pointer `output` points to a buffer large enough to store
 * one long per data field.
 */
SECStatus PrioTotalShare_final(const_PrioConfig cfg, unsigned long* output,
                               const_PrioTotalShare tA,
                               const_PrioTotalShare tB);

#endif /* __PRIO_H__ */

#ifdef __cplusplus
}
#endif

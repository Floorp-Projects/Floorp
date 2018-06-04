/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _RUSTSDPINC_H_
#define _RUSTSDPINC_H_

#include "nsError.h"

#include <stdint.h>
#include <stdbool.h>

struct BandwidthVec;
struct RustSdpSession;
struct RustSdpError;
struct RustMediaSection;
struct RustAttributeList;
struct StringVec;
struct U32Vec;
struct RustHeapString;

enum class RustSdpAddrType {
  kRustAddrNone,
  kRustAddrIp4,
  kRustAddrIp6
};

struct RustIpAddr {
  RustSdpAddrType addrType;
  char unicastAddr[50];
};

struct StringView {
  char* buf;
  size_t len;
};

struct RustSdpConnection {
  RustIpAddr addr;
  uint8_t ttl;
  uint64_t amount;
};

struct RustSdpOrigin {
  StringView username;
  uint64_t sessionId;
  uint64_t sessionVersion;
  RustIpAddr addr;
};

enum class RustSdpMediaValue {
  kRustAudio,
  kRustVideo,
  kRustApplication
};

enum class RustSdpProtocolValue {
  kRustRtpSavpf,
  kRustUdpTlsRtpSavpf,
  kRustTcpTlsRtpSavpf,
  kRustDtlsSctp,
  kRustUdpDtlsSctp,
  kRustTcpDtlsSctp,
};

enum class RustSdpFormatType {
  kRustIntegers,
  kRustStrings
};

struct RustSdpAttributeFingerprint {
  StringView hashAlgorithm;
  StringView fingerprint;
};

enum class RustSdpSetup {
  kRustActive,
  kRustActpass,
  kRustHoldconn,
  kRustPassive
};

struct RustSdpAttributeSsrc {
  uint32_t id;
  StringView attribute;
  StringView value;
};

struct RustSdpAttributeRtpmap {
  uint8_t payloadType;
  StringView codecName;
  uint32_t frequency;
  uint32_t channels;
};

struct RustSdpAttributeRtcpFb {
  uint32_t payloadType;
  uint32_t feedbackType;
  StringView parameter;
  StringView extra;
};

struct RustSdpAttributeFmtp {
  uint8_t payloadType;
  StringView codecName;
  StringVec* tokens;
};

struct RustSdpAttributeFlags {
  bool iceLite;
  bool rtcpMux;
  bool bundleOnly;
  bool endOfCandidates;
};

struct RustSdpAttributeMsid {
  StringView id;
  StringView appdata;
};

struct RustSdpAttributeMsidSemantic {
  StringView semantic;
  StringVec* msids;
};

enum class RustSdpAttributeGroupSemantic {
  kRustLipSynchronization,
  kRustFlowIdentification,
  kRustSingleReservationFlow,
  kRustAlternateNetworkAddressType,
  kRustForwardErrorCorrection,
  kRustDecodingDependency,
  kRustBundle,
};

struct RustSdpAttributeGroup {
  RustSdpAttributeGroupSemantic semantic;
  StringVec* tags;
};

struct RustSdpAttributeRtcp {
  uint32_t port;
  RustIpAddr unicastAddr;
};

struct RustSdpAttributeSctpmap {
  uint32_t port;
  uint32_t channels;
};

enum class RustDirection {
  kRustRecvonly,
  kRustSendonly,
  kRustSendrecv,
  kRustInactive
};

struct RustSdpAttributeRemoteCandidate {
  uint32_t component;
  RustIpAddr address;
  uint32_t port;
};

// TODO: Add field indicating whether direction was specified
// See Bug 1438536.
struct RustSdpAttributeExtmap {
  uint16_t id;
  RustDirection direction;
  StringView url;
  StringView extensionAttributes;
};

extern "C" {

size_t string_vec_len(const StringVec* vec);
nsresult string_vec_get_view(const StringVec* vec, size_t index,
                             StringView* str);

size_t u32_vec_len(const U32Vec* vec);
nsresult u32_vec_get(const U32Vec* vec, size_t index, uint32_t* ret);

void sdp_free_string(char* string);

nsresult parse_sdp(const char* sdp, uint32_t length, bool fail_on_warning,
                   RustSdpSession** ret, RustSdpError** err);
RustSdpSession* sdp_new_reference(RustSdpSession* aSess);
void sdp_free_session(RustSdpSession* ret);
size_t sdp_get_error_line_num(const RustSdpError* err);
StringView sdp_get_error_message(const RustSdpError* err);
void sdp_free_error(RustSdpError* err);

RustSdpOrigin sdp_get_origin(const RustSdpSession* aSess);

uint32_t get_sdp_bandwidth(const RustSdpSession* aSess,
                           const char* aBandwidthType);
BandwidthVec* sdp_get_session_bandwidth_vec(const RustSdpSession* aSess);
BandwidthVec* sdp_get_media_bandwidth_vec(const RustMediaSection* aMediaSec);
char* sdp_serialize_bandwidth(const BandwidthVec* bandwidths);
bool sdp_session_has_connection(const RustSdpSession* aSess);
nsresult sdp_get_session_connection(const RustSdpSession* aSess,
                                    RustSdpConnection* ret);
RustAttributeList* get_sdp_session_attributes(const RustSdpSession* aSess);

size_t sdp_media_section_count(const RustSdpSession* aSess);
RustMediaSection* sdp_get_media_section(const RustSdpSession* aSess,
                                        size_t aLevel);
RustSdpMediaValue sdp_rust_get_media_type(const RustMediaSection* aMediaSec);
RustSdpProtocolValue
sdp_get_media_protocol(const RustMediaSection* aMediaSec);
RustSdpFormatType sdp_get_format_type(const RustMediaSection* aMediaSec);
StringVec* sdp_get_format_string_vec(const RustMediaSection* aMediaSec);
U32Vec* sdp_get_format_u32_vec(const RustMediaSection* aMediaSec);
void sdp_set_media_port(const RustMediaSection* aMediaSec, uint32_t aPort);
uint32_t sdp_get_media_port(const RustMediaSection* aMediaSec);
uint32_t sdp_get_media_port_count(const RustMediaSection* aMediaSec);
uint32_t sdp_get_media_bandwidth(const RustMediaSection* aMediaSec,
                                 const char* aBandwidthType);
bool sdp_media_has_connection(const RustMediaSection* aMediaSec);
nsresult sdp_get_media_connection(const RustMediaSection* aMediaSec,
                                  RustSdpConnection* ret);

RustAttributeList*
sdp_get_media_attribute_list(const RustMediaSection* aMediaSec);

nsresult sdp_get_iceufrag(const RustAttributeList* aList, StringView* ret);
nsresult sdp_get_icepwd(const RustAttributeList* aList, StringView* ret);
nsresult sdp_get_identity(const RustAttributeList* aList, StringView* ret);
nsresult sdp_get_iceoptions(const RustAttributeList* aList, StringVec** ret);

size_t sdp_get_fingerprint_count(const RustAttributeList* aList);
void sdp_get_fingerprints(const RustAttributeList* aList, size_t listSize,
                          RustSdpAttributeFingerprint* ret);

nsresult sdp_get_setup(const RustAttributeList* aList, RustSdpSetup* ret);

size_t sdp_get_ssrc_count(const RustAttributeList* aList);
void sdp_get_ssrcs(const RustAttributeList* aList,
                   size_t listSize, RustSdpAttributeSsrc* ret);

size_t sdp_get_rtpmap_count(const RustAttributeList* aList);
void sdp_get_rtpmaps(const RustAttributeList* aList, size_t listSize,
                     RustSdpAttributeRtpmap* ret);

size_t sdp_get_fmtp_count(const RustAttributeList* aList);
size_t sdp_get_fmtp(const RustAttributeList* aList, size_t listSize,
                    RustSdpAttributeFmtp* ret);

int64_t sdp_get_ptime(const RustAttributeList* aList);

RustSdpAttributeFlags sdp_get_attribute_flags(const RustAttributeList* aList);

nsresult sdp_get_mid(const RustAttributeList* aList, StringView* ret);

size_t sdp_get_msid_count(const RustAttributeList* aList);
void sdp_get_msids(const RustAttributeList* aList, size_t listSize,
                   RustSdpAttributeMsid* ret);

size_t sdp_get_msid_semantic_count(RustAttributeList* aList);
void sdp_get_msid_semantics(const RustAttributeList* aList, size_t listSize,
                            RustSdpAttributeMsidSemantic* ret);

size_t sdp_get_group_count(const RustAttributeList* aList);
nsresult sdp_get_groups(const RustAttributeList* aList, size_t listSize,
                        RustSdpAttributeGroup* ret);

nsresult sdp_get_rtcp(const RustAttributeList* aList,
                      RustSdpAttributeRtcp* ret);


size_t sdp_get_rtcpfb_count(const RustAttributeList* aList);
void sdp_get_rtcpfbs(const RustAttributeList* aList, size_t listSize,
                     RustSdpAttributeRtcpFb* ret);

size_t sdp_get_imageattr_count(const RustAttributeList* aList);
void sdp_get_imageattrs(const RustAttributeList* aList, size_t listSize,
                        StringView* ret);

size_t sdp_get_sctpmap_count(const RustAttributeList* aList);
void sdp_get_sctpmaps(const RustAttributeList* aList, size_t listSize,
                      RustSdpAttributeSctpmap* ret);

RustDirection sdp_get_direction(const RustAttributeList* aList);


size_t sdp_get_remote_candidate_count(const RustAttributeList* aList);
void sdp_get_remote_candidates(const RustAttributeList* aList,
                               size_t listSize,
                               RustSdpAttributeRemoteCandidate* ret);

size_t sdp_get_rid_count(const RustAttributeList* aList);
void sdp_get_rids(const RustAttributeList* aList, size_t listSize,
                  StringView* ret);


size_t sdp_get_extmap_count(const RustAttributeList* aList);
void sdp_get_extmaps(const RustAttributeList* aList, size_t listSize,
                     RustSdpAttributeExtmap* ret);

} //extern "C"

#endif

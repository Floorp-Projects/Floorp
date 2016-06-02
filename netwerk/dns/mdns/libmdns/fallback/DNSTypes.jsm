/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext: true, moz: true */

'use strict';

this.EXPORTED_SYMBOLS = [
  'DNS_QUERY_RESPONSE_CODES',
  'DNS_AUTHORITATIVE_ANSWER_CODES',
  'DNS_CLASS_CODES',
  'DNS_RECORD_TYPES'
];

let DNS_QUERY_RESPONSE_CODES = {
  QUERY           : 0,      // RFC 1035 - Query
  RESPONSE        : 1       // RFC 1035 - Reponse
};

let DNS_AUTHORITATIVE_ANSWER_CODES = {
  NO              : 0,      // RFC 1035 - Not Authoritative
  YES             : 1       // RFC 1035 - Is Authoritative
};

let DNS_CLASS_CODES = {
  IN              : 0x01,   // RFC 1035 - Internet
  CS              : 0x02,   // RFC 1035 - CSNET
  CH              : 0x03,   // RFC 1035 - CHAOS
  HS              : 0x04,   // RFC 1035 - Hesiod
  NONE            : 0xfe,   // RFC 2136 - None
  ANY             : 0xff,   // RFC 1035 - Any
};

let DNS_RECORD_TYPES = {
  SIGZERO         : 0,      // RFC 2931
  A               : 1,      // RFC 1035
  NS              : 2,      // RFC 1035
  MD              : 3,      // RFC 1035
  MF              : 4,      // RFC 1035
  CNAME           : 5,      // RFC 1035
  SOA             : 6,      // RFC 1035
  MB              : 7,      // RFC 1035
  MG              : 8,      // RFC 1035
  MR              : 9,      // RFC 1035
  NULL            : 10,     // RFC 1035
  WKS             : 11,     // RFC 1035
  PTR             : 12,     // RFC 1035
  HINFO           : 13,     // RFC 1035
  MINFO           : 14,     // RFC 1035
  MX              : 15,     // RFC 1035
  TXT             : 16,     // RFC 1035
  RP              : 17,     // RFC 1183
  AFSDB           : 18,     // RFC 1183
  X25             : 19,     // RFC 1183
  ISDN            : 20,     // RFC 1183
  RT              : 21,     // RFC 1183
  NSAP            : 22,     // RFC 1706
  NSAP_PTR        : 23,     // RFC 1348
  SIG             : 24,     // RFC 2535
  KEY             : 25,     // RFC 2535
  PX              : 26,     // RFC 2163
  GPOS            : 27,     // RFC 1712
  AAAA            : 28,     // RFC 1886
  LOC             : 29,     // RFC 1876
  NXT             : 30,     // RFC 2535
  EID             : 31,     // RFC ????
  NIMLOC          : 32,     // RFC ????
  SRV             : 33,     // RFC 2052
  ATMA            : 34,     // RFC ????
  NAPTR           : 35,     // RFC 2168
  KX              : 36,     // RFC 2230
  CERT            : 37,     // RFC 2538
  DNAME           : 39,     // RFC 2672
  OPT             : 41,     // RFC 2671
  APL             : 42,     // RFC 3123
  DS              : 43,     // RFC 4034
  SSHFP           : 44,     // RFC 4255
  IPSECKEY        : 45,     // RFC 4025
  RRSIG           : 46,     // RFC 4034
  NSEC            : 47,     // RFC 4034
  DNSKEY          : 48,     // RFC 4034
  DHCID           : 49,     // RFC 4701
  NSEC3           : 50,     // RFC ????
  NSEC3PARAM      : 51,     // RFC ????
  HIP             : 55,     // RFC 5205
  SPF             : 99,     // RFC 4408
  UINFO           : 100,    // RFC ????
  UID             : 101,    // RFC ????
  GID             : 102,    // RFC ????
  UNSPEC          : 103,    // RFC ????
  TKEY            : 249,    // RFC 2930
  TSIG            : 250,    // RFC 2931
  IXFR            : 251,    // RFC 1995
  AXFR            : 252,    // RFC 1035
  MAILB           : 253,    // RFC 1035
  MAILA           : 254,    // RFC 1035
  ANY             : 255,    // RFC 1035
  DLV             : 32769   // RFC 4431
};

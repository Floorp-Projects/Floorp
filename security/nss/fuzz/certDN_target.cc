/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "shared.h"

#define TEST_FUNCTION(f) \
  out = f(certName);     \
  free(out);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string name(data, data + size);

  assert(SECOID_Init() == SECSuccess);

  CERTName* certName = CERT_AsciiToName(name.c_str());
  if (certName) {
    char* out;
    TEST_FUNCTION(CERT_NameToAscii)
    TEST_FUNCTION(CERT_GetCertEmailAddress)

    // These functions call CERT_GetNameElement with different OIDs.
    // Unfotunately CERT_GetNameElement is not accesible from here.
    TEST_FUNCTION(CERT_GetCertUid)
    TEST_FUNCTION(CERT_GetCommonName)
    TEST_FUNCTION(CERT_GetCountryName)
    TEST_FUNCTION(CERT_GetDomainComponentName)
    TEST_FUNCTION(CERT_GetLocalityName)
    TEST_FUNCTION(CERT_GetOrgName)
    TEST_FUNCTION(CERT_GetOrgUnitName)
    TEST_FUNCTION(CERT_GetStateName)

    out = CERT_NameToAsciiInvertible(certName, CERT_N2A_READABLE);
    free(out);
    out = CERT_NameToAsciiInvertible(certName, CERT_N2A_STRICT);
    free(out);
    out = CERT_NameToAsciiInvertible(certName, CERT_N2A_INVERTIBLE);
    free(out);
  }
  CERT_DestroyName(certName);

  return 0;
}

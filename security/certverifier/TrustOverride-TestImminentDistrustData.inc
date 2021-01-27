// Script from security/manager/tools/crtshToIdentifyingStruct/crtshToIdentifyingStruct.py
// Invocation: crtshToIdentifyingStruct.py -dn -listname TestImminentDistrustEndEntityDNs ../../ssl/tests/unit/bad_certs/ee-imminently-distrusted.pem

// This file is used by test_imminent_distrust.js and by
// browser_console_certificate_imminent_distrust.js to ensure that the UI for
// alerting users to an upcoming CA distrust action continues to function.

// /CN=Imminently Distrusted End Entity
// SHA256 Fingerprint: DB:4E:B0:BA:38:93:02:E8:32:87:03:FA:C3:C8:F7:6A
//                     88:77:03:B2:9E:CE:C0:C8:27:26:CC:8F:F5:64:E6:B5
static const uint8_t CAImminentlyDistrustedEndEntityDN[45] = {
  0x30, 0x2B, 0x31, 0x29, 0x30, 0x27, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x20,
  0x49, 0x6D, 0x6D, 0x69, 0x6E, 0x65, 0x6E, 0x74, 0x6C, 0x79, 0x20, 0x44, 0x69,
  0x73, 0x74, 0x72, 0x75, 0x73, 0x74, 0x65, 0x64, 0x20, 0x45, 0x6E, 0x64, 0x20,
  0x45, 0x6E, 0x74, 0x69, 0x74, 0x79,
};

static const DataAndLength TestImminentDistrustEndEntityDNs[]= {
  { CAImminentlyDistrustedEndEntityDN,
    sizeof(CAImminentlyDistrustedEndEntityDN) },
};

// Script from security/manager/tools/crtshToDNStruct/crtshToDNStruct.py
// Invocation: security/manager/tools/crtshToDNStruct/crtshToDNStruct.py security/manager/ssl/tests/unit/bad_certs/ee-imminently-distrusted.pem

// This file is used by test_imminent_distrust.js and by
// browser_console_certificate_imminent_distrust.js to ensure that the UI for
// alerting users to an upcoming CA distrust action continues to function.

// /C=US/CN=Imminently Distrusted End Entity
// SHA256 Fingerprint: 63:3A:70:8A:67:42:91:95:98:E9:D1:CB:8B:5D:73:80
//                     BA:6D:AD:25:82:62:52:AD:5E:5E:DC:06:BF:03:1F:D0
static const uint8_t CAImminentlyDistrustedEndEntityDN[58] = {
  0x30, 0x38, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
  0x55, 0x53, 0x31, 0x29, 0x30, 0x27, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x20,
  0x49, 0x6D, 0x6D, 0x69, 0x6E, 0x65, 0x6E, 0x74, 0x6C, 0x79, 0x20, 0x44, 0x69,
  0x73, 0x74, 0x72, 0x75, 0x73, 0x74, 0x65, 0x64, 0x20, 0x45, 0x6E, 0x64, 0x20,
  0x45, 0x6E, 0x74, 0x69, 0x74, 0x79,
};

static const DataAndLength TestImminentDistrustEndEntityDNs[]= {
  { CAImminentlyDistrustedEndEntityDN,
    sizeof(CAImminentlyDistrustedEndEntityDN) },
};

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "TransportSecurityInfo.h"
#include "nsCOMPtr.h"
#include "nsITransportSecurityInfo.h"
#include "nsIX509Cert.h"
#include "nsString.h"
#include "mozilla/Maybe.h"

using namespace mozilla;
using namespace mozilla::psm;

// nsITransportSecurityInfo de-serializatin tests
//
// These tests verify that we can still deserialize old binary strings
// generated for security info.  This is necessary because service workers
// stores these strings on disk.
//
// If you make a change and start breaking these tests, you will need to
// add a compat fix for loading the old versions.  For things that affect
// the UUID, but do not break the rest of the format you can simply add
// another hack condition in nsBinaryInputStream::ReadObject().  If you
// change the overall format of the serialization then we will need more
// complex handling in the security info concrete classes.
//
// We would like to move away from this binary compatibility requirement
// in service workers.  See bug 1248628.
void deserializeAndVerify(const nsCString& serializedSecInfo,
                          Maybe<size_t> failedCertChainLength = Nothing(),
                          Maybe<size_t> succeededCertChainLength = Nothing()) {
  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  nsresult rv = TransportSecurityInfo::Read(serializedSecInfo,
                                            getter_AddRefs(securityInfo));
  ASSERT_EQ(NS_OK, rv);
  ASSERT_TRUE(securityInfo);

  nsCOMPtr<nsIX509Cert> cert;
  rv = securityInfo->GetServerCert(getter_AddRefs(cert));
  ASSERT_EQ(NS_OK, rv);
  ASSERT_TRUE(cert);

  nsTArray<RefPtr<nsIX509Cert>> failedCertArray;
  rv = securityInfo->GetFailedCertChain(failedCertArray);
  ASSERT_EQ(NS_OK, rv);

  if (failedCertChainLength) {
    ASSERT_FALSE(failedCertArray.IsEmpty());
    for (const auto& cert : failedCertArray) {
      ASSERT_TRUE(cert);
    }
    ASSERT_EQ(*failedCertChainLength, failedCertArray.Length());
  } else {
    ASSERT_TRUE(failedCertArray.IsEmpty());
  }

  nsTArray<RefPtr<nsIX509Cert>> succeededCertArray;
  rv = securityInfo->GetSucceededCertChain(succeededCertArray);
  ASSERT_EQ(NS_OK, rv);

  if (succeededCertChainLength) {
    ASSERT_FALSE(succeededCertArray.IsEmpty());
    for (const auto& cert : succeededCertArray) {
      ASSERT_TRUE(cert);
    }
    ASSERT_EQ(*succeededCertChainLength, succeededCertArray.Length());
  } else {
    ASSERT_TRUE(succeededCertArray.IsEmpty());
  }
}

TEST(psm_DeserializeCert, gecko33)
{
  // clang-format off
  // Gecko 33+ vintage Security info serialized with UUIDs:
  //  - nsISupports  00000000-0000-0000-c000-000000000046
  //  - nsISSLStatus fa9ba95b-ca3b-498a-b889-7c79cf28fee8
  //  - nsIX509Cert  f8ed8364-ced9-4c6e-86ba-48af53c393e6
  nsCString base64Serialization(
  "FnhllAKWRHGAlo+ESXykKAAAAAAAAAAAwAAAAAAAAEaphjojH6pBabDSgSnsfLHeAAQAAgAAAAAAAAAAAAAAAAAAAAA"
  "B4vFIJp5wRkeyPxAQ9RJGKPqbqVvKO0mKuIl8ec8o/uhmCjImkVxP+7sgiYWmMt8F+O2DZM7ZTG6GukivU8OT5gAAAAIAAAWpMII"
  "FpTCCBI2gAwIBAgIQD4svsaKEC+QtqtsU2TF8ITANBgkqhkiG9w0BAQsFADBwMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUN"
  "lcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMS8wLQYDVQQDEyZEaWdpQ2VydCBTSEEyIEhpZ2ggQXNzdXJhbmNlIFN"
  "lcnZlciBDQTAeFw0xNTAyMjMwMDAwMDBaFw0xNjAzMDIxMjAwMDBaMGoxCzAJBgNVBAYTAlVTMRYwFAYDVQQHEw1TYW4gRnJhbmN"
  "pc2NvMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRUwEwYDVQQKEwxGYXN0bHksIEluYy4xFzAVBgNVBAMTDnd3dy5naXRodWIuY29tMII"
  "BIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA+9WUCgrgUNwP/JC3cUefLAXeDpq8Ko/U8p8IRvny0Ri0I6Uq0t+RP/nF0LJ"
  "Avda8QHYujdgeDTePepBX7+OiwBFhA0YO+rM3C2Z8IRaN/i9eLln+Yyc68+1z+E10s1EXdZrtDGvN6MHqygGsdfkXKfBLUJ1BZEh"
  "s9sBnfcjq3kh5gZdBArdG9l5NpdmQhtceaFGsPiWuJxGxRzS4i95veUHWkhMpEYDEEBdcDGxqArvQCvzSlngdttQCfx8OUkBTb3B"
  "A2okpTwwJfqPsxVetA6qR7UNc+fVb6KHwvm0bzi2rQ3xw3D/syRHwdMkpoVDQPCk43H9WufgfBKRen87dFwIDAQABo4ICPzCCAjs"
  "wHwYDVR0jBBgwFoAUUWj/kK8CB3U8zNllZGKiErhZcjswHQYDVR0OBBYEFGS/RLNGCZvPWh1xSaIEcouINIQjMHsGA1UdEQR0MHK"
  "CDnd3dy5naXRodWIuY29tggpnaXRodWIuY29tggwqLmdpdGh1Yi5jb22CCyouZ2l0aHViLmlvgglnaXRodWIuaW+CFyouZ2l0aHV"
  "idXNlcmNvbnRlbnQuY29tghVnaXRodWJ1c2VyY29udGVudC5jb20wDgYDVR0PAQH/BAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwM"
  "BBggrBgEFBQcDAjB1BgNVHR8EbjBsMDSgMqAwhi5odHRwOi8vY3JsMy5kaWdpY2VydC5jb20vc2hhMi1oYS1zZXJ2ZXItZzMuY3J"
  "sMDSgMqAwhi5odHRwOi8vY3JsNC5kaWdpY2VydC5jb20vc2hhMi1oYS1zZXJ2ZXItZzMuY3JsMEIGA1UdIAQ7MDkwNwYJYIZIAYb"
  "9bAEBMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwgYMGCCsGAQUFBwEBBHcwdTAkBggrBgEFBQc"
  "wAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29tME0GCCsGAQUFBzAChkFodHRwOi8vY2FjZXJ0cy5kaWdpY2VydC5jb20vRGlnaUN"
  "lcnRTSEEySGlnaEFzc3VyYW5jZVNlcnZlckNBLmNydDAMBgNVHRMBAf8EAjAAMA0GCSqGSIb3DQEBCwUAA4IBAQAc4dbVmuKvyI7"
  "KZ4Txk+ZqcAYToJGKUIVaPL94e5SZGweUisjaCbplAOihnf6Mxt8n6vnuH2IsCaz2NRHqhdcosjT3CwAiJpJNkXPKWVL/txgdSTV"
  "2cqB1GG4esFOalvI52dzn+J4fTIYZvNF+AtGyHSLm2XRXYZCw455laUKf6Sk9RDShDgUvzhOKL4GXfTwKXv12MyMknJybH8UCpjC"
  "HZmFBVHMcUN/87HsQo20PdOekeEvkjrrMIxW+gxw22Yb67yF/qKgwrWr+43bLN709iyw+LWiU7sQcHL2xk9SYiWQDj2tYz2soObV"
  "QYTJm0VUZMEVFhtALq46cx92Zu4vFwC8AAwAAAAABAQAA");
  // clang-format on

  deserializeAndVerify(base64Serialization);
}

TEST(psm_DeserializeCert, gecko46)
{
  // clang-format off
  // Gecko 46+ vintage Security info serialized with UUIDs:
  //  - nsISupports  00000000-0000-0000-c000-000000000046
  //  - nsISSLStatus fa9ba95b-ca3b-498a-b889-7c79cf28fee8
  //  - nsIX509Cert  bdc3979a-5422-4cd5-8589-696b6e96ea83
  nsCString base64Serialization(
  "FnhllAKWRHGAlo+ESXykKAAAAAAAAAAAwAAAAAAAAEaphjojH6pBabDSgSnsfLHeAAQAAgAAAAAAAAAAAAAAAAAAAAA"
  "B4vFIJp5wRkeyPxAQ9RJGKPqbqVvKO0mKuIl8ec8o/uhmCjImkVxP+7sgiYWmMt8FvcOXmlQiTNWFiWlrbpbqgwAAAAIAAAWzMII"
  "FrzCCBJegAwIBAgIQB3pdwzYjAfmJ/lT3+G8+ZDANBgkqhkiG9w0BAQsFADBwMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUN"
  "lcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMS8wLQYDVQQDEyZEaWdpQ2VydCBTSEEyIEhpZ2ggQXNzdXJhbmNlIFN"
  "lcnZlciBDQTAeFw0xNjAxMjAwMDAwMDBaFw0xNzA0MDYxMjAwMDBaMGoxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybml"
  "hMRYwFAYDVQQHEw1TYW4gRnJhbmNpc2NvMRUwEwYDVQQKEwxGYXN0bHksIEluYy4xFzAVBgNVBAMTDnd3dy5naXRodWIuY29tMII"
  "BIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA+9WUCgrgUNwP/JC3cUefLAXeDpq8Ko/U8p8IRvny0Ri0I6Uq0t+RP/nF0LJ"
  "Avda8QHYujdgeDTePepBX7+OiwBFhA0YO+rM3C2Z8IRaN/i9eLln+Yyc68+1z+E10s1EXdZrtDGvN6MHqygGsdfkXKfBLUJ1BZEh"
  "s9sBnfcjq3kh5gZdBArdG9l5NpdmQhtceaFGsPiWuJxGxRzS4i95veUHWkhMpEYDEEBdcDGxqArvQCvzSlngdttQCfx8OUkBTb3B"
  "A2okpTwwJfqPsxVetA6qR7UNc+fVb6KHwvm0bzi2rQ3xw3D/syRHwdMkpoVDQPCk43H9WufgfBKRen87dFwIDAQABo4ICSTCCAkU"
  "wHwYDVR0jBBgwFoAUUWj/kK8CB3U8zNllZGKiErhZcjswHQYDVR0OBBYEFGS/RLNGCZvPWh1xSaIEcouINIQjMHsGA1UdEQR0MHK"
  "CDnd3dy5naXRodWIuY29tggwqLmdpdGh1Yi5jb22CCmdpdGh1Yi5jb22CCyouZ2l0aHViLmlvgglnaXRodWIuaW+CFyouZ2l0aHV"
  "idXNlcmNvbnRlbnQuY29tghVnaXRodWJ1c2VyY29udGVudC5jb20wDgYDVR0PAQH/BAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwM"
  "BBggrBgEFBQcDAjB1BgNVHR8EbjBsMDSgMqAwhi5odHRwOi8vY3JsMy5kaWdpY2VydC5jb20vc2hhMi1oYS1zZXJ2ZXItZzUuY3J"
  "sMDSgMqAwhi5odHRwOi8vY3JsNC5kaWdpY2VydC5jb20vc2hhMi1oYS1zZXJ2ZXItZzUuY3JsMEwGA1UdIARFMEMwNwYJYIZIAYb"
  "9bAEBMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwCAYGZ4EMAQICMIGDBggrBgEFBQcBAQR3MHU"
  "wJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBNBggrBgEFBQcwAoZBaHR0cDovL2NhY2VydHMuZGlnaWNlcnQ"
  "uY29tL0RpZ2lDZXJ0U0hBMkhpZ2hBc3N1cmFuY2VTZXJ2ZXJDQS5jcnQwDAYDVR0TAQH/BAIwADANBgkqhkiG9w0BAQsFAAOCAQE"
  "ATxbRdPg+o49+96/P+rbdp4ie+CGtfCgUubT/Z9C54k+BfQO0nbxVgCSM5WZQuLgo2Q+0lcxisod8zxZeU0j5wviQINwOln/iN89"
  "Bx3VmDRynTe4CqhsAwOoO1ERmCAmsAJBwY/rNr4mK22p8erBrqMW0nYXYU5NFynI+pNTjojhKD4II8PNV8G2yMWwYOb/u4+WPzUA"
  "HC9DpZdrWTEH/W69Cr/KxRqGsWPwpgMv2Wqav8jaT35JxqTXjOlhQqzo6fNn3eYOeCf4PkCxZKwckWjy10qDaRbjhwAMHAGj2TPr"
  "idlvOj/7QyyX5m8up/1US8z1fRW4yoCSOt6V2bwuH6cAvAAMAAAAAAQEAAA==");
  // clang-format on

  deserializeAndVerify(base64Serialization);
}

TEST(psm_DeserializeCert, preSSLStatusConsolidation)
{
  // clang-format off
  // Generated using serialized output of test "good.include-subdomains.pinning.example.com"
  // in security/manager/ssl/tests/unit/test_cert_chains.js
  nsCString base64Serialization(
  "FnhllAKWRHGAlo+ESXykKAAAAAAAAAAAwAAAAAAAAEaphjojH6pBabDSgSnsfLHeAAgAAgAAAAAAAAAAAAAAAAAAAAAB4vFIJp5w"
  "RkeyPxAQ9RJGKPqbqVvKO0mKuIl8ec8o/uhmCjImkVxP+7sgiYWmMt8FvcOXmlQiTNWFiWlrbpbqgwAAAAAAAAONMIIDiTCCAnGg"
  "AwIBAgIUWbWLTwLBvfwcoiU7I8lDz9snfUgwDQYJKoZIhvcNAQELBQAwEjEQMA4GA1UEAwwHVGVzdCBDQTAiGA8yMDE2MTEyNzAw"
  "MDAwMFoYDzIwMTkwMjA1MDAwMDAwWjAaMRgwFgYDVQQDDA9UZXN0IEVuZC1lbnRpdHkwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw"
  "ggEKAoIBAQC6iFGoRI4W1kH9braIBjYQPTwT2erkNUq07PVoV2wke8HHJajg2B+9sZwGm24ahvJr4q9adWtqZHEIeqVap0WH9xzV"
  "JJwCfs1D/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7qdw4A8Njf1mCyuwJJKkfbmIYXmQsVeQPdI7xeC4SB+o"
  "N9OIQ+8nFthVt2Zaqn4CkC86exCABiTMHGyXrZZhW7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7LyJvaeO0ipVhHe4m1iWd"
  "q5EITjbLHCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NAgMBAAGjgcowgccwgZAGA1UdEQSBiDCBhYIJbG9jYWxob3N0"
  "gg0qLmV4YW1wbGUuY29tghUqLnBpbm5pbmcuZXhhbXBsZS5jb22CKCouaW5jbHVkZS1zdWJkb21haW5zLnBpbm5pbmcuZXhhbXBs"
  "ZS5jb22CKCouZXhjbHVkZS1zdWJkb21haW5zLnBpbm5pbmcuZXhhbXBsZS5jb20wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAB"
  "hhZodHRwOi8vbG9jYWxob3N0Ojg4ODgvMA0GCSqGSIb3DQEBCwUAA4IBAQBE+6IPJK5OeonoQPC4CCWMd69SjhwS7X6TNgxDJzW7"
  "qpVm4SFyYZ2xqzr2zib5LsYek6/jok5LPSpJVeFuSeiesvGMxk0O4ZEihPxSM4uR4xpCnPzz7LoFIzMELJv5i+cgLw4+6cINPkLj"
  "oCUdb+AXSTur7THJaO75B44I2JjJfMfzgW1FwoWgXL/PQWRw+VY6OY1glqZOXzP+vfSja1SoggpiCzdPx7h1/SEEZov7zhCZXv1C"
  "enx1njlpcj9wWEJMsyZczMNtiz5GkRrLaqCz9F8ah3NvkvPAZ0oOqtxuQgMXK/c0OXJVKi0SCJsWqZDoZhCrS/dE9guxlseZqhSI"
  "wC8DAwAAAAABAQAAAAAAAAZ4MjU1MTkAAAAOUlNBLVBTUy1TSEEyNTYBlZ+xZWUXSH+rm9iRO+Uxl650zaXNL0c/lvXwt//2LGgA"
  "AAACZgoyJpFcT/u7IImFpjLfBb3Dl5pUIkzVhYlpa26W6oMAAAAAAAADjTCCA4kwggJxoAMCAQICFFm1i08Cwb38HKIlOyPJQ8/b"
  "J31IMA0GCSqGSIb3DQEBCwUAMBIxEDAOBgNVBAMMB1Rlc3QgQ0EwIhgPMjAxNjExMjcwMDAwMDBaGA8yMDE5MDIwNTAwMDAwMFow"
  "GjEYMBYGA1UEAwwPVGVzdCBFbmQtZW50aXR5MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuohRqESOFtZB/W62iAY2"
  "ED08E9nq5DVKtOz1aFdsJHvBxyWo4NgfvbGcBptuGobya+KvWnVramRxCHqlWqdFh/cc1SScAn7NQ/weadA4ICmTqyDDSeTbuUzC"
  "a2wO7RWCD/F+rWkasdMCOosqQe6ncOAPDY39ZgsrsCSSpH25iGF5kLFXkD3SO8XguEgfqDfTiEPvJxbYVbdmWqp+ApAvOnsQgAYk"
  "zBxsl62WYVu34pYSwHUxowyR3bTK9/ytHSXTCe+5Fw6naOGzey8ib2njtIqVYR3uJtYlnauRCE42yxwkBCy/Fosv5fGPmRcxuLP+"
  "SSP6clHEMdUDrNoYCjXtjQIDAQABo4HKMIHHMIGQBgNVHREEgYgwgYWCCWxvY2FsaG9zdIINKi5leGFtcGxlLmNvbYIVKi5waW5u"
  "aW5nLmV4YW1wbGUuY29tgigqLmluY2x1ZGUtc3ViZG9tYWlucy5waW5uaW5nLmV4YW1wbGUuY29tgigqLmV4Y2x1ZGUtc3ViZG9t"
  "YWlucy5waW5uaW5nLmV4YW1wbGUuY29tMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcwAYYWaHR0cDovL2xvY2FsaG9zdDo4ODg4"
  "LzANBgkqhkiG9w0BAQsFAAOCAQEARPuiDySuTnqJ6EDwuAgljHevUo4cEu1+kzYMQyc1u6qVZuEhcmGdsas69s4m+S7GHpOv46JO"
  "Sz0qSVXhbknonrLxjMZNDuGRIoT8UjOLkeMaQpz88+y6BSMzBCyb+YvnIC8OPunCDT5C46AlHW/gF0k7q+0xyWju+QeOCNiYyXzH"
  "84FtRcKFoFy/z0FkcPlWOjmNYJamTl8z/r30o2tUqIIKYgs3T8e4df0hBGaL+84QmV79Qnp8dZ45aXI/cFhCTLMmXMzDbYs+RpEa"
  "y2qgs/RfGodzb5LzwGdKDqrcbkIDFyv3NDlyVSotEgibFqmQ6GYQq0v3RPYLsZbHmaoUiGYKMiaRXE/7uyCJhaYy3wW9w5eaVCJM"
  "1YWJaWtuluqDAAAAAAAAAtcwggLTMIIBu6ADAgECAhRdBTvvC7swO3cbVWIGn/56DrQ+cjANBgkqhkiG9w0BAQsFADASMRAwDgYD"
  "VQQDDAdUZXN0IENBMCIYDzIwMTYxMTI3MDAwMDAwWhgPMjAxOTAyMDUwMDAwMDBaMBIxEDAOBgNVBAMMB1Rlc3QgQ0EwggEiMA0G"
  "CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC6iFGoRI4W1kH9braIBjYQPTwT2erkNUq07PVoV2wke8HHJajg2B+9sZwGm24ahvJr"
  "4q9adWtqZHEIeqVap0WH9xzVJJwCfs1D/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7qdw4A8Njf1mCyuwJJKk"
  "fbmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8nFthVt2Zaqn4CkC86exCABiTMHGyXrZZhW7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo"
  "4bN7LyJvaeO0ipVhHe4m1iWdq5EITjbLHCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NAgMBAAGjHTAbMAwGA1UdEwQF"
  "MAMBAf8wCwYDVR0PBAQDAgEGMA0GCSqGSIb3DQEBCwUAA4IBAQCDjewR53YLc3HzZKugRDbQVxjJNILW6fSIyW9dSglYcWh6aiOK"
  "9cZFVtzRWYEYkIlicAyTiPw34bXzxU1cK6sCSmBR+UTXbRPGb4OOy3MRaoF1m3jxwnPkQwxezDiqJTydCbYcBu0sKwURAZOd5QK9"
  "22MsOsnrLjNlpRDmuH0VFhb5uN2I5mM3NvMnP2Or19O1Bk//iGD6AyJfiZFcii+FsDrJhbzw6lakEV7O/EnD0kk2l7I0VMtg1xZB"
  "bEw7P6+V9zz5cAzaaq7EB0mCE+jJckSzSETBN+7lyVD8gwmHYxxZfPnUM/yvPbMU9L3xWD/z6HHwO6r+9m7BT+2pHjBCAAA=");
  // clang-format on

  deserializeAndVerify(base64Serialization, Nothing(), Some(2));
}

TEST(psm_DeserializeCert, preSSLStatusConsolidationFailedCertChain)
{
  // clang-format off
  // Generated using serialized output of test "expired.example.com"
  // in security/manager/ssl/tests/unit/test_cert_chains.js
  nsCString base64Serialization(
  "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");
  // clang-format on

  deserializeAndVerify(base64Serialization, Some(2));
}

TEST(psm_DeserializeCert, preNsIX509CertListReplacement)
{
  // This was the serialized output of test
  // "good.include-subdomains.pinning.example.com" // in
  // security/manager/ssl/tests/unit/test_cert_chains.js The serialized output
  // was generated before we replace nsIX509CertList with Array<nsIX509Cert>, so
  // it had the old version of transportSecurityInfo.
  nsCString base64Serialization(
      "FnhllAKWRHGAlo+ESXykKAAAAAAAAAAAwAAAAAAAAEaphjojH6pBabDSgSnsfLHeAAAAAgA"
      "AAAAAAAAAAAAAAAAAAAEAMQFmCjImkVxP+7sgiYWmMt8FvcOXmlQiTNWFiWlrbpbqgwAAAA"
      "AAAAONMIIDiTCCAnGgAwIBAgIUDUo/9G0rz7fJiWTw0hY6TIyPRSIwDQYJKoZIhvcNAQELB"
      "QAwEjEQMA4GA1UEAwwHVGVzdCBDQTAiGA8yMDE3MTEyNzAwMDAwMFoYDzIwMjAwMjA1MDAw"
      "MDAwWjAaMRgwFgYDVQQDDA9UZXN0IEVuZC1lbnRpdHkwggEiMA0GCSqGSIb3DQEBAQUAA4I"
      "BDwAwggEKAoIBAQC6iFGoRI4W1kH9braIBjYQPTwT2erkNUq07PVoV2wke8HHJajg2B+9sZ"
      "wGm24ahvJr4q9adWtqZHEIeqVap0WH9xzVJJwCfs1D/B5p0DggKZOrIMNJ5Nu5TMJrbA7tF"
      "YIP8X6taRqx0wI6iypB7qdw4A8Njf1mCyuwJJKkfbmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8n"
      "FthVt2Zaqn4CkC86exCABiTMHGyXrZZhW7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN"
      "7LyJvaeO0ipVhHe4m1iWdq5EITjbLHCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe"
      "2NAgMBAAGjgcowgccwgZAGA1UdEQSBiDCBhYIJbG9jYWxob3N0gg0qLmV4YW1wbGUuY29tg"
      "hUqLnBpbm5pbmcuZXhhbXBsZS5jb22CKCouaW5jbHVkZS1zdWJkb21haW5zLnBpbm5pbmcu"
      "ZXhhbXBsZS5jb22CKCouZXhjbHVkZS1zdWJkb21haW5zLnBpbm5pbmcuZXhhbXBsZS5jb20"
      "wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzABhhZodHRwOi8vbG9jYWxob3N0Ojg4ODgvMA"
      "0GCSqGSIb3DQEBCwUAA4IBAQCkguNhMyVCYhyYXfE22wNvlaobK2YRb4OGMxySIKuQ80N0X"
      "lO+xpLJTs9YzFVY1+JTHNez1QfwP9KJeZznTzVzLh4sv0swx/+oUxCfLb0VIl/kdUqLkbGY"
      "rAmtjeOKZLaqVtRH0BnmbPowLak1pi6nQYOU+aL9QOuvT/j3rXoimcdo6X3TK1SN2/64fGM"
      "yG/pwas+JXehbReUf4n1ewk84ADtb+ew8tRAKf/uxzKUj5t/UgqDsnTWq5wUc5IJKwoHT41"
      "sQnNqPg12x4+WGWiAsWCpR/hKYHFGr7rb4JTGEPAJpWcv9WtZYAvwT78a2xpHp5XNglj16I"
      "jWEukvJuU1WwC8AAwAAAAABAQAAAAAAAAZ4MjU1MTkAAAAOUlNBLVBTUy1TSEEyNTYBlZ+x"
      "ZWUXSH+rm9iRO+Uxl650zaXNL0c/lvXwt//2LGgAAAACZgoyJpFcT/u7IImFpjLfBb3Dl5p"
      "UIkzVhYlpa26W6oMAAAAAAAADjTCCA4kwggJxoAMCAQICFA1KP/RtK8+3yYlk8NIWOkyMj0"
      "UiMA0GCSqGSIb3DQEBCwUAMBIxEDAOBgNVBAMMB1Rlc3QgQ0EwIhgPMjAxNzExMjcwMDAwM"
      "DBaGA8yMDIwMDIwNTAwMDAwMFowGjEYMBYGA1UEAwwPVGVzdCBFbmQtZW50aXR5MIIBIjAN"
      "BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuohRqESOFtZB/W62iAY2ED08E9nq5DVKtOz"
      "1aFdsJHvBxyWo4NgfvbGcBptuGobya+KvWnVramRxCHqlWqdFh/cc1SScAn7NQ/weadA4IC"
      "mTqyDDSeTbuUzCa2wO7RWCD/F+rWkasdMCOosqQe6ncOAPDY39ZgsrsCSSpH25iGF5kLFXk"
      "D3SO8XguEgfqDfTiEPvJxbYVbdmWqp+ApAvOnsQgAYkzBxsl62WYVu34pYSwHUxowyR3bTK"
      "9/ytHSXTCe+5Fw6naOGzey8ib2njtIqVYR3uJtYlnauRCE42yxwkBCy/Fosv5fGPmRcxuLP"
      "+SSP6clHEMdUDrNoYCjXtjQIDAQABo4HKMIHHMIGQBgNVHREEgYgwgYWCCWxvY2FsaG9zdI"
      "INKi5leGFtcGxlLmNvbYIVKi5waW5uaW5nLmV4YW1wbGUuY29tgigqLmluY2x1ZGUtc3ViZ"
      "G9tYWlucy5waW5uaW5nLmV4YW1wbGUuY29tgigqLmV4Y2x1ZGUtc3ViZG9tYWlucy5waW5u"
      "aW5nLmV4YW1wbGUuY29tMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcwAYYWaHR0cDovL2x"
      "vY2FsaG9zdDo4ODg4LzANBgkqhkiG9w0BAQsFAAOCAQEApILjYTMlQmIcmF3xNtsDb5WqGy"
      "tmEW+DhjMckiCrkPNDdF5TvsaSyU7PWMxVWNfiUxzXs9UH8D/SiXmc5081cy4eLL9LMMf/q"
      "FMQny29FSJf5HVKi5GxmKwJrY3jimS2qlbUR9AZ5mz6MC2pNaYup0GDlPmi/UDrr0/49616"
      "IpnHaOl90ytUjdv+uHxjMhv6cGrPiV3oW0XlH+J9XsJPOAA7W/nsPLUQCn/7scylI+bf1IK"
      "g7J01qucFHOSCSsKB0+NbEJzaj4NdsePlhlogLFgqUf4SmBxRq+62+CUxhDwCaVnL/VrWWA"
      "L8E+/GtsaR6eVzYJY9eiI1hLpLyblNVmYKMiaRXE/7uyCJhaYy3wW9w5eaVCJM1YWJaWtul"
      "uqDAAAAAAAAAtcwggLTMIIBu6ADAgECAhQpoXAjALAddSApG46EBfimNiyZuDANBgkqhkiG"
      "9w0BAQsFADASMRAwDgYDVQQDDAdUZXN0IENBMCIYDzIwMTcxMTI3MDAwMDAwWhgPMjAyMDA"
      "yMDUwMDAwMDBaMBIxEDAOBgNVBAMMB1Rlc3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDw"
      "AwggEKAoIBAQC6iFGoRI4W1kH9braIBjYQPTwT2erkNUq07PVoV2wke8HHJajg2B+9sZwGm"
      "24ahvJr4q9adWtqZHEIeqVap0WH9xzVJJwCfs1D/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP"
      "8X6taRqx0wI6iypB7qdw4A8Njf1mCyuwJJKkfbmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8nFth"
      "Vt2Zaqn4CkC86exCABiTMHGyXrZZhW7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7Ly"
      "JvaeO0ipVhHe4m1iWdq5EITjbLHCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NA"
      "gMBAAGjHTAbMAwGA1UdEwQFMAMBAf8wCwYDVR0PBAQDAgEGMA0GCSqGSIb3DQEBCwUAA4IB"
      "AQAgyCfLAcVs/MkERxunH9pZA4ja1QWWjsxSg9KgAIfOgj8c5RPHbl4oeWk0raNKWMu5+FR"
      "3/94IJeD45C3h/Y3+1HDyC6ZuzdgMXv63dk0a36JDFlPA3swqwYhnL7pHnbdcfDyWnMVfmL"
      "NeAhL7QA+Vf5fJmTsxEJwFaHo9JpKoQ469RdWno6aHeK3TfiQFaebzT1MRabCJXDeyw8Oal"
      "QICt0M0wx29B6HNof3px2NxKyC6qlf01wwNSaaIbsctDaLL5ZLN6T1LjpJsooMvDwRt69+S"
      "Xo8SmD4YO6Wr4Q9drI3cCwVeQXwxoUuB96muQQ2M3WDiMz5ZLI3oMLu8KSPsAA==");

  deserializeAndVerify(base64Serialization, Nothing(), Some(2));
}

TEST(psm_DeserializeCert, preNsIX509CertListReplacementV2)
{
  // Same as the above test, however, this is the v2 version of the
  // serialization.
  nsCString base64Serialization(
      "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");

  deserializeAndVerify(base64Serialization, Nothing(), Some(2));
}

TEST(psm_DeserializeCert, preNsIX509CertListReplacementWithFailedChain)
{
  // This was the serialized output of test "expired.example.com"
  // in security/manager/ssl/tests/unit/test_cert_chains.js
  // The serialized output was generated before we replace nsIX509CertList with
  // Array<nsIX509Cert>, so it had the old version of transportSecurityInfo.
  nsCString base64Serialization(
      "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"
      "96muQQ2M3WDiMz5ZLI3oMLu8KSPs");

  deserializeAndVerify(base64Serialization, Some(2));
}

TEST(psm_DeserializeCert, preNsIX509CertListReplacementWithFailedChainV2)
{
  // Same as the above test, however, this is the v2 version of the
  // serialization.
  nsCString base64Serialization(
      "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"
      "96muQQ2M3WDiMz5ZLI3oMLu8KSPsAA==");

  deserializeAndVerify(base64Serialization, Some(2));
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsSerializationHelper.h"

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

TEST(psm_DeserializeCert, gecko33)
{
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

  nsCOMPtr<nsISupports> cert;
  nsresult rv = NS_DeserializeObject(base64Serialization, getter_AddRefs(cert));
  ASSERT_EQ(NS_OK, rv);
  ASSERT_TRUE(cert);
}

TEST(psm_DeserializeCert, gecko46)
{
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

  nsCOMPtr<nsISupports> cert;
  nsresult rv = NS_DeserializeObject(base64Serialization, getter_AddRefs(cert));
  ASSERT_EQ(NS_OK, rv);
  ASSERT_TRUE(cert);
}

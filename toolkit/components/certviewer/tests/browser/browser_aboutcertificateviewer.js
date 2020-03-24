/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const validCert =
  "about:certificate?cert=MIIGMDCCBRigAwIBAgIQCa2oS7Nau6nwv1WyRNIJnzANBgkqhkiG9w0BAQsFADBwMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMS8wLQYDVQQDEyZEaWdpQ2VydCBTSEEyIEhpZ2ggQXNzdXJhbmNlIFNlcnZlciBDQTAeFw0xOTA2MDYwMDAwMDBaFw0xOTA5MDQxMjAwMDBaMGExCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTETMBEGA1UEBxMKTWVubG8gUGFyazEXMBUGA1UEChMORmFjZWJvb2ssIEluYy4xFzAVBgNVBAMMDiouZmFjZWJvb2suY29tMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEEALPlfjd8gHkIMDeBOFhRt0e%2B%2F3fdm%2BZdzdM4s1WIHVbYztgmdEy20PUwDCviYKJX4GG%2BT9ivT8kJ%2FvWl1P%2FYqOCA54wggOaMB8GA1UdIwQYMBaAFFFo%2F5CvAgd1PMzZZWRiohK4WXI7MB0GA1UdDgQWBBTsQZVRTuib7KY9w3FfOYsZHvOC9zCBxwYDVR0RBIG%2FMIG8gg4qLmZhY2Vib29rLmNvbYINbWVzc2VuZ2VyLmNvbYILKi5mYmNkbi5uZXSCCCouZmIuY29tghAqLm0uZmFjZWJvb2suY29tggZmYi5jb22CDiouZmFjZWJvb2submV0gg4qLnh4LmZiY2RuLm5ldIIOKi54ei5mYmNkbi5uZXSCDyoubWVzc2VuZ2VyLmNvbYILKi5mYnNieC5jb22CDioueHkuZmJjZG4ubmV0ggxmYWNlYm9vay5jb20wDgYDVR0PAQH%2FBAQDAgeAMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjB1BgNVHR8EbjBsMDSgMqAwhi5odHRwOi8vY3JsMy5kaWdpY2VydC5jb20vc2hhMi1oYS1zZXJ2ZXItZzYuY3JsMDSgMqAwhi5odHRwOi8vY3JsNC5kaWdpY2VydC5jb20vc2hhMi1oYS1zZXJ2ZXItZzYuY3JsMEwGA1UdIARFMEMwNwYJYIZIAYb9bAEBMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwCAYGZ4EMAQICMIGDBggrBgEFBQcBAQR3MHUwJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBNBggrBgEFBQcwAoZBaHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0U0hBMkhpZ2hBc3N1cmFuY2VTZXJ2ZXJDQS5jcnQwDAYDVR0TAQH%2FBAIwADCCAQQGCisGAQQB1nkCBAIEgfUEgfIA8AB1AKS5CZC0GFgUh7sTosxncAo8NZgE%2BRvfuON3zQ7IDdwQAAABay5CAaUAAAQDAEYwRAIgMwBM6q%2BdBwu2mNtMMjTEwJZZxyoUlHUEYO%2BbfkId%2F9MCIAQ2bxhnxrapYv74fzyoTkt9m%2BELq6%2B43OVpivRVKDKTAHcAdH7agzGtMxCRIZzOJU9CcMK%2F%2FV5CIAjGNzV55hB7zFYAAAFrLkIA%2FAAABAMASDBGAiEA0x1xPWue6RMSE9nbjYBt637CRC86ixrODP%2FEIlI5mCgCIQCHNdqgOlswd0LqaW4QRii2JHN4bnoEj%2FD9j7%2BkqEB7LjANBgkqhkiG9w0BAQsFAAOCAQEAnRdIVNZ850s7IvLgg%2BU9kKxA18kLKVpIF%2FrJHkXTkymvBHKAGOFNfzqF2YxHFhkDMIuoMO2F%2BA1E0Eo8zb1atL6%2FL59broEHTOH6xFmJAlZW0h6mZg8iRJ9Ae0pTN%2FfowaoN9aFVRnVr9ccKhOdqsXYyEZ3Ze39sEwx7Uau9KhzyuJW12Jh3S%2BZJYUdBADeeJNL5ZXSUFIyjgkwSQZPaaWAzSGHZFt3sWhMjdNoBkjRJFlASLDM3m1ZWsKA47vuXvJc%2FDXT35lC1DJmyhYb9qNPR71a1hJ8TS7vUwdDd%2BdEHiJj2wQLV3m7Tn7YvWyJOEyi4n6%2FrPqT44LZmgK7HWw%3D%3D&cert=MIIEsTCCA5mgAwIBAgIQBOHnpNxc8vNtwCtCuF0VnzANBgkqhkiG9w0BAQsFADBsMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5jZSBFViBSb290IENBMB4XDTEzMTAyMjEyMDAwMFoXDTI4MTAyMjEyMDAwMFowcDELMAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3LmRpZ2ljZXJ0LmNvbTEvMC0GA1UEAxMmRGlnaUNlcnQgU0hBMiBIaWdoIEFzc3VyYW5jZSBTZXJ2ZXIgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC24C%2FCJAbIbQRf1%2B8KZAayfSImZRauQkCbztyfn3YHPsMwVYcZuU%2BUDlqUH1VWtMICKq%2FQmO4LQNfE0DtyyBSe75CxEamu0si4QzrZCwvV1ZX1QK%2FIHe1NnF9Xt4ZQaJn1itrSxwUfqJfJ3KSxgoQtxq2lnMcZgqaFD15EWCo3j%2F018QsIJzJa9buLnqS9UdAn4t07QjOjBSjEuyjMmqwrIw14xnvmXnG3Sj4I%2B4G3FhahnSMSTeXXkgisdaScus0Xsh5ENWV%2FUyU50RwKmmMbGZJ0aAo3wsJSSMs5WqK24V3B3aAguCGikyZvFEohQcftbZvySC%2FzA%2FWiaJJTL17jAgMBAAGjggFJMIIBRTASBgNVHRMBAf8ECDAGAQH%2FAgEAMA4GA1UdDwEB%2FwQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wSwYDVR0fBEQwQjBAoD6gPIY6aHR0cDovL2NybDQuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0SGlnaEFzc3VyYW5jZUVWUm9vdENBLmNybDA9BgNVHSAENjA0MDIGBFUdIAAwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQUzAdBgNVHQ4EFgQUUWj%2FkK8CB3U8zNllZGKiErhZcjswHwYDVR0jBBgwFoAUsT7DaQP4v0cB1JgmGggC72NkK8MwDQYJKoZIhvcNAQELBQADggEBABiKlYkD5m3fXPwdaOpKj4PWUS%2BNa0QWnqxj9dJubISZi6qBcYRb7TROsLd5kinMLYBq8I4g4Xmk%2FgNHE%2Br1hspZcX30BJZr01lYPf7TMSVcGDiEo%2Bafgv2MW5gxTs14nhr9hctJqvIni5ly%2FD6q1UEL2tU2ob8cbkdJf17ZSHwD2f2LSaCYJkJA69aSEaRkCldUxPUd1gJea6zuxICaEnL6VpPX%2F78whQYwvwt%2FTv9XBZ0k7YXDK%2FumdaisLRbvfXknsuvCnQsH6qqF0wGjIChBWUMo0oHjqvbsezt3tkBigAVBRQHvFwY%2B3sAzm2fTYS5yh%2BRp%2FBIAV0AecPUeybQ%3D&cert=MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBsMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5jZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDELMAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2UgRVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm%2B9S75S0tMqbf5YE%2Fyc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTWPNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG%2B%2BMXs2ziS4wblCJEMxChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFBIk5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx%2BmM0aBhakaHPQNAQTXKFx01p8VdteZOE3hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsgEsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH%2FBAQDAgGGMA8GA1UdEwEB%2FwQFMAMBAf8wHQYDVR0OBBYEFLE%2Bw2kD%2BL9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaAFLE%2Bw2kD%2BL9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3NecnzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe%2FEW1ntlMMUu4kehDLI6zeM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jFhS9OMPagMRYjyOfiZRYzy78aG6A9%2BMpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2Yzi9RKR%2F5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2%2FS6cCZdkGCevEsXCS%2B0yx5DaMkHJ8HSXPfqIbloEpw8nL%2Be%2FIBcm2PN7EeqJSdnoDfzAIJ9VNep%2BOkuE6N36B9K";

const emptyUrl = "about:certificate";

const invalidCert = "about:certificate?cert=MIIH";

const pem_to_der_error = "about:certificate?cert=MIIHQ";

const error_parsing_cert =
  "about:certificate?cert=MIIHQjCCBiqgAwIBAgIQCgYwQn9bvO11MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMTQwMgYDVQQDEytEaWdpQ2VydCBTSEEyIEV4dGVuZGVkIFZhbGlkYXRpb24gU2VydmVyIENBMB4XDTE4MDUwODAwMDAwMFoXDTIwMDYwMzEyMDAwMFowgccxHTAbBgNVBA8MFFByaXZhdGUgT3JnYW5pemF0aW9uMRMwEQYLKwYBBAGCNzwCAQMTAlVTMRkwFwYLKwYBBAGCNzwCAQITCERlbGF3YXJlMRAwDgYDVQQFEwc1MTU3NTUwMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEVMBMGA1UEChMMR2l0SHViLCBJbmMuMRMwEQYDVQQDEwpnaXRodWIuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxjyq8jyXDDrBTyitcnB90865tWBzpHSbindG%2FXqYQkzFMBlXmqkzC%2BFdTRBYyneZw5Pz%2BXWQvL%2B74JW6LsWNc2EF0xCEqLOJuC9zjPAqbr7uroNLghGxYf13YdqbG5oj%2F4x%2BogEG3dF%2FU5YIwVr658DKyESMV6eoYV9mDVfTuJastkqcwero%2B5ZAKfYVMLUEsMwFtoTDJFmVf6JlkOWwsxp1WcQ%2FMRQK1cyqOoUFUgYylgdh3yeCDPeF22Ax8AlQxbcaI%2BGwfQL1FB7Jy%2Bh%2BKjME9lE%2FUpgV6Qt2R1xNSmvFCBWu%2BNFX6epwFP%2FJRbkMfLz0beYFUvmMgLtwVpEPSwIDAQABo4IDeTCCA3UwHwYDVR0jBBgwFoAUPdNQpdagre7zSmAKZdMh1Pj41g8wHQYDVR0OBBYEFMnCU2FmnV%2BrJfQmzQ84mqhJ6kipMCUGA1UdEQQeMByCCmdpdGh1Yi5jb22CDnd3dy5naXRodWIuY29tMA4GA1UdDwEB%2FwQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdQYDVR0fBG4wbDA0oDKgMIYuaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcyLmNybDA0oDKgMIYuaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcyLmNybDBLBgNVHSAERDBCMDcGCWCGSAGG%2FWwCATAqMCgGCCsGAQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMAcGBWeBDAEBMIGIBggrBgEFBQcBAQR8MHowJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBSBggrBgEFBQcwAoZGaHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0U0hBMkV4dGVuZGVkVmFsaWRhdGlvblNlcnZlckNBLmNydDAMBgNVHRMBAf8EAjAAMIIBfgYKKwYBBAHWeQIEAgSCAW4EggFqAWgAdgCkuQmQtBhYFIe7E6LMZ3AKPDWYBPkb37jjd80OyA3cEAAAAWNBYm0KAAAEAwBHMEUCIQDRZp38cTWsWH2GdBpe%2FuPTWnsu%2Fm4BEC2%2BdIcvSykZYgIgCP5gGv6yzaazxBK2NwGdmmyuEFNSg2pARbMJlUFgU5UAdgBWFAaaL9fC7NP14b1Esj7HRna5vJkRXMDvlJhV1onQ3QAAAWNBYm0tAAAEAwBHMEUCIQCi7omUvYLm0b2LobtEeRAYnlIo7n6JxbYdrtYdmPUWJQIgVgw1AZ51vK9ENinBg22FPxb82TvNDO05T17hxXRC2IYAdgC72d%2B8H4pxtZOUI5eqkntHOFeVCqtS6BqQlmQ2jh7RhQAAAWNBYm3fAAAEAwBHMEUCIQChzdTKUU2N%2BXcqcK0OJYrN8EYynloVxho4yPk6Dq3EPgIgdNH5u8rC3UcslQV4B9o0a0w204omDREGKTVuEpxGeOQwDQYJKoZIhvcNAQELBQADggEBAHAPWpanWOW%2Fip2oJ5grAH8mqQfaunuCVE%2Bvac%2B88lkDK%2FLVdFgl2B6kIHZiYClzKtfczG93hWvKbST4NRNHP9LiaQqdNC17e5vNHnXVUGw%2ByxyjMLGqkgepOnZ2Rb14kcTOGp4i5AuJuuaMwXmCo7jUwPwfLe1NUlVBKqg6LK0Hcq4K0sZnxE8HFxiZ92WpV2AVWjRMEc%2F2z2shNoDvxvFUYyY1Oe67xINkmyQKc%2BygSBZzyLnXSFVWmHr3u5dcaaQGGAR42v6Ydr4iL38Hd4dOiBma%2BFXsXBIqWUjbST4VXmdaol7uzFMojA4zkxQDZAvF5XgJlAFadfySna%2Fteik%3D";

async function checkForErrorSection(infoMessage, url, errorMessage, testType) {
  info(infoMessage);

  await BrowserTestUtils.withNewTab(url, async function(browser) {
    await SpecialPowers.spawn(
      browser,
      [{ errorMessage, testType }],
      async function({ errorMessage, testType }) {
        let errorSection;

        if (testType === "invalid") {
          await ContentTaskUtils.waitForCondition(() => {
            return (errorSection = content.document
              .querySelector("certificate-section")
              .shadowRoot.querySelector("error-section"));
          }, errorMessage);
          ok(errorSection, errorMessage);
        } else if (testType === "valid") {
          ok(!errorSection, errorMessage);
        }
      }
    );
  });
}

add_task(async function test_checkForErrorSection() {
  let itemsToTest = [
    {
      testType: "invalid",
      url: emptyUrl,
      infoMessage: "Testing for error when no certificate is provided.",
      errorMessage:
        "Error section should be visible when URL does not contain a certificate.",
    },
    {
      testType: "valid",
      url: validCert,
      infoMessage: "Testing for error when a valid certificate is provided.",
      errorMessage:
        "Error section should not be visible when URL contains a valid certificate.",
    },
    {
      testType: "invalid",
      url: invalidCert,
      infoMessage: "Testing for error when invalid certificate is provided.",
      errorMessage:
        "Error section should be visible when certificate is invalid.",
    },
    {
      testType: "invalid",
      url: pem_to_der_error,
      infoMessage: "Testing for error when PEM to DIR error occurs.",
      errorMessage:
        "Error section should be visible when PEM to DIR error occurs.",
    },
    {
      testType: "invalid",
      url: error_parsing_cert,
      infoMessage: "Testing for error when parsing error occurs.",
      errorMessage:
        "Error section should be visible when parsing error occurs.",
    },
  ];

  for (let i = 0; i < itemsToTest.length; i++) {
    let item = itemsToTest[i];
    await checkForErrorSection(
      item.infoMessage,
      item.url,
      item.errorMessage,
      item.testType
    );
  }
});

"use strict";

/*
To change this file you will have to add a function to download a file in toolkit/components/certviewer/content/certviewer.js (e.g: https://ourcodeworld.com/articles/read/189/how-to-create-a-file-and-generate-a-download-with-javascript-in-the-browser-without-a-server),
download adjustedCerts (e.g: download("out.txt", JSON.stringify(adjustedCerts)), do it after this line https://searchfox.org/mozilla-central/rev/e3fc8f8970491aef14d3212b2d052942f4d29818/toolkit/components/certviewer/content/certviewer.js#428),
then open Nightly and go to about:certificate?cert=MIIHQjCCBiqgAwIBAgIQCgYwQn9bvO1pVzllk7ZFHzANBgkqhkiG9w0BAQsFADB1MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMTQwMgYDVQQDEytEaWdpQ2VydCBTSEEyIEV4dGVuZGVkIFZhbGlkYXRpb24gU2VydmVyIENBMB4XDTE4MDUwODAwMDAwMFoXDTIwMDYwMzEyMDAwMFowgccxHTAbBgNVBA8MFFByaXZhdGUgT3JnYW5pemF0aW9uMRMwEQYLKwYBBAGCNzwCAQMTAlVTMRkwFwYLKwYBBAGCNzwCAQITCERlbGF3YXJlMRAwDgYDVQQFEwc1MTU3NTUwMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEVMBMGA1UEChMMR2l0SHViLCBJbmMuMRMwEQYDVQQDEwpnaXRodWIuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxjyq8jyXDDrBTyitcnB90865tWBzpHSbindG%2FXqYQkzFMBlXmqkzC%2BFdTRBYyneZw5Pz%2BXWQvL%2B74JW6LsWNc2EF0xCEqLOJuC9zjPAqbr7uroNLghGxYf13YdqbG5oj%2F4x%2BogEG3dF%2FU5YIwVr658DKyESMV6eoYV9mDVfTuJastkqcwero%2B5ZAKfYVMLUEsMwFtoTDJFmVf6JlkOWwsxp1WcQ%2FMRQK1cyqOoUFUgYylgdh3yeCDPeF22Ax8AlQxbcaI%2BGwfQL1FB7Jy%2Bh%2BKjME9lE%2FUpgV6Qt2R1xNSmvFCBWu%2BNFX6epwFP%2FJRbkMfLz0beYFUvmMgLtwVpEPSwIDAQABo4IDeTCCA3UwHwYDVR0jBBgwFoAUPdNQpdagre7zSmAKZdMh1Pj41g8wHQYDVR0OBBYEFMnCU2FmnV%2BrJfQmzQ84mqhJ6kipMCUGA1UdEQQeMByCCmdpdGh1Yi5jb22CDnd3dy5naXRodWIuY29tMA4GA1UdDwEB%2FwQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdQYDVR0fBG4wbDA0oDKgMIYuaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcyLmNybDA0oDKgMIYuaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcyLmNybDBLBgNVHSAERDBCMDcGCWCGSAGG%2FWwCATAqMCgGCCsGAQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMAcGBWeBDAEBMIGIBggrBgEFBQcBAQR8MHowJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBSBggrBgEFBQcwAoZGaHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0U0hBMkV4dGVuZGVkVmFsaWRhdGlvblNlcnZlckNBLmNydDAMBgNVHRMBAf8EAjAAMIIBfgYKKwYBBAHWeQIEAgSCAW4EggFqAWgAdgCkuQmQtBhYFIe7E6LMZ3AKPDWYBPkb37jjd80OyA3cEAAAAWNBYm0KAAAEAwBHMEUCIQDRZp38cTWsWH2GdBpe%2FuPTWnsu%2Fm4BEC2%2BdIcvSykZYgIgCP5gGv6yzaazxBK2NwGdmmyuEFNSg2pARbMJlUFgU5UAdgBWFAaaL9fC7NP14b1Esj7HRna5vJkRXMDvlJhV1onQ3QAAAWNBYm0tAAAEAwBHMEUCIQCi7omUvYLm0b2LobtEeRAYnlIo7n6JxbYdrtYdmPUWJQIgVgw1AZ51vK9ENinBg22FPxb82TvNDO05T17hxXRC2IYAdgC72d%2B8H4pxtZOUI5eqkntHOFeVCqtS6BqQlmQ2jh7RhQAAAWNBYm3fAAAEAwBHMEUCIQChzdTKUU2N%2BXcqcK0OJYrN8EYynloVxho4yPk6Dq3EPgIgdNH5u8rC3UcslQV4B9o0a0w204omDREGKTVuEpxGeOQwDQYJKoZIhvcNAQELBQADggEBAHAPWpanWOW%2Fip2oJ5grAH8mqQfaunuCVE%2Bvac%2B88lkDK%2FLVdFgl2B6kIHZiYClzKtfczG93hWvKbST4NRNHP9LiaQqdNC17e5vNHnXVUGw%2ByxyjMLGqkgepOnZ2Rb14kcTOGp4i5AuJuuaMwXmCo7jUwPwfLe1NUlVBKqg6LK0Hcq4K0sZnxE8HFxiZ92WpV2AVWjRMEc%2F2z2shNoDvxvFUYyY1Oe67xINkmyQKc%2BygSBZzyLnXSFVWmHr3u5dcaaQGGAR42v6Ydr4iL38Hd4dOiBma%2BFXsXBIqWUjbST4VXmdaol7uzFMojA4zkxQDZAvF5XgJlAFadfySna%2Fteik%3D
open the file, and finally copy and paste it here like

const adjustedCerts = <what you just copied>;
*/

const adjustedCerts = [
  {
    certItems: [
      {
        sectionId: "subject-name",
        sectionItems: [
          { labelId: "business-category", info: "Private Organization" },
          { labelId: "inc-country", info: "US" },
          { labelId: "inc-state-province", info: "Delaware" },
          { labelId: "serial-number", info: "5157550" },
          { labelId: "country", info: "US" },
          { labelId: "state-province", info: "California" },
          { labelId: "locality", info: "San Francisco" },
          { labelId: "organization", info: "GitHub, Inc." },
          { labelId: "common-name", info: "github.com" },
        ],
        Critical: false,
      },
      {
        sectionId: "issuer-name",
        sectionItems: [
          { labelId: "country", info: "US" },
          { labelId: "organization", info: "DigiCert Inc" },
          { labelId: "organizational-unit", info: "www.digicert.com" },
          {
            labelId: "common-name",
            info: "DigiCert SHA2 Extended Validation Server CA",
          },
        ],
        Critical: false,
      },
      {
        sectionId: "validity",
        sectionItems: [
          {
            labelId: "not-before",
            info: {
              local: "5/7/2018, 9:00:00 PM (Brasilia Standard Time)",
              utc: "Tue, 08 May 2018 00:00:00 GMT",
            },
          },
          {
            labelId: "not-after",
            info: {
              local: "6/3/2020, 9:00:00 AM (Brasilia Standard Time)",
              utc: "Wed, 03 Jun 2020 12:00:00 GMT",
            },
          },
        ],
        Critical: false,
      },
      {
        sectionId: "subject-alt-names",
        sectionItems: [
          { labelId: "dns-name", info: "github.com" },
          { labelId: "dns-name", info: "www.github.com" },
        ],
        Critical: false,
      },
      {
        sectionId: "public-key-info",
        sectionItems: [
          { labelId: "algorithm", info: "RSA" },
          { labelId: "key-size", info: 2048 },
          { labelId: "exponent", info: 65537 },
          {
            labelId: "modulus",
            info:
              "C6:3C:AA:F2:3C:97:0C:3A:C1:4F:28:AD:72:70:7D:D3:CE:B9:B5:60:73:A4:74:9B:8A:77:46:FD:7A:98:42:4C:C5:30:19:57:9A:A9:33:0B:E1:5D:4D:10:58:CA:77:99:C3:93:F3:F9:75:90:BC:BF:BB:E0:95:BA:2E:C5:8D:73:61:05:D3:10:84:A8:B3:89:B8:2F:73:8C:F0:2A:6E:BE:EE:AE:83:4B:82:11:B1:61:FD:77:61:DA:9B:1B:9A:23:FF:8C:7E:A2:01:06:DD:D1:7F:53:96:08:C1:5A:FA:E7:C0:CA:C8:44:8C:57:A7:A8:61:5F:66:0D:57:D3:B8:96:AC:B6:4A:9C:C1:EA:E8:FB:96:40:29:F6:15:30:B5:04:B0:CC:05:B6:84:C3:24:59:95:7F:A2:65:90:E5:B0:B3:1A:75:59:C4:3F:31:14:0A:D5:CC:AA:3A:85:05:52:06:32:96:07:61:DF:27:82:0C:F7:85:DB:60:31:F0:09:50:C5:B7:1A:23:E1:B0:7D:02:F5:14:1E:C9:CB:E8:7E:2A:33:04:F6:51:3F:52:98:15:E9:0B:76:47:5C:4D:4A:6B:C5:08:15:AE:F8:D1:57:E9:EA:70:14:FF:C9:45:B9:0C:7C:BC:F4:6D:E6:05:52:F9:8C:80:BB:70:56:91:0F:4B",
          },
        ],
        Critical: false,
      },
      {
        sectionId: "miscellaneous",
        sectionItems: [
          {
            labelId: "serial-number",
            info: "0A:06:30:42:7F:5B:BC:ED:69:57:39:65:93:B6:45:1F",
          },
          {
            labelId: "signature-algorithm",
            info: "SHA-256 with RSA Encryption",
          },
          { labelId: "version", info: "3" },
          {
            labelId: "download",
            info: "PEM (cert)PEM (chain)",
          },
        ],
        Critical: false,
      },
      {
        sectionId: "fingerprints",
        sectionItems: [
          {
            labelId: "sha-256",
            info:
              "31:11:50:0C:4A:66:01:2C:DA:E3:33:EC:3F:CA:1C:9D:DE:45:C9:54:44:0E:7E:E4:13:71:6B:FF:36:63:C0:74",
          },
          {
            labelId: "sha-1",
            info: "CA:06:F5:6B:25:8B:7A:0D:4F:2B:05:47:09:39:47:86:51:15:19:84",
          },
        ],
        Critical: false,
      },
      {
        sectionId: "basic-constraints",
        sectionItems: [{ labelId: "certificate-authority", info: false }],
        Critical: true,
      },
      {
        sectionId: "key-usages",
        sectionItems: [
          {
            labelId: "purposes",
            info: ["Digital Signature", "Key Encipherment"],
          },
        ],
        Critical: true,
      },
      {
        sectionId: "extended-key-usages",
        sectionItems: [
          {
            labelId: "purposes",
            info: ["Server Authentication", "Client Authentication"],
          },
        ],
        Critical: false,
      },
      {
        sectionId: "subject-key-id",
        sectionItems: [
          {
            labelId: "key-id",
            info: "C9:C2:53:61:66:9D:5F:AB:25:F4:26:CD:0F:38:9A:A8:49:EA:48:A9",
          },
        ],
        Critical: false,
      },
      {
        sectionId: "authority-key-id",
        sectionItems: [
          {
            labelId: "key-id",
            info: "3D:D3:50:A5:D6:A0:AD:EE:F3:4A:60:0A:65:D3:21:D4:F8:F8:D6:0F",
          },
        ],
        Critical: false,
      },
      {
        sectionId: "crl-endpoints",
        sectionItems: [
          {
            labelId: "distribution-point",
            info: "http://crl3.digicert.com/sha2-ev-server-g2.crl",
          },
          {
            labelId: "distribution-point",
            info: "http://crl4.digicert.com/sha2-ev-server-g2.crl",
          },
        ],
        Critical: false,
      },
      {
        sectionId: "authority-info-aia",
        sectionItems: [
          { labelId: "location", info: "http://ocsp.digicert.com" },
          {
            labelId: "method",
            info: "Online Certificate Status Protocol (OCSP)",
          },
          {
            labelId: "location",
            info:
              "http://cacerts.digicert.com/DigiCertSHA2ExtendedValidationServerCA.crt",
          },
          { labelId: "method", info: "CA Issuers" },
        ],
        Critical: false,
      },
      {
        sectionId: "certificate-policies",
        sectionItems: [
          {
            labelId: "policy",
            info: "ANSI Organizational Identifier ( 2.16.840 )",
          },
          { labelId: "value", info: "2.16.840.1.114412.2.1" },
          {
            labelId: "qualifier",
            info: "Practices Statement ( 1.3.6.1.5.5.7.2.1 )",
          },
          { labelId: "value", info: "https://www.digicert.com/CPS" },
          { labelId: "policy", info: "Certificate Type ( 2.23.140.1.1 )" },
          { labelId: "value", info: "Extended Validation" },
        ],
        Critical: false,
      },
      {
        sectionId: "embedded-scts",
        sectionItems: [
          {
            labelId: "logid",
            info:
              "A4:B9:09:90:B4:18:58:14:87:BB:13:A2:CC:67:70:0A:3C:35:98:04:F9:1B:DF:B8:E3:77:CD:0E:C8:0D:DC:10",
          },
          { labelId: "name", info: "Google “Pilot”" },
          { labelId: "signaturealgorithm", info: "SHA-256 ECDSA" },
          { labelId: "version", info: 1 },
          {
            labelId: "timestamp",
            info: {
              local: "5/8/2018, 5:12:39 PM (Brasilia Standard Time)",
              utc: "Tue, 08 May 2018 20:12:39 GMT",
            },
          },
          {
            labelId: "logid",
            info:
              "56:14:06:9A:2F:D7:C2:EC:D3:F5:E1:BD:44:B2:3E:C7:46:76:B9:BC:99:11:5C:C0:EF:94:98:55:D6:89:D0:DD",
          },
          { labelId: "name", info: "DigiCert Server" },
          { labelId: "signaturealgorithm", info: "SHA-256 ECDSA" },
          { labelId: "version", info: 1 },
          {
            labelId: "timestamp",
            info: {
              local: "5/8/2018, 5:12:39 PM (Brasilia Standard Time)",
              utc: "Tue, 08 May 2018 20:12:39 GMT",
            },
          },
          {
            labelId: "logid",
            info:
              "BB:D9:DF:BC:1F:8A:71:B5:93:94:23:97:AA:92:7B:47:38:57:95:0A:AB:52:E8:1A:90:96:64:36:8E:1E:D1:85",
          },
          { labelId: "name", info: "Google “Skydiver”" },
          { labelId: "signaturealgorithm", info: "SHA-256 ECDSA" },
          { labelId: "version", info: 1 },
          {
            labelId: "timestamp",
            info: {
              local: "5/8/2018, 5:12:39 PM (Brasilia Standard Time)",
              utc: "Tue, 08 May 2018 20:12:39 GMT",
            },
          },
        ],
        Critical: false,
      },
    ],
    tabName: "github.com",
  },
];

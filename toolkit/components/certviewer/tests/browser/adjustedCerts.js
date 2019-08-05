"use strict";

const adjustedCerts = {
  certItems: [
    {
      sectionTitle: "Subject Name",
      sectionItems: [
        { label: "Business Category", info: "Private Organization" },
        { label: "Inc. Country", info: "US" },
        { label: "Inc. State / Province", info: "Delaware" },
        { label: "Serial Number", info: "5157550" },
        { label: "Country", info: "US" },
        { label: "State / Province", info: "California" },
        { label: "Locality", info: "San Francisco" },
        { label: "Organization", info: "GitHub, Inc." },
        { label: "Common Name", info: "github.com" },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Issuer Name",
      sectionItems: [
        { label: "Country", info: "US" },
        { label: "Organization", info: "DigiCert Inc" },
        { label: "Organizational Unit", info: "www.digicert.com" },
        {
          label: "Common Name",
          info: "DigiCert SHA2 Extended Validation Server CA",
        },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Validity",
      sectionItems: [
        {
          label: "Not Before",
          info: "5/7/2018, 9:00:00 PM (Brasilia Standard Time)",
        },
        { label: "Not Before UTC", info: "Tue, 08 May 2018 00:00:00 GMT" },
        {
          label: "Not After",
          info: "6/3/2020, 9:00:00 AM (Brasilia Standard Time)",
        },
        { label: "Not After UTC", info: "Wed, 03 Jun 2020 12:00:00 GMT" },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Subject Alt Names",
      sectionItems: [
        { label: "DNS Name", info: "github.com" },
        { label: "DNS Name", info: "www.github.com" },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Public Key Info",
      sectionItems: [
        { label: "Algorithm", info: "RSA" },
        { label: "Key size", info: 2048 },
        { label: "Curve" },
        { label: "Public Value" },
        { label: "Exponent", info: 65537 },
        {
          label: "Modulus",
          info:
            "C6:3C:AA:F2:3C:97:0C:3A:C1:4F:28:AD:72:70:7D:D3:CE:B9:B5:60:73:A4:74:9B:8A:77:46:FD:7A:98:42:4C:C5:30:19:57:9A:A9:33:0B:E1:5D:4D:10:58:CA:77:99:C3:93:F3:F9:75:90:BC:BF:BB:E0:95:BA:2E:C5:8D:73:61:05:D3:10:84:A8:B3:89:B8:2F:73:8C:F0:2A:6E:BE:EE:AE:83:4B:82:11:B1:61:FD:77:61:DA:9B:1B:9A:23:FF:8C:7E:A2:01:06:DD:D1:7F:53:96:08:C1:5A:FA:E7:C0:CA:C8:44:8C:57:A7:A8:61:5F:66:0D:57:D3:B8:96:AC:B6:4A:9C:C1:EA:E8:FB:96:40:29:F6:15:30:B5:04:B0:CC:05:B6:84:C3:24:59:95:7F:A2:65:90:E5:B0:B3:1A:75:59:C4:3F:31:14:0A:D5:CC:AA:3A:85:05:52:06:32:96:07:61:DF:27:82:0C:F7:85:DB:60:31:F0:09:50:C5:B7:1A:23:E1:B0:7D:02:F5:14:1E:C9:CB:E8:7E:2A:33:04:F6:51:3F:52:98:15:E9:0B:76:47:5C:4D:4A:6B:C5:08:15:AE:F8:D1:57:E9:EA:70:14:FF:C9:45:B9:0C:7C:BC:F4:6D:E6:05:52:F9:8C:80:BB:70:56:91:0F:4B",
        },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Miscellaneous",
      sectionItems: [
        {
          label: "Serial Number",
          info: "0A:06:30:42:7F:5B:BC:ED:69:57:39:65:93:B6:45:1F",
        },
        { label: "Signature Algorithm", info: "SHA-256 with RSA Encryption" },
        { label: "Version", info: "3" },
        {
          label: "Download",
          info:
            "-----BEGIN%20CERTIFICATE-----%0D%0AMIIHQjCCBiqgAwIBAgIQCgYwQn9bvO1pVzllk7ZFHzANBgkqhkiG9w0BAQsFADB1%0D%0AMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3%0D%0Ad3cuZGlnaWNlcnQuY29tMTQwMgYDVQQDEytEaWdpQ2VydCBTSEEyIEV4dGVuZGVk%0D%0AIFZhbGlkYXRpb24gU2VydmVyIENBMB4XDTE4MDUwODAwMDAwMFoXDTIwMDYwMzEy%0D%0AMDAwMFowgccxHTAbBgNVBA8MFFByaXZhdGUgT3JnYW5pemF0aW9uMRMwEQYLKwYB%0D%0ABAGCNzwCAQMTAlVTMRkwFwYLKwYBBAGCNzwCAQITCERlbGF3YXJlMRAwDgYDVQQF%0D%0AEwc1MTU3NTUwMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEWMBQG%0D%0AA1UEBxMNU2FuIEZyYW5jaXNjbzEVMBMGA1UEChMMR2l0SHViLCBJbmMuMRMwEQYD%0D%0AVQQDEwpnaXRodWIuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA%0D%0Axjyq8jyXDDrBTyitcnB90865tWBzpHSbindG/XqYQkzFMBlXmqkzC+FdTRBYyneZ%0D%0Aw5Pz+XWQvL+74JW6LsWNc2EF0xCEqLOJuC9zjPAqbr7uroNLghGxYf13YdqbG5oj%0D%0A/4x+ogEG3dF/U5YIwVr658DKyESMV6eoYV9mDVfTuJastkqcwero+5ZAKfYVMLUE%0D%0AsMwFtoTDJFmVf6JlkOWwsxp1WcQ/MRQK1cyqOoUFUgYylgdh3yeCDPeF22Ax8AlQ%0D%0AxbcaI+GwfQL1FB7Jy+h+KjME9lE/UpgV6Qt2R1xNSmvFCBWu+NFX6epwFP/JRbkM%0D%0AfLz0beYFUvmMgLtwVpEPSwIDAQABo4IDeTCCA3UwHwYDVR0jBBgwFoAUPdNQpdag%0D%0Are7zSmAKZdMh1Pj41g8wHQYDVR0OBBYEFMnCU2FmnV+rJfQmzQ84mqhJ6kipMCUG%0D%0AA1UdEQQeMByCCmdpdGh1Yi5jb22CDnd3dy5naXRodWIuY29tMA4GA1UdDwEB/wQE%0D%0AAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdQYDVR0fBG4wbDA0%0D%0AoDKgMIYuaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcy%0D%0ALmNybDA0oDKgMIYuaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NoYTItZXYtc2Vy%0D%0AdmVyLWcyLmNybDBLBgNVHSAERDBCMDcGCWCGSAGG/WwCATAqMCgGCCsGAQUFBwIB%0D%0AFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMAcGBWeBDAEBMIGIBggrBgEF%0D%0ABQcBAQR8MHowJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBS%0D%0ABggrBgEFBQcwAoZGaHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0%0D%0AU0hBMkV4dGVuZGVkVmFsaWRhdGlvblNlcnZlckNBLmNydDAMBgNVHRMBAf8EAjAA%0D%0AMIIBfgYKKwYBBAHWeQIEAgSCAW4EggFqAWgAdgCkuQmQtBhYFIe7E6LMZ3AKPDWY%0D%0ABPkb37jjd80OyA3cEAAAAWNBYm0KAAAEAwBHMEUCIQDRZp38cTWsWH2GdBpe/uPT%0D%0AWnsu/m4BEC2+dIcvSykZYgIgCP5gGv6yzaazxBK2NwGdmmyuEFNSg2pARbMJlUFg%0D%0AU5UAdgBWFAaaL9fC7NP14b1Esj7HRna5vJkRXMDvlJhV1onQ3QAAAWNBYm0tAAAE%0D%0AAwBHMEUCIQCi7omUvYLm0b2LobtEeRAYnlIo7n6JxbYdrtYdmPUWJQIgVgw1AZ51%0D%0AvK9ENinBg22FPxb82TvNDO05T17hxXRC2IYAdgC72d+8H4pxtZOUI5eqkntHOFeV%0D%0ACqtS6BqQlmQ2jh7RhQAAAWNBYm3fAAAEAwBHMEUCIQChzdTKUU2N+XcqcK0OJYrN%0D%0A8EYynloVxho4yPk6Dq3EPgIgdNH5u8rC3UcslQV4B9o0a0w204omDREGKTVuEpxG%0D%0AeOQwDQYJKoZIhvcNAQELBQADggEBAHAPWpanWOW/ip2oJ5grAH8mqQfaunuCVE+v%0D%0Aac+88lkDK/LVdFgl2B6kIHZiYClzKtfczG93hWvKbST4NRNHP9LiaQqdNC17e5vN%0D%0AHnXVUGw+yxyjMLGqkgepOnZ2Rb14kcTOGp4i5AuJuuaMwXmCo7jUwPwfLe1NUlVB%0D%0AKqg6LK0Hcq4K0sZnxE8HFxiZ92WpV2AVWjRMEc/2z2shNoDvxvFUYyY1Oe67xINk%0D%0AmyQKc+ygSBZzyLnXSFVWmHr3u5dcaaQGGAR42v6Ydr4iL38Hd4dOiBma+FXsXBIq%0D%0AWUjbST4VXmdaol7uzFMojA4zkxQDZAvF5XgJlAFadfySna/teik=%0D%0A-----END%20CERTIFICATE-----%0D%0A",
        },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Fingerprints",
      sectionItems: [
        {
          label: "SHA-256",
          info:
            "31:11:50:0C:4A:66:01:2C:DA:E3:33:EC:3F:CA:1C:9D:DE:45:C9:54:44:0E:7E:E4:13:71:6B:FF:36:63:C0:74",
        },
        {
          label: "SHA-1",
          info: "CA:06:F5:6B:25:8B:7A:0D:4F:2B:05:47:09:39:47:86:51:15:19:84",
        },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Basic Constraints",
      sectionItems: [{ label: "Certificate Authority" }],
      Critical: true,
    },
    {
      sectionTitle: "Key Usages",
      sectionItems: [
        { label: "Purposes", info: ["Digital Signature", "Key Encipherment"] },
      ],
      Critical: true,
    },
    {
      sectionTitle: "Extended Key Usages",
      sectionItems: [
        {
          label: "Purposes",
          info: ["Server Authentication", "Client Authentication"],
        },
      ],
      Critical: false,
    },
    {
      sectionTitle: "OCSP Stapling",
      sectionItems: [{ label: "Required", info: false }],
      Critical: false,
    },
    {
      sectionTitle: "Subject Key ID",
      sectionItems: [
        {
          label: "Key ID",
          info: "C9:C2:53:61:66:9D:5F:AB:25:F4:26:CD:0F:38:9A:A8:49:EA:48:A9",
        },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Authority Key ID",
      sectionItems: [
        {
          label: "Key ID",
          info: "3D:D3:50:A5:D6:A0:AD:EE:F3:4A:60:0A:65:D3:21:D4:F8:F8:D6:0F",
        },
      ],
      Critical: false,
    },
    {
      sectionTitle: "CRL Endpoints",
      sectionItems: [
        {
          label: "Distribution Point",
          info: "http://crl3.digicert.com/sha2-ev-server-g2.crl",
        },
        {
          label: "Distribution Point",
          info: "http://crl4.digicert.com/sha2-ev-server-g2.crl",
        },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Authority Info (AIA)",
      sectionItems: [
        { label: "Location", info: "http://ocsp.digicert.com" },
        { label: "Method", info: "Online Certificate Status Protocol (OCSP)" },
        {
          label: "Location",
          info:
            "http://cacerts.digicert.com/DigiCertSHA2ExtendedValidationServerCA.crt",
        },
        { label: "Method", info: "CA Issuers" },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Certificate Policies",
      sectionItems: [
        {
          label: "Policy",
          info: "ANSI Organizational Identifier ( 2.16.840 )",
        },
        { label: "Value", info: "2.16.840.1.114412.2.1" },
        {
          label: "Qualifier",
          info: "Practices Statement ( 1.3.6.1.5.5.7.2.1 )",
        },
        { label: "Value", info: "https://www.digicert.com/CPS" },
        { label: "Policy", info: "Certificate Type ( 2.23.140.1.1 )" },
        { label: "Value", info: "Extended Validation" },
      ],
      Critical: false,
    },
    {
      sectionTitle: "Embedded SCTs",
      sectionItems: [
        {
          label: "logId",
          info:
            "A4:B9:09:90:B4:18:58:14:87:BB:13:A2:CC:67:70:0A:3C:35:98:04:F9:1B:DF:B8:E3:77:CD:0E:C8:0D:DC:10",
        },
        { label: "name", info: "Google “Pilot”" },
        { label: "signatureAlgorithm", info: "SHA-256 ECDSA" },
        {
          label: "timestamp",
          info: "5/8/2018, 5:12:39 PM (Brasilia Standard Time)",
        },
        { label: "timestampUTC", info: "Tue, 08 May 2018 20:12:39 GMT" },
        { label: "version", info: 1 },
        {
          label: "logId",
          info:
            "56:14:06:9A:2F:D7:C2:EC:D3:F5:E1:BD:44:B2:3E:C7:46:76:B9:BC:99:11:5C:C0:EF:94:98:55:D6:89:D0:DD",
        },
        { label: "name", info: "DigiCert Server" },
        { label: "signatureAlgorithm", info: "SHA-256 ECDSA" },
        {
          label: "timestamp",
          info: "5/8/2018, 5:12:39 PM (Brasilia Standard Time)",
        },
        { label: "timestampUTC", info: "Tue, 08 May 2018 20:12:39 GMT" },
        { label: "version", info: 1 },
        {
          label: "logId",
          info:
            "BB:D9:DF:BC:1F:8A:71:B5:93:94:23:97:AA:92:7B:47:38:57:95:0A:AB:52:E8:1A:90:96:64:36:8E:1E:D1:85",
        },
        { label: "name", info: "Google “Skydiver”" },
        { label: "signatureAlgorithm", info: "SHA-256 ECDSA" },
        {
          label: "timestamp",
          info: "5/8/2018, 5:12:39 PM (Brasilia Standard Time)",
        },
        { label: "timestampUTC", info: "Tue, 08 May 2018 20:12:39 GMT" },
        { label: "version", info: 1 },
      ],
      Critical: false,
    },
  ],
  tabName: "github.com",
};

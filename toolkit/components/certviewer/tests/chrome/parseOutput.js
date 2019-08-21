export const parseOutput = [
  {
    ext: {
      aia: {
        descriptions: [
          {
            location: "http://ocsp.digicert.com",
            method: "Online Certificate Status Protocol (OCSP)",
          },
          {
            location:
              "http://cacerts.digicert.com/DigiCertSHA2ExtendedValidationServerCA.crt",
            method: "CA Issuers",
          },
        ],
        critical: false,
      },
      aKID: {
        critical: false,
        id: "3D:D3:50:A5:D6:A0:AD:EE:F3:4A:60:0A:65:D3:21:D4:F8:F8:D6:0F",
      },
      basicConstraints: { cA: false, critical: true },
      crlPoints: {
        critical: false,
        points: [
          "http://crl3.digicert.com/sha2-ev-server-g2.crl",
          "http://crl4.digicert.com/sha2-ev-server-g2.crl",
        ],
      },
      cp: {
        critical: false,
        policies: [
          {
            id: "2.16.840",
            name: "ANSI Organizational Identifier",
            qualifiers: [
              {
                id: "1.3.6.1.5.5.7.2.1",
                name: "Practices Statement",
                value: "https://www.digicert.com/CPS",
              },
            ],
            value: "2.16.840.1.114412.2.1",
          },
          {
            id: "2.23.140.1.1",
            name: "Certificate Type",
            value: "Extended Validation",
          },
        ],
      },
      eKeyUsages: {
        critical: false,
        purposes: ["Server Authentication", "Client Authentication"],
      },
      keyUsages: {
        critical: true,
        purposes: ["Digital Signature", "Key Encipherment"],
      },
      msCrypto: { exists: false },
      ocspStaple: { critical: false, required: false },
      scts: {
        critical: false,
        timestamps: [
          {
            logId:
              "A4:B9:09:90:B4:18:58:14:87:BB:13:A2:CC:67:70:0A:3C:35:98:04:F9:1B:DF:B8:E3:77:CD:0E:C8:0D:DC:10",
            name: "Google “Pilot”",
            signatureAlgorithm: "SHA-256 ECDSA",
            timestamp: "11/7/2018, 7:50:01 PM (Brasilia Standard Time)",
            timestampUTC: "Wed, 07 Nov 2018 21:50:01 GMT",
            version: 1,
          },
          {
            logId:
              "87:75:BF:E7:59:7C:F8:8C:43:99:5F:BD:F3:6E:FF:56:8D:47:56:36:FF:4A:B5:60:C1:B4:EA:FF:5E:A0:83:0F",
            name: "DigiCert Server 2",
            signatureAlgorithm: "SHA-256 ECDSA",
            timestamp: "11/7/2018, 7:50:01 PM (Brasilia Standard Time)",
            timestampUTC: "Wed, 07 Nov 2018 21:50:01 GMT",
            version: 1,
          },
          {
            logId:
              "EE:4B:BD:B7:75:CE:60:BA:E1:42:69:1F:AB:E1:9E:66:A3:0F:7E:5F:B0:72:D8:83:00:C4:7B:89:7A:A8:FD:CB",
            name: "Google “Rocketeer”",
            signatureAlgorithm: "SHA-256 ECDSA",
            timestamp: "11/7/2018, 7:50:01 PM (Brasilia Standard Time)",
            timestampUTC: "Wed, 07 Nov 2018 21:50:01 GMT",
            version: 1,
          },
        ],
      },
      sKID: {
        critical: false,
        id: "6C:B0:43:56:FE:3D:E8:12:EC:D9:12:F5:63:D5:C4:CA:07:AF:B0:76",
      },
      san: {
        altNames: [
          ["DNS Name", "www.digicert.com"],
          ["DNS Name", "admin.digicert.com"],
          ["DNS Name", "digicert.com"],
          ["DNS Name", "content.digicert.com"],
          ["DNS Name", "login.digicert.com"],
          ["DNS Name", "api.digicert.com"],
          ["DNS Name", "ws.digicert.com"],
          ["DNS Name", "www.origin.digicert.com"],
        ],
        critical: false,
      },
    },
    files: {
      pem:
        "-----BEGIN%20CERTIFICATE-----%0D%0AMIIIzDCCB7SgAwIBAgIQDrbqtBjIc9jzwDHc3d8YsDANBgkqhkiG9w0BAQsFADB1%0D%0AMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3%0D%0Ad3cuZGlnaWNlcnQuY29tMTQwMgYDVQQDEytEaWdpQ2VydCBTSEEyIEV4dGVuZGVk%0D%0AIFZhbGlkYXRpb24gU2VydmVyIENBMB4XDTE4MTEwNzAwMDAwMFoXDTIwMTExMzEy%0D%0AMDAwMFowgc8xHTAbBgNVBA8MFFByaXZhdGUgT3JnYW5pemF0aW9uMRMwEQYLKwYB%0D%0ABAGCNzwCAQMTAlVTMRUwEwYLKwYBBAGCNzwCAQITBFV0YWgxFTATBgNVBAUTDDUy%0D%0AOTk1MzctMDE0MjELMAkGA1UEBhMCVVMxDTALBgNVBAgTBFV0YWgxDTALBgNVBAcT%0D%0ABExlaGkxFzAVBgNVBAoTDkRpZ2lDZXJ0LCBJbmMuMQwwCgYDVQQLEwNTUkUxGTAX%0D%0ABgNVBAMTEHd3dy5kaWdpY2VydC5jb20wggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAw%0D%0AggIKAoICAQDOn4XKOTAwt/aYabScEF1QOyVj0OVo1NmlyizWNZWyPg0pi53ggUoE%0D%0A98CeNUkz+6scEYqWNY6l3qKB56pJJIqNQmo9NoWO8k2G/jTIjFFGqNWYIq23i4+H%0D%0AqaXi1/H/aWFgazk1qkyyAOQQA/p56bG9m5Ok/IBM/BZnLqVJLGJOx9ihgG1dI9Dr%0D%0A6vap+8QaPRau3t9sEd2cxe4Ix7gLdaYG3vxsYf3BycKTSKtyrbkX1Qy0dsSxy+GC%0D%0AM2ETxE1gMa7vRomQ/ZoZo8Ib55kFp6lIT6UOOkkdyiJdpWPXIZZlsZR5wkegWDsJ%0D%0AP7Xv7nE0WMkY1+05iNYtrzZRhhlnBw2AoMGNI+tsBXLQKeZfWFmU30bhkzX99pmv%0D%0AIYJ3f1fQGLao44nQEjdknIvpm0HMgvagYCnQVnnhJStzyYz324flWLPSp57OQeNM%0D%0Atr6O5W0HdWyhUZU+D4R6wObYQMZ5biYjRhtAQjMg8EVQEfZzEdr0WGO5JRHLHyot%0D%0A8tErXM9DiF5cCbzfcjeuoik2SHW+vbuPagMiHTM9+3lr0oRO+ZWwcM7fJvn1JfR2%0D%0APDLAaI3QUv7OLhSH32UfQsk+1ICq05m2HwSxiAviDRl5De66MEZDdvu03sUAQTHv%0D%0AWnw0Mr7Jgbjtn0DeUKLYwsRWg+spqoFTJHWGbb9RIb+3lxev7nIqOQIDAQABo4ID%0D%0A+zCCA/cwHwYDVR0jBBgwFoAUPdNQpdagre7zSmAKZdMh1Pj41g8wHQYDVR0OBBYE%0D%0AFGywQ1b+PegS7NkS9WPVxMoHr7B2MIGlBgNVHREEgZ0wgZqCEHd3dy5kaWdpY2Vy%0D%0AdC5jb22CEmFkbWluLmRpZ2ljZXJ0LmNvbYIMZGlnaWNlcnQuY29tghRjb250ZW50%0D%0ALmRpZ2ljZXJ0LmNvbYISbG9naW4uZGlnaWNlcnQuY29tghBhcGkuZGlnaWNlcnQu%0D%0AY29tgg93cy5kaWdpY2VydC5jb22CF3d3dy5vcmlnaW4uZGlnaWNlcnQuY29tMA4G%0D%0AA1UdDwEB/wQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdQYD%0D%0AVR0fBG4wbDA0oDKgMIYuaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL3NoYTItZXYt%0D%0Ac2VydmVyLWcyLmNybDA0oDKgMIYuaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3No%0D%0AYTItZXYtc2VydmVyLWcyLmNybDBLBgNVHSAERDBCMDcGCWCGSAGG/WwCATAqMCgG%0D%0ACCsGAQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMAcGBWeBDAEB%0D%0AMIGIBggrBgEFBQcBAQR8MHowJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2lj%0D%0AZXJ0LmNvbTBSBggrBgEFBQcwAoZGaHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY29t%0D%0AL0RpZ2lDZXJ0U0hBMkV4dGVuZGVkVmFsaWRhdGlvblNlcnZlckNBLmNydDAMBgNV%0D%0AHRMBAf8EAjAAMIIBfwYKKwYBBAHWeQIEAgSCAW8EggFrAWkAdQCkuQmQtBhYFIe7%0D%0AE6LMZ3AKPDWYBPkb37jjd80OyA3cEAAAAWbwJ1QeAAAEAwBGMEQCIAzNFnn0UQyL%0D%0A5ZkwA7BX2JiQkjCk9QZrNHR9S1So7l8LAiAzoHDpWV5Dq5iM5NU86nJwqB1qsjZt%0D%0Aa4fWlvNKs0S+eQB3AId1v+dZfPiMQ5lfvfNu/1aNR1Y2/0q1YMG06v9eoIMPAAAB%0D%0AZvAnVbMAAAQDAEgwRgIhAOr0s3YbgyIs7dytmC1s5ClugVmo+r+W6ZKqRpphsIjT%0D%0AAiEAqgzFns9X9+SKwTSz0h1epijxrnWH+qZ815QgaJVd83UAdwDuS723dc5guuFC%0D%0AaR+r4Z5mow9+X7By2IMAxHuJeqj9ywAAAWbwJ1RAAAAEAwBIMEYCIQCsFv4KXmNK%0D%0A+POADrfs37hcjApxSuiK14+tN5wFGby6QQIhANa+RvbVx1nYk13IyLCBiCMENLEV%0D%0A9urcBZfprZN+ZsrlMA0GCSqGSIb3DQEBCwUAA4IBAQB+tQhW07rk6eb2L2Ir+Wwq%0D%0Al5ZJaoaZq3zwoG8vPMFTZbcWPYsSVI5KuXELdB1QnmPbHebTJuI/gjfD65vf7C3N%0D%0ABjgLoXui39mwN4UNTBCc2Ls8s8/aVfTZGPCNNwynCEsY9h0HRobI8kiVPvU0kyBJ%0D%0AO3p3msqR7W2lAIWciJWmoPQkEFh7u84/oc7Pz2SSE6tqMW9lZmyBqjSuBufRJn1g%0D%0ACTdp/5DZdQWTNL6L5GSsRoXQnarsVtFVriqOjWf5R4O4z6LQznlbBQCMUR3geUNq%0D%0A0vSblnmgav0+L9HK97wqtONu1CrJ9/Q2JwoxFAs/zps/nh9oJU2ToK9lzl/38gpo%0D%0A-----END%20CERTIFICATE-----%0D%0A",
    },
    fingerprint: {
      sha1: "8E:43:B7:D0:84:FD:B2:6C:08:9E:D3:F1:10:0C:D2:1D:D4:AD:C6:DF",
      sha256:
        "37:BF:44:A9:28:03:8A:22:AE:0C:E3:2D:96:3C:36:F5:D2:2D:37:99:D6:B2:63:CB:9C:1D:45:D1:CF:FE:B0:69",
    },
    issuer: {
      cn: "DigiCert SHA2 Extended Validation Server CA",
      dn:
        "c=US, o=DigiCert Inc, ou=www.digicert.com, cn=DigiCert SHA2 Extended Validation Server CA",
      entries: [
        ["Country", "US"],
        ["Organization", "DigiCert Inc"],
        ["Organizational Unit", "www.digicert.com"],
        ["Common Name", "DigiCert SHA2 Extended Validation Server CA"],
      ],
    },
    notBefore: "11/6/2018, 10:00:00 PM (Brasilia Standard Time)",
    notBeforeUTC: "Wed, 07 Nov 2018 00:00:00 GMT",
    notAfter: "11/13/2020, 9:00:00 AM (Brasilia Standard Time)",
    notAfterUTC: "Fri, 13 Nov 2020 12:00:00 GMT",
    subject: {
      cn: "www.digicert.com",
      dn:
        "OID.2.5.4.15=Private Organization, OID.1.3.6.1.4.1.311.60.2.1.3=US, OID.1.3.6.1.4.1.311.60.2.1.2=Utah, serialNumber=5299537-0142, c=US, s=Utah, l=Lehi, o=DigiCert, Inc., ou=SRE, cn=www.digicert.com",
      entries: [
        ["Business Category", "Private Organization"],
        ["Inc. Country", "US"],
        ["Inc. State / Province", "Utah"],
        ["Serial Number", "5299537-0142"],
        ["Country", "US"],
        ["State / Province", "Utah"],
        ["Locality", "Lehi"],
        ["Organization", "DigiCert, Inc."],
        ["Organizational Unit", "SRE"],
        ["Common Name", "www.digicert.com"],
      ],
    },
    serialNumber: "0E:B6:EA:B4:18:C8:73:D8:F3:C0:31:DC:DD:DF:18:B0",
    signature: {
      name: "SHA-256 with RSA Encryption",
      type: "1.2.840.113549.1.1.11",
    },
    subjectPublicKeyInfo: {
      e: 65537,
      kty: "RSA",
      n:
        "CE:9F:85:CA:39:30:30:B7:F6:98:69:B4:9C:10:5D:50:3B:25:63:D0:E5:68:D4:D9:A5:CA:2C:D6:35:95:B2:3E:0D:29:8B:9D:E0:81:4A:04:F7:C0:9E:35:49:33:FB:AB:1C:11:8A:96:35:8E:A5:DE:A2:81:E7:AA:49:24:8A:8D:42:6A:3D:36:85:8E:F2:4D:86:FE:34:C8:8C:51:46:A8:D5:98:22:AD:B7:8B:8F:87:A9:A5:E2:D7:F1:FF:69:61:60:6B:39:35:AA:4C:B2:00:E4:10:03:FA:79:E9:B1:BD:9B:93:A4:FC:80:4C:FC:16:67:2E:A5:49:2C:62:4E:C7:D8:A1:80:6D:5D:23:D0:EB:EA:F6:A9:FB:C4:1A:3D:16:AE:DE:DF:6C:11:DD:9C:C5:EE:08:C7:B8:0B:75:A6:06:DE:FC:6C:61:FD:C1:C9:C2:93:48:AB:72:AD:B9:17:D5:0C:B4:76:C4:B1:CB:E1:82:33:61:13:C4:4D:60:31:AE:EF:46:89:90:FD:9A:19:A3:C2:1B:E7:99:05:A7:A9:48:4F:A5:0E:3A:49:1D:CA:22:5D:A5:63:D7:21:96:65:B1:94:79:C2:47:A0:58:3B:09:3F:B5:EF:EE:71:34:58:C9:18:D7:ED:39:88:D6:2D:AF:36:51:86:19:67:07:0D:80:A0:C1:8D:23:EB:6C:05:72:D0:29:E6:5F:58:59:94:DF:46:E1:93:35:FD:F6:99:AF:21:82:77:7F:57:D0:18:B6:A8:E3:89:D0:12:37:64:9C:8B:E9:9B:41:CC:82:F6:A0:60:29:D0:56:79:E1:25:2B:73:C9:8C:F7:DB:87:E5:58:B3:D2:A7:9E:CE:41:E3:4C:B6:BE:8E:E5:6D:07:75:6C:A1:51:95:3E:0F:84:7A:C0:E6:D8:40:C6:79:6E:26:23:46:1B:40:42:33:20:F0:45:50:11:F6:73:11:DA:F4:58:63:B9:25:11:CB:1F:2A:2D:F2:D1:2B:5C:CF:43:88:5E:5C:09:BC:DF:72:37:AE:A2:29:36:48:75:BE:BD:BB:8F:6A:03:22:1D:33:3D:FB:79:6B:D2:84:4E:F9:95:B0:70:CE:DF:26:F9:F5:25:F4:76:3C:32:C0:68:8D:D0:52:FE:CE:2E:14:87:DF:65:1F:42:C9:3E:D4:80:AA:D3:99:B6:1F:04:B1:88:0B:E2:0D:19:79:0D:EE:BA:30:46:43:76:FB:B4:DE:C5:00:41:31:EF:5A:7C:34:32:BE:C9:81:B8:ED:9F:40:DE:50:A2:D8:C2:C4:56:83:EB:29:AA:81:53:24:75:86:6D:BF:51:21:BF:B7:97:17:AF:EE:72:2A:39",
      keysize: 4096,
    },
    unsupportedExtensions: [],
    version: "3",
  },
  {
    ext: {
      aia: {
        descriptions: [
          {
            location: "http://ocsp.digicert.com",
            method: "Online Certificate Status Protocol (OCSP)",
          },
        ],
        critical: false,
      },
      aKID: {
        critical: false,
        id: "B1:3E:C3:69:03:F8:BF:47:01:D4:98:26:1A:08:02:EF:63:64:2B:C3",
      },
      basicConstraints: { cA: true, critical: true },
      crlPoints: {
        critical: false,
        points: ["http://crl4.digicert.com/DigiCertHighAssuranceEVRootCA.crl"],
      },
      cp: {
        critical: false,
        policies: [
          {
            id: "2.5.29.32.0",
            qualifiers: [
              {
                id: "1.3.6.1.5.5.7.2.1",
                name: "Practices Statement",
                value: "https://www.digicert.com/CPS",
              },
            ],
          },
        ],
      },
      eKeyUsages: {
        critical: false,
        purposes: ["Server Authentication", "Client Authentication"],
      },
      keyUsages: {
        critical: true,
        purposes: ["Digital Signature", "Certificate Signing", "CRL Signing"],
      },
      msCrypto: { exists: false },
      ocspStaple: { critical: false, required: false },
      scts: { critical: false, timestamps: [] },
      sKID: {
        critical: false,
        id: "3D:D3:50:A5:D6:A0:AD:EE:F3:4A:60:0A:65:D3:21:D4:F8:F8:D6:0F",
      },
      san: { altNames: [], critical: false },
    },
    files: {
      pem:
        "-----BEGIN%20CERTIFICATE-----%0D%0AMIIEtjCCA56gAwIBAgIQDHmpRLCMEZUgkmFf4msdgzANBgkqhkiG9w0BAQsFADBs%0D%0AMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3%0D%0Ad3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j%0D%0AZSBFViBSb290IENBMB4XDTEzMTAyMjEyMDAwMFoXDTI4MTAyMjEyMDAwMFowdTEL%0D%0AMAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3%0D%0ALmRpZ2ljZXJ0LmNvbTE0MDIGA1UEAxMrRGlnaUNlcnQgU0hBMiBFeHRlbmRlZCBW%0D%0AYWxpZGF0aW9uIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoC%0D%0AggEBANdTpARR+JmmFkhLZyeqk0nQOe0MsLAAh/FnKIaFjI5j2ryxQDji0/XspQUY%0D%0AuD0+xZkXMuwYjPrxDKZkIYXLBxA0sFKIKx9om9KxjxKws9LniB8f7zh3VFNfgHk/%0D%0ALhqqqB5LKw2rt2O5Nbd9FLxZS99RStKh4gzikIKHaq7q12TWmFXo/a8aUGxUvBHy%0D%0A/Urynbt/DvTVvo4WiRJV2MBxNO723C3sxIclho3YIeSwTQyJ3DkmF93215SF2AQh%0D%0AcJ1vb/9cuhnhRctWVyh+HA1BV6q3uCe7seT6Ku8hI3UarS2bhjWMnHe1c63YlC3k%0D%0A8wyd7sFOYn4XwHGeLN7x+RAoGTMCAwEAAaOCAUkwggFFMBIGA1UdEwEB/wQIMAYB%0D%0AAf8CAQAwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEF%0D%0ABQcDAjA0BggrBgEFBQcBAQQoMCYwJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRp%0D%0AZ2ljZXJ0LmNvbTBLBgNVHR8ERDBCMECgPqA8hjpodHRwOi8vY3JsNC5kaWdpY2Vy%0D%0AdC5jb20vRGlnaUNlcnRIaWdoQXNzdXJhbmNlRVZSb290Q0EuY3JsMD0GA1UdIAQ2%0D%0AMDQwMgYEVR0gADAqMCgGCCsGAQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5j%0D%0Ab20vQ1BTMB0GA1UdDgQWBBQ901Cl1qCt7vNKYApl0yHU+PjWDzAfBgNVHSMEGDAW%0D%0AgBSxPsNpA/i/RwHUmCYaCALvY2QrwzANBgkqhkiG9w0BAQsFAAOCAQEAnbbQkIbh%0D%0AhgLtxaDwNBx0wY12zIYKqPBKikLWP8ipTa18CK3mtlC4ohpNiAexKSHc59rGPCHg%0D%0A4xFJcKx6HQGkyhE6V6t9VypAdP3THYUYUN9XR3WhfVUgLkc3UHKMf4Ib0mKPLQNa%0D%0A2sPIoc4sUqIAY+tzunHISScjl2SFnjgOrWNoPLpSgVh5oywM395t6zHyuqB8bPEs%0D%0A1OG9d4Q3A84ytciagRpKkk47RpqF/oOi+Z6Mo8wNXrM9zwR4jxQUezKcxwCmXMS1%0D%0AoVWNWlZopCJwqjyBcdmdqEU79OX2olHdx3ti6G8MdOu42vi/hw15UJGQmxg7kVkn%0D%0A8TUoE6smftX3eg==%0D%0A-----END%20CERTIFICATE-----%0D%0A",
    },
    fingerprint: {
      sha1: "7E:2F:3A:4F:8F:E8:FA:8A:57:30:AE:CA:02:96:96:63:7E:98:6F:3F",
      sha256:
        "40:3E:06:2A:26:53:05:91:13:28:5B:AF:80:A0:D4:AE:42:2C:84:8C:9F:78:FA:D0:1F:C9:4B:C5:B8:7F:EF:1A",
    },
    issuer: {
      cn: "DigiCert High Assurance EV Root CA",
      dn:
        "c=US, o=DigiCert Inc, ou=www.digicert.com, cn=DigiCert High Assurance EV Root CA",
      entries: [
        ["Country", "US"],
        ["Organization", "DigiCert Inc"],
        ["Organizational Unit", "www.digicert.com"],
        ["Common Name", "DigiCert High Assurance EV Root CA"],
      ],
    },
    notBefore: "10/22/2013, 10:00:00 AM (Brasilia Standard Time)",
    notBeforeUTC: "Tue, 22 Oct 2013 12:00:00 GMT",
    notAfter: "10/22/2028, 9:00:00 AM (Brasilia Standard Time)",
    notAfterUTC: "Sun, 22 Oct 2028 12:00:00 GMT",
    subject: {
      cn: "DigiCert SHA2 Extended Validation Server CA",
      dn:
        "c=US, o=DigiCert Inc, ou=www.digicert.com, cn=DigiCert SHA2 Extended Validation Server CA",
      entries: [
        ["Country", "US"],
        ["Organization", "DigiCert Inc"],
        ["Organizational Unit", "www.digicert.com"],
        ["Common Name", "DigiCert SHA2 Extended Validation Server CA"],
      ],
    },
    serialNumber: "0C:79:A9:44:B0:8C:11:95:20:92:61:5F:E2:6B:1D:83",
    signature: {
      name: "SHA-256 with RSA Encryption",
      type: "1.2.840.113549.1.1.11",
    },
    subjectPublicKeyInfo: {
      e: 65537,
      kty: "RSA",
      n:
        "D7:53:A4:04:51:F8:99:A6:16:48:4B:67:27:AA:93:49:D0:39:ED:0C:B0:B0:00:87:F1:67:28:86:85:8C:8E:63:DA:BC:B1:40:38:E2:D3:F5:EC:A5:05:18:B8:3D:3E:C5:99:17:32:EC:18:8C:FA:F1:0C:A6:64:21:85:CB:07:10:34:B0:52:88:2B:1F:68:9B:D2:B1:8F:12:B0:B3:D2:E7:88:1F:1F:EF:38:77:54:53:5F:80:79:3F:2E:1A:AA:A8:1E:4B:2B:0D:AB:B7:63:B9:35:B7:7D:14:BC:59:4B:DF:51:4A:D2:A1:E2:0C:E2:90:82:87:6A:AE:EA:D7:64:D6:98:55:E8:FD:AF:1A:50:6C:54:BC:11:F2:FD:4A:F2:9D:BB:7F:0E:F4:D5:BE:8E:16:89:12:55:D8:C0:71:34:EE:F6:DC:2D:EC:C4:87:25:86:8D:D8:21:E4:B0:4D:0C:89:DC:39:26:17:DD:F6:D7:94:85:D8:04:21:70:9D:6F:6F:FF:5C:BA:19:E1:45:CB:56:57:28:7E:1C:0D:41:57:AA:B7:B8:27:BB:B1:E4:FA:2A:EF:21:23:75:1A:AD:2D:9B:86:35:8C:9C:77:B5:73:AD:D8:94:2D:E4:F3:0C:9D:EE:C1:4E:62:7E:17:C0:71:9E:2C:DE:F1:F9:10:28:19:33",
      keysize: 2048,
    },
    unsupportedExtensions: [],
    version: "3",
  },
  {
    ext: {
      aia: { critical: false },
      aKID: {
        critical: false,
        id: "B1:3E:C3:69:03:F8:BF:47:01:D4:98:26:1A:08:02:EF:63:64:2B:C3",
      },
      basicConstraints: { cA: true, critical: true },
      cp: { critical: false },
      keyUsages: {
        critical: true,
        purposes: ["Digital Signature", "Certificate Signing", "CRL Signing"],
      },
      msCrypto: { exists: false },
      ocspStaple: { critical: false, required: false },
      scts: { critical: false, timestamps: [] },
      sKID: {
        critical: false,
        id: "B1:3E:C3:69:03:F8:BF:47:01:D4:98:26:1A:08:02:EF:63:64:2B:C3",
      },
      san: { altNames: [], critical: false },
    },
    files: {
      pem:
        "-----BEGIN%20CERTIFICATE-----%0D%0AMIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs%0D%0AMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3%0D%0Ad3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j%0D%0AZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL%0D%0AMAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3%0D%0ALmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug%0D%0ARVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm%0D%0A+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW%0D%0APNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM%0D%0AxChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB%0D%0AIk5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3%0D%0AhzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg%0D%0AEsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF%0D%0AMAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA%0D%0AFLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec%0D%0AnzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z%0D%0AeM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF%0D%0AhS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2%0D%0AYzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe%0D%0AvEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep%0D%0A+OkuE6N36B9K%0D%0A-----END%20CERTIFICATE-----%0D%0A",
    },
    fingerprint: {
      sha1: "5F:B7:EE:06:33:E2:59:DB:AD:0C:4C:9A:E6:D3:8F:1A:61:C7:DC:25",
      sha256:
        "74:31:E5:F4:C3:C1:CE:46:90:77:4F:0B:61:E0:54:40:88:3B:A9:A0:1E:D0:0B:A6:AB:D7:80:6E:D3:B1:18:CF",
    },
    issuer: {
      cn: "DigiCert High Assurance EV Root CA",
      dn:
        "c=US, o=DigiCert Inc, ou=www.digicert.com, cn=DigiCert High Assurance EV Root CA",
      entries: [
        ["Country", "US"],
        ["Organization", "DigiCert Inc"],
        ["Organizational Unit", "www.digicert.com"],
        ["Common Name", "DigiCert High Assurance EV Root CA"],
      ],
    },
    notBefore: "11/9/2006, 10:00:00 PM (Brasilia Standard Time)",
    notBeforeUTC: "Fri, 10 Nov 2006 00:00:00 GMT",
    notAfter: "11/9/2031, 9:00:00 PM (Brasilia Standard Time)",
    notAfterUTC: "Mon, 10 Nov 2031 00:00:00 GMT",
    subject: {
      cn: "DigiCert High Assurance EV Root CA",
      dn:
        "c=US, o=DigiCert Inc, ou=www.digicert.com, cn=DigiCert High Assurance EV Root CA",
      entries: [
        ["Country", "US"],
        ["Organization", "DigiCert Inc"],
        ["Organizational Unit", "www.digicert.com"],
        ["Common Name", "DigiCert High Assurance EV Root CA"],
      ],
    },
    serialNumber: "02:AC:5C:26:6A:0B:40:9B:8F:0B:79:F2:AE:46:25:77",
    signature: {
      name: "SHA-1 with RSA Encryption",
      type: "1.2.840.113549.1.1.5",
    },
    subjectPublicKeyInfo: {
      e: 65537,
      kty: "RSA",
      n:
        "C6:CC:E5:73:E6:FB:D4:BB:E5:2D:2D:32:A6:DF:E5:81:3F:C9:CD:25:49:B6:71:2A:C3:D5:94:34:67:A2:0A:1C:B0:5F:69:A6:40:B1:C4:B7:B2:8F:D0:98:A4:A9:41:59:3A:D3:DC:94:D6:3C:DB:74:38:A4:4A:CC:4D:25:82:F7:4A:A5:53:12:38:EE:F3:49:6D:71:91:7E:63:B6:AB:A6:5F:C3:A4:84:F8:4F:62:51:BE:F8:C5:EC:DB:38:92:E3:06:E5:08:91:0C:C4:28:41:55:FB:CB:5A:89:15:7E:71:E8:35:BF:4D:72:09:3D:BE:3A:38:50:5B:77:31:1B:8D:B3:C7:24:45:9A:A7:AC:6D:00:14:5A:04:B7:BA:13:EB:51:0A:98:41:41:22:4E:65:61:87:81:41:50:A6:79:5C:89:DE:19:4A:57:D5:2E:E6:5D:1C:53:2C:7E:98:CD:1A:06:16:A4:68:73:D0:34:04:13:5C:A1:71:D3:5A:7C:55:DB:5E:64:E1:37:87:30:56:04:E5:11:B4:29:80:12:F1:79:39:88:A2:02:11:7C:27:66:B7:88:B7:78:F2:CA:0A:A8:38:AB:0A:64:C2:BF:66:5D:95:84:C1:A1:25:1E:87:5D:1A:50:0B:20:12:CC:41:BB:6E:0B:51:38:B8:4B:CB",
      keysize: 2048,
    },
    unsupportedExtensions: [],
    version: "3",
  },
];

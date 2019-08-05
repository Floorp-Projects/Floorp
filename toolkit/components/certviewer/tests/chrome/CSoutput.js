export const certOutputCS = [
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
              "http://cacerts.digicert.com/DigiCertSHA2SecureServerCA.crt",
            method: "CA Issuers",
          },
        ],
        critical: false,
      },
      aKID: {
        critical: false,
        id: "0F:80:61:1C:82:31:61:D5:2F:28:E7:8D:46:38:B4:2C:E1:C6:D9:E2",
      },
      basicConstraints: { cA: false, critical: true },
      crlPoints: {
        critical: false,
        points: [
          "http://crl3.digicert.com/ssca-sha2-g6.crl",
          "http://crl4.digicert.com/ssca-sha2-g6.crl",
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
            value: "2.16.840.1.114412.1.1",
          },
          {
            id: "2.23.140.1.2.2",
            name: "Certificate Type",
            value: "Organization Validation",
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
            timestamp: "11/5/2018, 8:55:28 PM (Brasilia Standard Time)",
            timestampUTC: "Mon, 05 Nov 2018 22:55:28 GMT",
            version: 1,
          },
          {
            logId:
              "87:75:BF:E7:59:7C:F8:8C:43:99:5F:BD:F3:6E:FF:56:8D:47:56:36:FF:4A:B5:60:C1:B4:EA:FF:5E:A0:83:0F",
            name: "DigiCert Server 2",
            signatureAlgorithm: "SHA-256 ECDSA",
            timestamp: "11/5/2018, 8:55:28 PM (Brasilia Standard Time)",
            timestampUTC: "Mon, 05 Nov 2018 22:55:28 GMT",
            version: 1,
          },
        ],
      },
      sKID: {
        critical: false,
        id: "DA:52:BD:21:9C:37:65:53:FC:1F:53:75:0F:1E:5F:07:9B:A3:AD:3F",
      },
      san: {
        altNames: [
          ["DNS Name", "www.mozilla.org"],
          ["DNS Name", "mozilla.org"],
        ],
        critical: false,
      },
    },
    files: {
      pem:
        "-----BEGIN%20CERTIFICATE-----%0D%0AMIIGRjCCBS6gAwIBAgIQDJduPkI49CDWPd+G7+u6kDANBgkqhkiG9w0BAQsFADBN%0D%0AMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMScwJQYDVQQDEx5E%0D%0AaWdpQ2VydCBTSEEyIFNlY3VyZSBTZXJ2ZXIgQ0EwHhcNMTgxMTA1MDAwMDAwWhcN%0D%0AMTkxMTEzMTIwMDAwWjCBgzELMAkGA1UEBhMCVVMxEzARBgNVBAgTCkNhbGlmb3Ju%0D%0AaWExFjAUBgNVBAcTDU1vdW50YWluIFZpZXcxHDAaBgNVBAoTE01vemlsbGEgQ29y%0D%0AcG9yYXRpb24xDzANBgNVBAsTBldlYk9wczEYMBYGA1UEAxMPd3d3Lm1vemlsbGEu%0D%0Ab3JnMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuKruymkkmkqCJh7Q%0D%0AjmXlUOBcLFRyw5LG/vUUWVrsxC2gsbR8WJq+cYoYBpoNVStKrO4U2rBh1GEbccvT%0D%0A6qKOQI+pjjDxx9cmRdubGTGp8L0MF1ohVvhIvYLumOEoRDDPU4PvGJjGhek/ojve%0D%0AdPWe8dhciHkxOC2qPFZvVFMwg1/o/b80147BwZQmzB18mnHsmcyKlpsCN8pxw86u%0D%0Aao9Iun8gZQrsllW64rTZlRR56pHdAcuGAoZjYZxwS9Z+lvrSjEgrddemWyGGalqy%0D%0AFp1rXlVM1Tf4/IYWAQXTgTUN303u3xMjss7QK7eUDsACRxiWPLW9XQDd1c+yvaYJ%0D%0AKzgJ2wIDAQABo4IC6TCCAuUwHwYDVR0jBBgwFoAUD4BhHIIxYdUvKOeNRji0LOHG%0D%0A2eIwHQYDVR0OBBYEFNpSvSGcN2VT/B9TdQ8eXwebo60/MCcGA1UdEQQgMB6CD3d3%0D%0Ady5tb3ppbGxhLm9yZ4ILbW96aWxsYS5vcmcwDgYDVR0PAQH/BAQDAgWgMB0GA1Ud%0D%0AJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjBrBgNVHR8EZDBiMC+gLaArhilodHRw%0D%0AOi8vY3JsMy5kaWdpY2VydC5jb20vc3NjYS1zaGEyLWc2LmNybDAvoC2gK4YpaHR0%0D%0AcDovL2NybDQuZGlnaWNlcnQuY29tL3NzY2Etc2hhMi1nNi5jcmwwTAYDVR0gBEUw%0D%0AQzA3BglghkgBhv1sAQEwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNl%0D%0AcnQuY29tL0NQUzAIBgZngQwBAgIwfAYIKwYBBQUHAQEEcDBuMCQGCCsGAQUFBzAB%0D%0AhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wRgYIKwYBBQUHMAKGOmh0dHA6Ly9j%0D%0AYWNlcnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydFNIQTJTZWN1cmVTZXJ2ZXJDQS5j%0D%0AcnQwDAYDVR0TAQH/BAIwADCCAQIGCisGAQQB1nkCBAIEgfMEgfAA7gB1AKS5CZC0%0D%0AGFgUh7sTosxncAo8NZgE+RvfuON3zQ7IDdwQAAABZuYWiHwAAAQDAEYwRAIgZnMS%0D%0AH1JdG6NASHWTwD0mlP/zbr0hzP263c02Ym0DU64CIEe4QHJDP47j0b6oTFu6RrZz%0D%0A1NQ9cq8Az1KnMKRuaFAlAHUAh3W/51l8+IxDmV+9827/Vo1HVjb/SrVgwbTq/16g%0D%0Agw8AAAFm5haJAgAABAMARjBEAiAxGLXkUaOAkZhXNeNR3pWyahZeKmSaMXadgu18%0D%0ASfK1ZAIgKtwu5eGxK76rgaszLCZ9edBIjuU0DKorzPUuxUXFY0QwDQYJKoZIhvcN%0D%0AAQELBQADggEBAKLJAFO3wuaP5MM/ed1lhk5Uc2aDokhcM7XyvdhEKSHbgPhcgMoT%0D%0A9YIVoPa70gNC6KHcwoXu0g8wt7X6Vm1ql/68G5q844kFuC6JPl4LVT9mciD+VW6b%0D%0AHUSXD9xifL9DqdJ0Ic0SllTlM+oq5aAeOxUQGXhXIqj6fSQv9fQN6mXxQIoc/gjx%0D%0Ateskq/Vl8YmY1FIZP9Bh7g27kxZ9GAAGQtjTL03RzKAuSg6yeImYVdQWasc7UPnB%0D%0AXlRAzZ8+OJThUbzK16a2CI3Rg4agKSJk+uA47h1/ImmngpFLRb/MvRX6H1oWcUuy%0D%0AH6O7PZdl0YpwTpw1THIuqCGl/wpPgyQgcTM=%0D%0A-----END%20CERTIFICATE-----%0D%0A",
    },
    fingerprint: {
      sha1: "0E:95:79:81:17:5C:51:D2:76:05:AC:8E:C4:61:71:61:97:29:ED:61",
      sha256:
        "30:23:CB:EC:1B:A8:C2:D9:E7:45:05:D5:4A:E2:0F:73:E1:51:C4:2D:46:59:CE:B4:EE:E6:51:7B:9E:CB:84:7B",
    },
    issuer: {
      cn: "DigiCert SHA2 Secure Server CA",
      dn: "c=US, o=DigiCert Inc, cn=DigiCert SHA2 Secure Server CA",
      entries: [
        ["Country", "US"],
        ["Organization", "DigiCert Inc"],
        ["Common Name", "DigiCert SHA2 Secure Server CA"],
      ],
    },
    notBefore: "11/4/2018, 10:00:00 PM (Brasilia Standard Time)",
    notBeforeUTC: "Mon, 05 Nov 2018 00:00:00 GMT",
    notAfter: "11/13/2019, 10:00:00 AM (Brasilia Standard Time)",
    notAfterUTC: "Wed, 13 Nov 2019 12:00:00 GMT",
    subject: {
      cn: "www.mozilla.org",
      dn:
        "c=US, s=California, l=Mountain View, o=Mozilla Corporation, ou=WebOps, cn=www.mozilla.org",
      entries: [
        ["Country", "US"],
        ["State / Province", "California"],
        ["Locality", "Mountain View"],
        ["Organization", "Mozilla Corporation"],
        ["Organizational Unit", "WebOps"],
        ["Common Name", "www.mozilla.org"],
      ],
    },
    serialNumber: "0C:97:6E:3E:42:38:F4:20:D6:3D:DF:86:EF:EB:BA:90",
    signature: {
      name: "SHA-256 with RSA Encryption",
      type: "1.2.840.113549.1.1.11",
    },
    subjectPublicKeyInfo: {
      e: 65537,
      kty: "RSA",
      n:
        "B8:AA:EE:CA:69:24:9A:4A:82:26:1E:D0:8E:65:E5:50:E0:5C:2C:54:72:C3:92:C6:FE:F5:14:59:5A:EC:C4:2D:A0:B1:B4:7C:58:9A:BE:71:8A:18:06:9A:0D:55:2B:4A:AC:EE:14:DA:B0:61:D4:61:1B:71:CB:D3:EA:A2:8E:40:8F:A9:8E:30:F1:C7:D7:26:45:DB:9B:19:31:A9:F0:BD:0C:17:5A:21:56:F8:48:BD:82:EE:98:E1:28:44:30:CF:53:83:EF:18:98:C6:85:E9:3F:A2:3B:DE:74:F5:9E:F1:D8:5C:88:79:31:38:2D:AA:3C:56:6F:54:53:30:83:5F:E8:FD:BF:34:D7:8E:C1:C1:94:26:CC:1D:7C:9A:71:EC:99:CC:8A:96:9B:02:37:CA:71:C3:CE:AE:6A:8F:48:BA:7F:20:65:0A:EC:96:55:BA:E2:B4:D9:95:14:79:EA:91:DD:01:CB:86:02:86:63:61:9C:70:4B:D6:7E:96:FA:D2:8C:48:2B:75:D7:A6:5B:21:86:6A:5A:B2:16:9D:6B:5E:55:4C:D5:37:F8:FC:86:16:01:05:D3:81:35:0D:DF:4D:EE:DF:13:23:B2:CE:D0:2B:B7:94:0E:C0:02:47:18:96:3C:B5:BD:5D:00:DD:D5:CF:B2:BD:A6:09:2B:38:09:DB",
      keysize: 2048,
    },
    unsupportedExtensions: [],
    version: "3",
  },
];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const handshakeArray = [
  {
    label: "Protocol",
    info: "TLS 1.2",
  },
  {
    label: "Cipher Suite",
    info: "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
  },
  {
    label: "Key Exchange Group",
    info: "P256",
  },
  {
    label: "Signature Scheme",
    info: "RSA-PKCS1-SHA512",
  },
];

const certArray = [
  [
    {
      sectionTitle: "Subject Name",
      sectionItems: [
        {
          label: "Common Name",
          info: "developer.mozilla.org",
        },
      ],
    },
    {
      sectionTitle: "Issuer Name",
      sectionItems: [
        {
          label: "Country",
          info: "US",
        },
        {
          label: "Organization",
          info: "Amazon",
        },
        {
          label: "Organizational Unit",
          info: "Server CA 1B",
        },
        {
          label: "Common Name",
          info: "Amazon",
        },
      ],
    },
    {
      sectionTitle: "Validity",
      sectionItems: [
        {
          label: "Not Before",
          info: "5/14/2019, 9:00:00 PM (Atlantic Daylight Time)",
        },
        {
          label: "Not After",
          info: "6/15/2020, 9:00:00 AM (Atlantic Daylight Time)",
        },
      ],
    },
    {
      sectionTitle: "Subject Alt Names",
      sectionItems: [
        {
          label: "DNS Name",
          info: "developer.mozilla.org",
        },
        {
          label: "DNS Name",
          info: "beta.developer.mozilla.org",
        },
        {
          label: "DNS Name",
          info: "developer-prod.mdn.mozit.cloud",
        },
        {
          label: "DNS Name",
          info: "wiki.developer.mozilla.org",
        },
      ],
    },
    {
      sectionTitle: "Public Key Info",
      sectionItems: [
        {
          label: "Algorithm",
          info: "RSA",
        },
        {
          label: "Key Size",
          info: "2048 bits",
        },
        {
          label: "Exponent",
          info: "65537",
        },
        {
          label: "Modulus",
          info:
            "8B:FF:8A:9E:9E:2B:11:68:78:02:95:57:B6:84:F7:F3:32:46:BE:06:41:29:5B:AF:13:D7:93:28:4A:FC:8D:33:C9:07:BC:C5:CE:45:F5:60:42:A3:65:07:19:69:B8:67:97:9C:DB:B3:A7:67:D6:7A:57:BA:82:4E:63:83:33:B9:64:A1:56:1C:8A:EF:9F:7B:74:08:3F:D0:9B:E5:39:80:1C:C3:5D:4D:1B:4F:4A:23:BE:B5:BC:DD:18:5E:1D:CE:27:C8:7B:F7:5E:E6:9C:C3:E7:69:50:45:D1:BE:01:71:A3:61:19:6D:7F:B6:6E:4B:C0:E5:11:B0:0D:01:D3:5C:66:B1:1D:61:7D:BB:43:E4:40:63:D8:C5:82:18:6B:28:24:15:39:6A:82:4F:60:3F:66:6E:23:86:2A:84:E1:34:70:AE:06:2D:92:A7:84:80:AD:6F:6F:24:52:FA:7B:E8:C2:CD:E2:55:2E:AE:27:07:04:D4:B6:F1:EC:80:2D:D1:B2:E1:74:BE:ED:D4:04:8C:D8:06:44:CC:F9:6C:4E:64:68:35:38:48:59:F7:45:49:BF:34:EE:DD:55:C6:1A:EB:61:1F:4A:FA:30:3F:73:8B:36:A8:90:6E:CB:2E:58:8F:9C:78:0A:AE:4E:45:A0:30:61:5A:6A:F8:A3:32:92:E3",
        },
      ],
    },
  ],
  [
    {
      sectionTitle: "Subject Name",
      sectionItems: [
        {
          label: "Common Name",
          info: "developer.mozilla.org 2",
        },
      ],
    },
    {
      sectionTitle: "Issuer Name",
      sectionItems: [
        {
          label: "Country",
          info: "US 2",
        },
        {
          label: "Organization",
          info: "Amazon",
        },
        {
          label: "Organizational Unit",
          info: "Server CA 1B",
        },
        {
          label: "Common Name",
          info: "Amazon",
        },
      ],
    },
    {
      sectionTitle: "Validity",
      sectionItems: [
        {
          label: "Not Before",
          info: "5/14/2019, 9:00:00 PM (Atlantic Daylight Time) 2",
        },
        {
          label: "Not After",
          info: "6/15/2020, 9:00:00 AM (Atlantic Daylight Time)",
        },
      ],
    },
    {
      sectionTitle: "Subject Alt Names",
      sectionItems: [
        {
          label: "DNS Name",
          info: "developer.mozilla.org 2",
        },
        {
          label: "DNS Name",
          info: "beta.developer.mozilla.org",
        },
        {
          label: "DNS Name",
          info: "developer-prod.mdn.mozit.cloud",
        },
        {
          label: "DNS Name",
          info: "wiki.developer.mozilla.org",
        },
      ],
    },
    {
      sectionTitle: "Public Key Info",
      sectionItems: [
        {
          label: "Algorithm",
          info: "RSA 2",
        },
        {
          label: "Key Size",
          info: "2048 bits",
        },
        {
          label: "Exponent",
          info: "65537",
        },
        {
          label: "Modulus",
          info:
            "8B:FF:8A:9E:9E:2B:11:68:78:02:95:57:B6:84:F7:F3:32:46:BE:06:41:29:5B:AF:13:D7:93:28:4A:FC:8D:33:C9:07:BC:C5:CE:45:F5:60:42:A3:65:07:19:69:B8:67:97:9C:DB:B3:A7:67:D6:7A:57:BA:82:4E:63:83:33:B9:64:A1:56:1C:8A:EF:9F:7B:74:08:3F:D0:9B:E5:39:80:1C:C3:5D:4D:1B:4F:4A:23:BE:B5:BC:DD:18:5E:1D:CE:27:C8:7B:F7:5E:E6:9C:C3:E7:69:50:45:D1:BE:01:71:A3:61:19:6D:7F:B6:6E:4B:C0:E5:11:B0:0D:01:D3:5C:66:B1:1D:61:7D:BB:43:E4:40:63:D8:C5:82:18:6B:28:24:15:39:6A:82:4F:60:3F:66:6E:23:86:2A:84:E1:34:70:AE:06:2D:92:A7:84:80:AD:6F:6F:24:52:FA:7B:E8:C2:CD:E2:55:2E:AE:27:07:04:D4:B6:F1:EC:80:2D:D1:B2:E1:74:BE:ED:D4:04:8C:D8:06:44:CC:F9:6C:4E:64:68:35:38:48:59:F7:45:49:BF:34:EE:DD:55:C6:1A:EB:61:1F:4A:FA:30:3F:73:8B:36:A8:90:6E:CB:2E:58:8F:9C:78:0A:AE:4E:45:A0:30:61:5A:6A:F8:A3:32:92:E3",
        },
      ],
    },
  ],
];

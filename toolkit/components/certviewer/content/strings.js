/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const strings = {
  ux: {
    upload: "Upload Certificate",
  },

  names: {
    // Directory Pilot Attributes
    "0.9.2342.19200300.100.1.1": {
      short: "uid",
      long: "User ID",
    },
    "0.9.2342.19200300.100.1.25": {
      short: "dc",
      long: "Domain Component",
    },

    // PKCS-9
    "1.2.840.113549.1.9.1": {
      short: "e",
      long: "Email Address",
    },

    // Incorporated Locations
    "1.3.6.1.4.1.311.60.2.1.1": {
      short: undefined,
      long: "Inc. Locality",
    },
    "1.3.6.1.4.1.311.60.2.1.2": {
      short: undefined,
      long: "Inc. State / Province",
    },
    "1.3.6.1.4.1.311.60.2.1.3": {
      short: undefined,
      long: "Inc. Country",
    },

    // microsoft cryptographic extensions
    "1.3.6.1.4.1.311.21.7": {
      name: {
        short: "Certificate Template",
        long: "Microsoft Certificate Template",
      },
    },
    "1.3.6.1.4.1.311.21.10": {
      name: {
        short: "Certificate Policies",
        long: "Microsoft Certificate Policies",
      },
    },

    // certificate extensions
    "1.3.6.1.4.1.11129.2.4.2": {
      name: {
        short: "Embedded SCTs",
        long: "Embedded Signed Certificate Timestamps",
      },
    },
    "1.3.6.1.5.5.7.1.1": {
      name: {
        short: undefined,
        long: "Authority Information Access",
      },
    },
    "1.3.6.1.5.5.7.1.24": {
      name: {
        short: "OCSP Stapling",
        long: "Online Certificate Status Protocol Stapling",
      },
    },

    // X.500 attribute types
    "2.5.4.1": {
      short: undefined,
      long: "Aliased Entry",
    },
    "2.5.4.2": {
      short: undefined,
      long: "Knowledge Information",
    },
    "2.5.4.3": {
      short: "cn",
      long: "Common Name",
    },
    "2.5.4.4": {
      short: "sn",
      long: "Surname",
    },
    "2.5.4.5": {
      short: "serialNumber",
      long: "Serial Number",
    },
    "2.5.4.6": {
      short: "c",
      long: "Country",
    },
    "2.5.4.7": {
      short: "l",
      long: "Locality",
    },
    "2.5.4.8": {
      short: "s",
      long: "State / Province",
    },
    "2.5.4.9": {
      short: "street",
      long: "Stress Address",
    },
    "2.5.4.10": {
      short: "o",
      long: "Organization",
    },
    "2.5.4.11": {
      short: "ou",
      long: "Organizational Unit",
    },
    "2.5.4.12": {
      short: "t",
      long: "Title",
    },
    "2.5.4.13": {
      short: "description",
      long: "Description",
    },
    "2.5.4.14": {
      short: undefined,
      long: "Search Guide",
    },
    "2.5.4.15": {
      short: undefined,
      long: "Business Category",
    },
    "2.5.4.16": {
      short: undefined,
      long: "Postal Address",
    },
    "2.5.4.17": {
      short: "postalCode",
      long: "Postal Code",
    },
    "2.5.4.18": {
      short: "POBox",
      long: "PO Box",
    },
    "2.5.4.19": {
      short: undefined,
      long: "Physical Delivery Office Name",
    },
    "2.5.4.20": {
      short: "phone",
      long: "Phone Number",
    },
    "2.5.4.21": {
      short: undefined,
      long: "Telex Number",
    },
    "2.5.4.22": {
      short: undefined,
      long: "Teletex Terminal Identifier",
    },
    "2.5.4.23": {
      short: undefined,
      long: "Fax Number",
    },
    "2.5.4.24": {
      short: undefined,
      long: "X.121 Address",
    },
    "2.5.4.25": {
      short: undefined,
      long: "International ISDN Number",
    },
    "2.5.4.26": {
      short: undefined,
      long: "Registered Address",
    },
    "2.5.4.27": {
      short: undefined,
      long: "Destination Indicator",
    },
    "2.5.4.28": {
      short: undefined,
      long: "Preferred Delivery Method",
    },
    "2.5.4.29": {
      short: undefined,
      long: "Presentation Address",
    },
    "2.5.4.30": {
      short: undefined,
      long: "Supported Application Context",
    },
    "2.5.4.31": {
      short: undefined,
      long: "Member",
    },
    "2.5.4.32": {
      short: undefined,
      long: "Owner",
    },
    "2.5.4.33": {
      short: undefined,
      long: "Role Occupant",
    },
    "2.5.4.34": {
      short: undefined,
      long: "See Also",
    },
    "2.5.4.35": {
      short: undefined,
      long: "User Password",
    },
    "2.5.4.36": {
      short: undefined,
      long: "User Certificate",
    },
    "2.5.4.37": {
      short: undefined,
      long: "CA Certificate",
    },
    "2.5.4.38": {
      short: undefined,
      long: "Authority Revocation List",
    },
    "2.5.4.39": {
      short: undefined,
      long: "Certificate Revocation List",
    },
    "2.5.4.40": {
      short: undefined,
      long: "Cross-certificate Pair",
    },
    "2.5.4.41": {
      short: undefined,
      long: "Name",
    },
    "2.5.4.42": {
      short: "g",
      long: "Given Name",
    },
    "2.5.4.43": {
      short: "i",
      long: "Initials",
    },
    "2.5.4.44": {
      short: undefined,
      long: "Generation Qualifier",
    },
    "2.5.4.45": {
      short: undefined,
      long: "Unique Identifier",
    },
    "2.5.4.46": {
      short: undefined,
      long: "DN Qualifier",
    },
    "2.5.4.47": {
      short: undefined,
      long: "Enhanced Search Guide",
    },
    "2.5.4.48": {
      short: undefined,
      long: "Protocol Information",
    },
    "2.5.4.49": {
      short: "dn",
      long: "Distinguished Name",
    },
    "2.5.4.50": {
      short: undefined,
      long: "Unique Member",
    },
    "2.5.4.51": {
      short: undefined,
      long: "House Identifier",
    },
    "2.5.4.52": {
      short: undefined,
      long: "Supported Algorithms",
    },
    "2.5.4.53": {
      short: undefined,
      long: "Delta Revocation List",
    },
    "2.5.4.58": {
      short: undefined,
      long: "Attribute Certificate Attribute", // huh
    },
    "2.5.4.65": {
      short: undefined,
      long: "Pseudonym",
    },

    // extensions
    "2.5.29.14": {
      name: {
        short: "Subject Key ID",
        long: "Subject Key Identifier",
      },
    },
    "2.5.29.15": {
      name: {
        short: undefined,
        long: "Key Usages",
      },
    },
    "2.5.29.17": {
      name: {
        short: "Subject Alt Names",
        long: "Subject Alternative Names",
      },
    },
    "2.5.29.19": {
      name: {
        short: undefined,
        long: "Basic Constraints",
      },
    },
    "2.5.29.31": {
      name: {
        short: "CRL Endpoints",
        long: "Certificate Revocation List Endpoints",
      },
    },
    "2.5.29.32": {
      name: {
        short: undefined,
        long: "Certificate Policies",
      },
    },
    "2.5.29.35": {
      name: {
        short: "Authority Key ID",
        long: "Authority Key Identifier",
      },
    },
    "2.5.29.37": {
      name: {
        short: undefined,
        long: "Extended Key Usages",
      },
    },
  },

  keyUsages: [
    "CRL Signing",
    "Certificate Signing",
    "Key Agreement",
    "Data Encipherment",
    "Key Encipherment",
    "Non-Repudiation",
    "Digital Signature",
  ],

  san: [
    "Other Name",
    "RFC 822 Name",
    "DNS Name",
    "X.400 Address",
    "Directory Name",
    "EDI Party Name",
    "URI",
    "IP Address",
    "Registered ID",
  ],

  eKU: {
    "1.3.6.1.4.1.311.10.3.1": "Certificate Trust List (CTL) Signing",
    "1.3.6.1.4.1.311.10.3.2": "Timestamp Signing",
    "1.3.6.1.4.1.311.10.3.4": "EFS Encryption",
    "1.3.6.1.4.1.311.10.3.4.1": "EFS Recovery",
    "1.3.6.1.4.1.311.10.3.5":
      "Windows Hardware Quality Labs (WHQL) Cryptography",
    "1.3.6.1.4.1.311.10.3.7": "Windows NT 5 Cryptography",
    "1.3.6.1.4.1.311.10.3.8": "Windows NT Embedded Cryptography",
    "1.3.6.1.4.1.311.10.3.10": "Qualified Subordination",
    "1.3.6.1.4.1.311.10.3.11": "Escrowed Key Recovery",
    "1.3.6.1.4.1.311.10.3.12": "Document Signing",
    "1.3.6.1.4.1.311.10.5.1": "Digital Rights Management",
    "1.3.6.1.4.1.311.10.6.1": "Key Pack Licenses",
    "1.3.6.1.4.1.311.10.6.2": "License Server",
    "1.3.6.1.4.1.311.20.2.1": "Enrollment Agent",
    "1.3.6.1.4.1.311.20.2.2": "Smartcard Login",
    "1.3.6.1.4.1.311.21.5": "Certificate Authority Private Key Archival",
    "1.3.6.1.4.1.311.21.6": "Key Recovery Agent",
    "1.3.6.1.4.1.311.21.19": "Directory Service Email Replication",
    "1.3.6.1.5.5.7.3.1": "Server Authentication",
    "1.3.6.1.5.5.7.3.2": "Client Authentication",
    "1.3.6.1.5.5.7.3.3": "Code Signing",
    "1.3.6.1.5.5.7.3.4": "E-mail Protection",
    "1.3.6.1.5.5.7.3.5": "IPsec End System",
    "1.3.6.1.5.5.7.3.6": "IPsec Tunnel",
    "1.3.6.1.5.5.7.3.7": "IPSec User",
    "1.3.6.1.5.5.7.3.8": "Timestamping",
    "1.3.6.1.5.5.7.3.9": "OCSP Signing",
    "1.3.6.1.5.5.8.2.2": "Internet Key Exchange (IKE)",
  },

  signature: {
    "1.2.840.113549.1.1.4": "MD5 with RSA Encryption",
    "1.2.840.113549.1.1.5": "SHA-1 with RSA Encryption",
    "1.2.840.113549.1.1.11": "SHA-256 with RSA Encryption",
    "1.2.840.113549.1.1.12": "SHA-384 with RSA Encryption",
    "1.2.840.113549.1.1.13": "SHA-512 with RSA Encryption",
    "1.2.840.10040.4.3": "DSA with SHA-1",
    "2.16.840.1.101.3.4.3.2": "DSA with SHA-256",
    "1.2.840.10045.4.1": "ECDSA with SHA-1",
    "1.2.840.10045.4.3.2": "ECDSA with SHA-256",
    "1.2.840.10045.4.3.3": "ECDSA with SHA-384",
    "1.2.840.10045.4.3.4": "ECDSA with SHA-512",
  },

  aia: {
    "1.3.6.1.5.5.7.48.1": "Online Certificate Status Protocol (OCSP)",
    "1.3.6.1.5.5.7.48.2": "CA Issuers",
  },

  // this includes qualifiers as well
  cps: {
    "1.3.6.1.4.1": {
      name: "Statement Identifier",
      value: undefined,
    },
    "1.3.6.1.5.5.7.2.1": {
      name: "Practices Statement",
      value: undefined,
    },
    "1.3.6.1.5.5.7.2.2": {
      name: "User Notice",
      value: undefined,
    },
    "2.16.840": {
      name: "ANSI Organizational Identifier",
      value: undefined,
    },
    "2.23.140.1.1": {
      name: "Certificate Type",
      value: "Extended Validation",
    },
    "2.23.140.1.2.1": {
      name: "Certificate Type",
      value: "Domain Validation",
    },
    "2.23.140.1.2.2": {
      name: "Certificate Type",
      value: "Organization Validation",
    },
    "2.23.140.1.2.3": {
      name: "Certificate Type",
      value: "Individual Validation",
    },
    "2.23.140.1.3": {
      name: "Certificate Type",
      value: "Extended Validation (Code Signing)",
    },
    "2.23.140.1.31": {
      name: "Certificate Type",
      value: ".onion Extended Validation",
    },
    "2.23.140.2.1": {
      name: "Certificate Type",
      value: "Test Certificate",
    },
  },

  microsoftCertificateTypes: {
    Administrator: "Administrator",
    CA: "Root Certification Authority",
    CAExchange: "CA Exchange",
    CEPEncryption: "CEP Encryption",
    CertificateRequestAgent: "Certificate Request Agent",
    ClientAuth: "Authenticated Session",
    CodeSigning: "Code Signing",
    CrossCA: "Cross Certification Authority",
    CTLSigning: "Trust List Signing",
    DirectoryEmailReplication: "Directory Email Replication",
    DomainController: "Domain Controller",
    DomainControllerAuthentication: "Domain Controller Authentication",
    EFS: "Basic EFS",
    EFSRecovery: "EFS Recovery Agent",
    EnrollmentAgent: "Enrollment Agent",
    EnrollmentAgentOffline: "Exchange Enrollment Agent (Offline request)",
    ExchangeUser: "Exchange User",
    ExchangeUserSignature: "Exchange Signature Only",
    IPSECIntermediateOffline: "IPSec (Offline request)",
    IPSECIntermediateOnline: "IPSEC",
    KerberosAuthentication: "Kerberos Authentication",
    KeyRecoveryAgent: "Key Recovery Agent",
    Machine: "Computer",
    MachineEnrollmentAgent: "Enrollment Agent (Computer)",
    OCSPResponseSigning: "OCSP Response Signing",
    OfflineRouter: "Router (Offline request)",
    RASAndIASServer: "RAS and IAS Server",
    SmartcardLogon: "Smartcard Logon",
    SmartcardUser: "Smartcard User",
    SubCA: "Subordinate Certification Authority",
    User: "User",
    UserSignature: "User Signature Only",
    WebServer: "Web Server",
    Workstation: "Workstation Authentication",
  },
};

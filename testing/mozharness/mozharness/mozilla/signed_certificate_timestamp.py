#!/usr/bin/env python

import struct
import base64

class SignedCertificateTimestamp:
    """
    Represents a Signed Certificate Timestamp from a Certificate Transparency
    log, which is how the log indicates that it has seen and logged a
    certificate.  The format for SCTs in RFC 6962 is as follows:

        struct {
            Version sct_version;
            LogID id;
            uint64 timestamp;
            CtExtensions extensions;
            digitally-signed struct {
                Version sct_version;
                SignatureType signature_type = certificate_timestamp;
                uint64 timestamp;
                LogEntryType entry_type;
                select(entry_type) {
                    case x509_entry: ASN.1Cert;
                    case precert_entry: PreCert;
                } signed_entry;
               CtExtensions extensions;
            };
        } SignedCertificateTimestamp;

    Here, the "digitally-signed" is just a fixed struct encoding the algorithm
    and signature:

        struct {
            SignatureAndHashAlgorithm algorithm;
            opaque signature<0..2^16-1>;
        } DigitallySigned;

    In other words the whole serialized SCT comprises:

      - 1 octet of version = v1 = resp["sct_version"]
      - 32 octets of LogID = resp["id"]
      - 8 octets of timestamp = resp["timestamp"]
      - 2 octets of extensions length + resp["extensions"]
      - 2+2+N octets of signature

    These are built from RFC 6962 API responses, which are encoded in JSON
    object of the following form:

        {
            "sct_version": 0,
            "id": "...",
            "timestamp": ...,
            "extensions": "",
            "signature": "...",
        }

    The "signature" field contains the whole DigitallySigned struct.
    """

    # We only support SCTs from RFC 6962 logs
    SCT_VERSION = 0

    def __init__(self, response_json=None):
        self.version = SignedCertificateTimestamp.SCT_VERSION

        if response_json is not None:
            if response_json['sct_version'] is not SignedCertificateTimestamp.SCT_VERSION:
                raise Exception('Incorrect version for SCT')

            self.id = base64.b64decode(response_json['id'])
            self.timestamp = response_json['timestamp']
            self.signature = base64.b64decode(response_json['signature'])

            self.extensions = b''
            if 'extensions' in response_json:
                self.extensions = base64.b64decode(response_json['extensions'])


    @staticmethod
    def from_rfc6962(serialized):
        start = 0
        read = 1 + 32 + 8
        if len(serialized) < start + read:
            raise Exception('SCT too short for version, log ID, and timestamp')
        version, = struct.unpack('B', serialized[0])
        log_id = serialized[1:1+32]
        timestamp, = struct.unpack('!Q', serialized[1+32:1+32+8])
        start += read

        if version is not SignedCertificateTimestamp.SCT_VERSION:
            raise Exception('Incorrect version for SCT')

        read = 2
        if len(serialized) < start + read:
            raise Exception('SCT too short for extension length')
        ext_len, = struct.unpack('!H', serialized[start:start+read])
        start += read

        read = ext_len
        if len(serialized) < start + read:
            raise Exception('SCT too short for extensions')
        extensions = serialized[start:read]
        start += read

        read = 4
        if len(serialized) < start + read:
            raise Exception('SCT too short for signature header')
        alg, sig_len, = struct.unpack('!HH', serialized[start:start+read])
        start += read

        read = sig_len
        if len(serialized) < start + read:
            raise Exception('SCT too short for signature')
        sig = serialized[start:start+read]

        sct = SignedCertificateTimestamp()
        sct.id = log_id
        sct.timestamp = timestamp
        sct.extensions = extensions
        sct.signature = struct.pack('!HH', alg, sig_len) + sig
        return sct


    def to_rfc6962(self):
        version = struct.pack("B", self.version)
        timestamp = struct.pack("!Q", self.timestamp)
        ext_len = struct.pack("!H", len(self.extensions))

        return version + self.id + timestamp + \
               ext_len + self.extensions + self.signature

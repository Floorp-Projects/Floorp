#!/usr/bin/python

# Given a PEM encoded X.509 certificate, outputs
#   base64(SHA256(subject || spki))
# where `subject` is the RFC 5280 RDNSequence encoding
# the certificate's subject, and `spki` is the RFC 5280
# SubjectPublicKeyInfo field encoding the certificate's
# public key.

import sys
import base64

from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives import hashes

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <path to pem cert>")
    sys.exit(1)

with open(sys.argv[1], "r") as f:
    cert = x509.load_pem_x509_certificate(f.read().encode("utf-8"), backend=None)

subj = cert.subject.public_bytes()
spki = cert.public_key().public_bytes(
    format=serialization.PublicFormat.SubjectPublicKeyInfo,
    encoding=serialization.Encoding.DER,
)

digest = hashes.Hash(hashes.SHA256(), backend=None)
digest.update(subj)
digest.update(spki)
print(base64.b64encode(digest.finalize()).decode("utf-8"))

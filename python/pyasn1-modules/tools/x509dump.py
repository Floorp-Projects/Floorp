#!/usr/bin/python
#
# Read ASN.1/PEM X.509 certificates on stdin, parse each into plain text,
# then build substrate from it
#
from pyasn1.codec.der import decoder, encoder
from pyasn1_modules import rfc2459, pem
import sys

if len(sys.argv) != 1:
    print("""Usage:
$ cat CACertificate.pem | %s
$ cat userCertificate.pem | %s""" % (sys.argv[0], sys.argv[0]))
    sys.exit(-1)
    
certType = rfc2459.Certificate()

certCnt = 0

while 1:
    idx, substrate = pem.readPemBlocksFromFile(
                        sys.stdin, ('-----BEGIN CERTIFICATE-----',
                                    '-----END CERTIFICATE-----')
                     )
    if not substrate:
        break
        
    cert, rest = decoder.decode(substrate, asn1Spec=certType)

    if rest: substrate = substrate[:-len(rest)]
        
    print(cert.prettyPrint())

    assert encoder.encode(cert, defMode=False) == substrate or \
           encoder.encode(cert, defMode=True) == substrate, \
           'cert recode fails'
        
    certCnt = certCnt + 1
    
print('*** %s PEM cert(s) de/serialized' % certCnt)

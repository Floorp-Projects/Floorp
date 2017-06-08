#!/usr/bin/python
#
# Read ASN.1/PEM X.509 certificate requests (PKCS#10 format) on stdin, 
# parse each into plain text, then build substrate from it
#
from pyasn1.codec.der import decoder, encoder
from pyasn1_modules import rfc2314, pem
import sys

if len(sys.argv) != 1:
    print("""Usage:
$ cat certificateRequest.pem | %s""" % sys.argv[0])
    sys.exit(-1)
    
certType = rfc2314.CertificationRequest()

certCnt = 0

while 1:
    idx, substrate = pem.readPemBlocksFromFile(
                      sys.stdin, ('-----BEGIN CERTIFICATE REQUEST-----',
                                  '-----END CERTIFICATE REQUEST-----')
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
    
print('*** %s PEM certificate request(s) de/serialized' % certCnt)

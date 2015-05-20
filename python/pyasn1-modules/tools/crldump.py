#!/usr/bin/python
#
# Read X.509 CRL on stdin, print them pretty and encode back into 
# original wire format.
# CRL can be generated with "openssl openssl ca -gencrl ..." commands.
#
from pyasn1_modules import rfc2459, pem
from pyasn1.codec.der import encoder, decoder
import sys

if len(sys.argv) != 1:
    print("""Usage:
$ cat crl.pem | %s""" % sys.argv[0])
    sys.exit(-1)
    
asn1Spec = rfc2459.CertificateList()

cnt = 0

while 1:
    idx, substrate = pem.readPemBlocksFromFile(sys.stdin, ('-----BEGIN X509 CRL-----', '-----END X509 CRL-----'))
    if not substrate:
        break


    key, rest = decoder.decode(substrate, asn1Spec=asn1Spec)

    if rest: substrate = substrate[:-len(rest)]
        
    print(key.prettyPrint())

    assert encoder.encode(key, defMode=False) == substrate or \
           encoder.encode(key, defMode=True) == substrate, \
           'pkcs8 recode fails'
        
    cnt = cnt + 1
 
print('*** %s CRL(s) re/serialized' % cnt)

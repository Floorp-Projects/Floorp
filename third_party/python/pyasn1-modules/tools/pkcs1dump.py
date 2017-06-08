#!/usr/bin/python
#
# Read unencrypted PKCS#1/PKIX-compliant, PEM&DER encoded private keys on
# stdin, print them pretty and encode back into original wire format.
# Private keys can be generated with "openssl genrsa|gendsa" commands.
#
from pyasn1_modules import rfc2459, rfc2437, pem
from pyasn1.codec.der import encoder, decoder
import sys

if len(sys.argv) != 1:
    print("""Usage:
$ cat rsakey.pem | %s""" % sys.argv[0])
    sys.exit(-1)
    
cnt = 0

while 1:
    idx, substrate = pem.readPemBlocksFromFile(sys.stdin, ('-----BEGIN RSA PRIVATE KEY-----', '-----END RSA PRIVATE KEY-----'), ('-----BEGIN DSA PRIVATE KEY-----', '-----END DSA PRIVATE KEY-----') )
    if not substrate:
        break

    if idx == 0:
        asn1Spec = rfc2437.RSAPrivateKey()
    elif idx == 1:
        asn1Spec = rfc2459.DSAPrivateKey()
    else:
        break

    key, rest = decoder.decode(substrate, asn1Spec=asn1Spec)

    if rest: substrate = substrate[:-len(rest)]
        
    print(key.prettyPrint())

    assert encoder.encode(key, defMode=False) == substrate or \
           encoder.encode(key, defMode=True) == substrate, \
           'pkcs8 recode fails'
        
    cnt = cnt + 1
 
print('*** %s key(s) re/serialized' % cnt)

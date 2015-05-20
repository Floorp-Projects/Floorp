#!/usr/bin/python
#
# Read ASN.1/PEM OCSP response on stdin, parse into
# plain text, then build substrate from it
#
from pyasn1.codec.der import decoder, encoder
from pyasn1_modules import rfc2560, pem
import sys

if len(sys.argv) != 1:
    print("""Usage:
$ cat ocsp-response.pem | %s""" % sys.argv[0])
    sys.exit(-1)
    
ocspReq = rfc2560.OCSPResponse()

substrate = pem.readBase64FromFile(sys.stdin)
if not substrate:
    sys.exit(0)
        
cr, rest = decoder.decode(substrate, asn1Spec=ocspReq)

print(cr.prettyPrint())

assert encoder.encode(cr, defMode=False) == substrate or \
       encoder.encode(cr, defMode=True) == substrate, \
       'OCSP request recode fails'

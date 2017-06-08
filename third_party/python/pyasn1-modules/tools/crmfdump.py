#!/usr/bin/python
# Read ASN.1/PEM X.509 CRMF request on stdin, parse into
# plain text, then build substrate from it
from pyasn1.codec.der import decoder, encoder
from pyasn1_modules import rfc2511, pem
import sys

if len(sys.argv) != 1:
    print("""Usage:
$ cat crmf.pem | %s""" % sys.argv[0])
    sys.exit(-1)
    
certReq = rfc2511.CertReqMessages()

substrate = pem.readBase64FromFile(sys.stdin)
if not substrate:
    sys.exit(0)
        
cr, rest = decoder.decode(substrate, asn1Spec=certReq)

print(cr.prettyPrint())

assert encoder.encode(cr, defMode=False) == substrate or \
       encoder.encode(cr, defMode=True) == substrate, \
       'crmf recode fails'

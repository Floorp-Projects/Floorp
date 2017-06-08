#!/usr/bin/python
#
from pyasn1.codec.der import decoder, encoder
from pyasn1_modules import rfc2560, rfc2459, pem
from pyasn1.type import univ
import sys, hashlib
try:
    import urllib2
except ImportError:
    import urllib.request as urllib2

sha1oid = univ.ObjectIdentifier((1, 3, 14, 3, 2, 26))

class ValueOnlyBitStringEncoder(encoder.encoder.BitStringEncoder):
    # These methods just do not encode tag and length fields of TLV
    def encodeTag(self, *args): return ''
    def encodeLength(self, *args): return ''
    def encodeValue(*args):
        substrate, isConstructed = encoder.encoder.BitStringEncoder.encodeValue(*args)
        # OCSP-specific hack follows: cut off the "unused bit count"
        # encoded bit-string value.
        return substrate[1:], isConstructed

    def __call__(self, bitStringValue):
        return self.encode(None, bitStringValue, defMode=1, maxChunkSize=0)

valueOnlyBitStringEncoder = ValueOnlyBitStringEncoder()

def mkOcspRequest(issuerCert, userCert):
    issuerTbsCertificate = issuerCert.getComponentByName('tbsCertificate')
    issuerSubject = issuerTbsCertificate.getComponentByName('subject')
    
    userTbsCertificate = userCert.getComponentByName('tbsCertificate')
    userIssuer = userTbsCertificate.getComponentByName('issuer')

    assert issuerSubject == userIssuer, '%s\n%s' % (
        issuerSubject.prettyPrint(), userIssuer.prettyPrint()
        )

    userIssuerHash = hashlib.sha1(
        encoder.encode(userIssuer)
        ).digest()
    
    issuerSubjectPublicKey = issuerTbsCertificate.getComponentByName('subjectPublicKeyInfo').getComponentByName('subjectPublicKey')
    
    issuerKeyHash =  hashlib.sha1(
        valueOnlyBitStringEncoder(issuerSubjectPublicKey)
        ).digest()
    
    userSerialNumber = userTbsCertificate.getComponentByName('serialNumber')

    # Build request object

    request = rfc2560.Request()
    
    reqCert = request.setComponentByName('reqCert').getComponentByName('reqCert')
    
    hashAlgorithm = reqCert.setComponentByName('hashAlgorithm').getComponentByName('hashAlgorithm')
    hashAlgorithm.setComponentByName('algorithm', sha1oid)
    
    reqCert.setComponentByName('issuerNameHash', userIssuerHash)
    reqCert.setComponentByName('issuerKeyHash', issuerKeyHash)
    reqCert.setComponentByName('serialNumber', userSerialNumber)

    ocspRequest = rfc2560.OCSPRequest()
    
    tbsRequest = ocspRequest.setComponentByName('tbsRequest').getComponentByName('tbsRequest')
    tbsRequest.setComponentByName('version', 'v1')
    
    requestList = tbsRequest.setComponentByName('requestList').getComponentByName('requestList')
    requestList.setComponentByPosition(0, request)
    
    return ocspRequest

def parseOcspRequest(ocspRequest):
    tbsRequest = ocspRequest['responseStatus']
    
    assert responseStatus == rfc2560.OCSPResponseStatus('successful'), responseStatus.prettyPrint()    
    responseBytes = ocspResponse.getComponentByName('responseBytes')
    responseType = responseBytes.getComponentByName('responseType')
    assert responseType == id_pkix_ocsp_basic, responseType.prettyPrint()
    
    response = responseBytes.getComponentByName('response')

    basicOCSPResponse, _ = decoder.decode(
        response, asn1Spec=rfc2560.BasicOCSPResponse()
        )
    
    tbsResponseData = basicOCSPResponse.getComponentByName('tbsResponseData')

    response0 = tbsResponseData.getComponentByName('responses').getComponentByPosition(0)
    
    return (
        tbsResponseData.getComponentByName('producedAt'),
        response0.getComponentByName('certID'),
        response0.getComponentByName('certStatus').getName(),
        response0.getComponentByName('thisUpdate')
        )

if len(sys.argv) != 2:
    print("""Usage:
$ cat CACertificate.pem userCertificate.pem | %s <ocsp-responder-url>""" % sys.argv[0])
    sys.exit(-1)
else:
    ocspUrl = sys.argv[1]

# Parse CA and user certificates

issuerCert, _ = decoder.decode(
    pem.readPemFromFile(sys.stdin)[1],
    asn1Spec=rfc2459.Certificate()
    )
userCert, _ = decoder.decode(
    pem.readPemFromFile(sys.stdin)[1],
    asn1Spec=rfc2459.Certificate()
    )

# Build OCSP request
    
ocspReq = mkOcspRequest(issuerCert, userCert)

# Use HTTP POST to get response (see Appendix A of RFC 2560)
# In case you need proxies, set the http_proxy env variable

httpReq = urllib2.Request(
    ocspUrl,
    encoder.encode(ocspReq),
    { 'Content-Type': 'application/ocsp-request' }
    )
httpRsp = urllib2.urlopen(httpReq).read()

# Process OCSP response
    
ocspRsp, _ = decoder.decode(httpRsp, asn1Spec=rfc2560.OCSPResponse())

producedAt, certId, certStatus, thisUpdate = parseOcspResponse(ocspRsp)

print('Certificate ID %s is %s at %s till %s\n' % (
    certId.getComponentByName('serialNumber'),
    certStatus,
    producedAt,
    thisUpdate
    ))

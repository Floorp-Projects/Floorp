#!/usr/bin/env python

import re
import rsa

def _logon( username, password ):
    # Retrive the public key
    # network stuff # req = urllib2.Request(AAA_GET_KEY, headers={'User-Agent': CLIENT_ID})
    # network stuff # response = urllib2.urlopen(req)
    # network stuff # html = response.read()
    # network stuff # print response.info() # DEBUG
    # network stuff # print html # DEBUG
    
    # replacement for network stuff #
    html="<x509PublicKey>30820122300d06092a864886f70d01010105000382010f003082010a0282010100dad8e3c084137bab285e869ae99a5de9752a095753680e9128adbe981e8141225704e558b8ee437836ec8c5460514efae61550bfdd883549981458bae388c9490b5ab43475068b169b32da446b0aae2dfbb3a5f425c74b284ced3f57ed33b30ec7b4b95a8216f8b063e34af2c84fef58bab381f3b79b80d06b687e0b5fc7aaeb311a88389ab7aa1422ae0b58956bb9e91c5cbf2b98422b05e1eacb82e29938566f6f05274294a8c596677c950ce97dcd003709d008f1ae6418ce5bf55ad2bf921318c6e31b324bdda4b4f12ff1fd86b5b71e647d1fc175aea137ba0ff869d5fbcf9ed0289fe7da3619c1204fc42d616462ac1b6a4e6ca2655d44bce039db519d0203010001</x509PublicKey>"
    # end replacement for network stuff #
    
    # This shall pick the key 
    hexstring = re.compile('<x509PublicKey[^>]*>([0-9a-fA-F]+)</x509PublicKey>')
    
    # pick the key and convert it to der format
    hex_pub_der = hexstring.search(html).group(1)
    pub_der = hex_pub_der.decode('hex')
    
    # Convert it to a public key
    pub_key = rsa.PublicKey.load_pkcs1_openssl_der(pub_der)
    
    # encode the password
    enc_pass = rsa.encrypt(password, pub_key)
    
    # and hex-encode it
    hex_pass = enc_pass.encode('hex')

# _logon('me', 'MyPass')

import timeit
timeit.timeit('_logon( "me", "MyPass" )',
              setup='from __main__ import _logon',
              number=1000)



#   ====================================================================
#
#       ELLIPTIC CURVE KEY ENCAPSULATION
#       Version 2011-01-26
#
#       Copyright (c) 2010 - 2011 | Toni Mattis
#
#   ====================================================================

"""
== Elliptic Curve Key Encapsulation ==

Keypairs
--------
Keypairs are generated using: Key.generate(bits)

The number of bits is tied to the NIST-proposed elliptic curves
and has to be 192, 224, 256, 384 or 521 (not 512!).
The result is a Key object containing public and private key.

private() is a method for checking whether the Key object is a
pure public key or also includes the private part.


Exchange
--------
Public keys have to be exported using the export()-Method without
passing an argument. The result is a string which can be safely
transmitted.

Using Key.decode(<encoded key>) the receiver obtains a new
public Key object of the sender.


Storage
-------
For storing a key, export(True) exports both private and public
key as a string. Make sure this information is properly encrypted
when stored.

Key.decode(<encoded key>) obtains the full Key object from the
encoded keypair.


Public Keys
-----------
A public Key object can perform the following cryptographic
operations:

*   validate()      Checks key integrity, i.e. after loading the
                    key from a file. Returns True if the key is
                    valid. Invalid keys should be discarded.

*   fingerprint()   Returns the public key fingerprint used to
                    identify the key. Optional arguments:
                    1. as_hex - True, if output should be formatted
                        as hexadecimal number (default: True).
                    2. hashfunc - The official name of the hash
                        function being used (default: 'sha1')
                        For supported hash functions see below.

*   keyid()         Returns a (mostly) unique Key ID, which is
                    shorter than the fingerprint. The result
                    is an integer of max. 64 bits.

*   verify()        Verifies whether the given data (argument 1)
                    matches the signature (argument 2) issued
                    by the owner of this key. A falsification
                    can have multiple causes:
                    
                    - Data, public key or signature were altered
                      during transmission/storage.
                    - The siganture was not issued by the owner
                      of this key but may be valid with another
                      key.
                    - The signature was issued for different data.
                    - The signature was issued using a different
                      hash function. Another hash function may work.
                      
                    Optionally, the name of a hash algorithm
                    can be provided. For hash names see below.

* encrypt()         Encrypts a packet of data destined for the owner
                    of this key*. After encryption only the holder
                    of this Key's private part is able to decrypt
                    the message.

Private Keys / Keypairs
-----------------------

If the key object is private, then it is a keypair consisting of
a public and a private key. Therefore all Public key operations
are supported.

Additional functions:

* sign()            Signs given data using this private key. The
                    result is a signature which can be passed as
                    argument to the verify() function in addition
                    to the data being verified.

                    As additional argument the name of the hash
                    function can be provided (defaults to 'sha256').
                    For hash names see below.

* auth_encrypt()    Performs authenticated encryption of data
                    (argument 1) for the holder of the key provided
                    as second argument. Only the receiver whose
                    public key is given is able to derypt and verify
                    the message. The message will be implicitly
                    signed using the own private key. *

* decrypt()         Decrypts a message which has been encrypted
                    using the public key of this keypair*. If
                    decryption yields random data, this can have
                    multiple causes:
                    - You were not the intended receiver, a different
                      private key may be able to decrypt it.
                    - The message was altered.
                    - Your private key is damaged.

* auth_decrypt()    Decrypts a message while verifying whether
                    it has been authentically issued by the holder
                    of the given key (argument 2). When
                    authentication failed, a
                    SecurityViolationException is thrown. Reasons
                    for this to happen are those mentioned with
                    decrypt() and verify(). *

*) The encryption used here depends on the "eccrypt" module imported
by this module. Default implementation should use RABBIT as cipher
and do the asymmetric part using an optimized El-Gamal scheme.
        
            

Hash functions
--------------
The following hash functions can be passed at the moment:

name     | hash size              | security level
         | (bits, bytes, hex digits)
---------+------------------------+----------------
'sha1'      160 / 20 / 40           medium
'sha224'    224 / 28 / 56           medium-strong
'sha256'    256 / 32 / 64           strong
'sha384'    384 / 48 / 96           very strong
'sha512'    512 / 64 / 128          very strong

'md5'       128 / 16 / 32           weak (not recommended!)


Curves
------
According to FIPS 186-3, Appendix D.1.2 there are 5 elliptic
curves recommended. All of those are strong, but those with
a higher bit number even stronger.

192 and 224 bits are sufficient for most purposes.
256 bits offer an additional magnitude of security.
    (i.e. for classified / strongly confidential data)
384 and 521 bits provide exceptionally strong security. According
    to current research they most probably keep this level for
    decades in the future.

FIPS also recommends curves over polynomial fields but actually
only prime fields are implemented here. (Because 2^521-1 is a mersenne
prime having great security characteristics, 521 bits are preferred
over a constructed 512 bit field.)
"""

from encoding import *
from eccrypt import *
import ecdsa
import hashlib
from SecurityViolationException import *

class Key:

    # --- KEY SETUP ------------------------------------------------------------

    def __init__(self, public_key, private_key = None):
        '''Create a Key(pair) from numeric keys.'''
        self._pub = public_key
        self._priv = private_key
        self._fingerprint = {}
        self._id = None

    @staticmethod
    def generate(bits):
        '''Generate a new ECDSA keypair'''
        return Key(*ecdsa.keypair(bits))

    # --- BINARY REPRESENTATION ------------------------------------------------

    def encode(self, include_private = False):
        '''Returns a strict binary representation of this Key'''
        e = Encoder().int(self.keyid(), 8)
        e.int(self._pub[0], 2).point(self._pub[1], 2)
        if include_private and self._priv:
            e.long(self._priv[1], 2)
        else:
            e.long(0, 2)
        return e.out()

    def compress(self):
        '''Returns a compact public key representation'''
        

    @staticmethod
    def decode(s):
        '''Constructs a new Key object from its binary representation'''
        kid, ksize, pub, priv = Decoder(s).int(8).int(2).point(2).long(2).out()
        k = Key((ksize, pub), (ksize, priv) if priv else None)
        if kid == k.keyid():
            return k
        else:
            raise ValueError, "Invalid Key ID"

    # --- IDENTIFICATION AND VALIDATION ----------------------------------------

    def private(self):
        '''Checks whether Key object contains private key'''
        return bool(self._priv)

    def validate(self):
        '''Checks key validity'''
        if ecdsa.validate_public_key(self._pub):
            if self._priv:          # ? validate and match private key
                return ecdsa.validate_private_key(self._priv) and \
                       ecdsa.match_keys(self._pub, self._priv)
            else:
                return True         # : everything valid
        else:
            return False

    def fingerprint(self, as_hex = True, hashfunc = 'sha1'):
        '''Get the public key fingerprint'''
        if hashfunc in self._fingerprint:
            return self._fingerprint[hashfunc] if not as_hex else \
                   self._fingerprint[hashfunc].encode("hex")
        else:
            h = hashlib.new(hashfunc, enc_point(self._pub[1]))
            d = h.digest()
            self._fingerprint[hashfunc] = d
            return d.encode("hex") if as_hex else d

    def keyid(self):
        '''Get a short, unique identifier'''
        if not self._id:
            self._id = dec_long(self.fingerprint(False, 'sha1')[:8])
        return self._id

    # --- DIGITAL SIGNATURES ---------------------------------------------------

    def sign(self, data, hashfunc = 'sha256'):
        '''Sign data using the specified hash function'''
        if self._priv:
            h = dec_long(hashlib.new(hashfunc, data).digest())
            s = ecdsa.sign(h, self._priv)
            return enc_point(s)
        else:
            raise AttributeError, "Private key needed for signing."

    def verify(self, data, sig, hashfunc = 'sha256'):
        '''Verify the signature of data using the specified hash function'''
        h = dec_long(hashlib.new(hashfunc, data).digest())
        s = dec_point(sig)
        return ecdsa.verify(h, s, self._pub)

    # --- HYBRID ENCRYPTION ----------------------------------------------------

    def encrypt(self, data):
        '''Encrypt a message using this public key'''
        ctext, mkey = encrypt(data, self._pub)
        return Encoder().point(mkey).str(ctext, 4).out()

    def decrypt(self, data):
        '''Decrypt an encrypted message using this private key'''
        mkey, ctext = Decoder(data).point().str(4).out()
        return decrypt(ctext, mkey, self._priv)
        
    # --- AUTHENTICATED ENCRYPTION ---------------------------------------------

    def auth_encrypt(self, data, receiver):
        '''Sign and encrypt a message'''
        sgn = self.sign(data)
        ctext, mkey = encrypt(data, receiver._pub)
        return Encoder().point(mkey).str(ctext, 4).str(sgn, 2).out()

    def auth_decrypt(self, data, source):
        '''Decrypt and verify a message'''
        mkey, ctext, sgn = Decoder(data).point().str(4).str(2).out()
        text = decrypt(ctext, mkey, self._priv)
        if source.verify(text, sgn):
            return text
        else:
            raise SecurityViolationException, "Invalid Signature"


if __name__ == "__main__":

    import time

    def test_overhead():
        print "sender", "receiver", "+bytes", "+enctime", "+dectime"
        for s in [192, 224, 256, 384, 521]:
            sender = Key.generate(s)
            for r in [192, 224, 256, 384, 521]:
                receiver = Key.generate(r)
                t = time.time()
                e = sender.auth_encrypt("", receiver)
                t1 = time.time() - t
                t = time.time()
                receiver.auth_decrypt(e, sender)
                t2 = time.time() - t
                print s, r, len(e), t1, t2

                
                
    

"""
Primary classes for performing signing and verification operations.

.. glossary::

    raw encoding
        Conversion of public, private keys and signatures (which in
        mathematical sense are integers or pairs of integers) to strings of
        bytes that does not use any special tags or encoding rules.
        For any given curve, all keys of the same type or signatures will be
        encoded to byte strings of the same length. In more formal sense,
        the integers are encoded as big-endian, constant length byte strings,
        where the string length is determined by the curve order (e.g.
        for NIST256p the order is 256 bits long, so the private key will be 32
        bytes long while public key will be 64 bytes long). The encoding of a
        single integer is zero-padded on the left if the numerical value is
        low. In case of public keys and signatures, which are comprised of two
        integers, the integers are simply concatenated.

    uncompressed
        The most common formatting specified in PKIX standards. Specified in
        X9.62 and SEC1 standards. The only difference between it and
        :term:`raw encoding` is the prepending of a 0x04 byte. Thus an
        uncompressed NIST256p public key encoding will be 65 bytes long.

    compressed
        The public point representation that uses half of bytes of the
        :term:`uncompressed` encoding (rounded up). It uses the first byte of
        the encoding to specify the sign of the y coordinate and encodes the
        x coordinate as-is. The first byte of the encoding is equal to
        0x02 or 0x03. Compressed encoding of NIST256p public key will be 33
        bytes long.

    hybrid
        A combination of :term:`uncompressed` and :term:`compressed` encodings.
        Both x and y coordinates are stored just as in :term:`compressed`
        encoding, but the first byte reflects the sign of the y coordinate. The
        first byte of the encoding will be equal to 0x06 or 0x7. Hybrid
        encoding of NIST256p public key will be 65 bytes long.

    PEM
        The acronym stands for Privacy Enhanced Email, but currently it is used
        primarily as the way to encode :term:`DER` objects into text that can
        be either easily copy-pasted or transferred over email.
        It uses headers like ``-----BEGIN <type of contents>-----`` and footers
        like ``-----END <type of contents>-----`` to separate multiple
        types of objects in the same file or the object from the surrounding
        comments. The actual object stored is base64 encoded.

    DER
        Distinguished Encoding Rules, the way to encode :term:`ASN.1` objects
        deterministically and uniquely into byte strings.

    ASN.1
        Abstract Syntax Notation 1 is a standard description language for
        specifying serialisation and deserialisation of data structures in a
        portable and cross-platform way.

    bytes-like object
        All the types that implement the buffer protocol. That includes
        ``str`` (only on python2), ``bytes``, ``bytesarray``, ``array.array`
        and ``memoryview`` of those objects.
        Please note that ``array.array` serialisation (converting it to byte
        string) is endianess dependant! Signature computed over ``array.array``
        of integers on a big-endian system will not be verified on a
        little-endian system and vice-versa.
"""

import binascii
from hashlib import sha1
from six import PY3, b
from . import ecdsa
from . import der
from . import rfc6979
from . import ellipticcurve
from .curves import NIST192p, find_curve
from .numbertheory import square_root_mod_prime, SquareRootError
from .ecdsa import RSZeroError
from .util import string_to_number, number_to_string, randrange
from .util import sigencode_string, sigdecode_string
from .util import oid_ecPublicKey, encoded_oid_ecPublicKey, MalformedSignature
from ._compat import normalise_bytes


__all__ = ["BadSignatureError", "BadDigestError", "VerifyingKey", "SigningKey",
           "MalformedPointError"]


class BadSignatureError(Exception):
    """
    Raised when verification of signature failed.

    Will be raised irrespective of reason of the failure:

    * the calculated or provided hash does not match the signature
    * the signature does not match the curve/public key
    * the encoding of the signature is malformed
    * the size of the signature does not match the curve of the VerifyingKey
    """

    pass


class BadDigestError(Exception):
    """Raised in case the selected hash is too large for the curve."""

    pass


class MalformedPointError(AssertionError):
    """Raised in case the encoding of private or public key is malformed."""

    pass


class VerifyingKey(object):
    """
    Class for handling keys that can verify signatures (public keys).

    :ivar ecdsa.curves.Curve curve: The Curve over which all the cryptographic
        operations will take place
    :ivar default_hashfunc: the function that will be used for hashing the
        data. Should implement the same API as hashlib.sha1
    :vartype default_hashfunc: callable
    :ivar pubkey: the actual public key
    :vartype pubkey: ecdsa.ecdsa.Public_key
    """

    def __init__(self, _error__please_use_generate=None):
        """Unsupported, please use one of the classmethods to initialise."""
        if not _error__please_use_generate:
            raise TypeError("Please use VerifyingKey.generate() to "
                            "construct me")
        self.curve = None
        self.default_hashfunc = None
        self.pubkey = None

    def __repr__(self):
        pub_key = self.to_string("compressed")
        return "VerifyingKey.from_string({0!r}, {1!r}, {2})".format(
            pub_key, self.curve, self.default_hashfunc().name)

    def __eq__(self, other):
        """Return True if the points are identical, False otherwise."""
        if isinstance(other, VerifyingKey):
            return self.curve == other.curve \
                and self.pubkey == other.pubkey
        return NotImplemented

    @classmethod
    def from_public_point(cls, point, curve=NIST192p, hashfunc=sha1,
                          validate_point=True):
        """
        Initialise the object from a Point object.

        This is a low-level method, generally you will not want to use it.

        :param point: The point to wrap around, the actual public key
        :type point: ecdsa.ellipticcurve.Point
        :param curve: The curve on which the point needs to reside, defaults
            to NIST192p
        :type curve: ecdsa.curves.Curve
        :param hashfunc: The default hash function that will be used for
            verification, needs to implement the same interface
            as hashlib.sha1
        :type hashfunc: callable
        :type bool validate_point: whether to check if the point lies on curve
            should always be used if the public point is not a result
            of our own calculation

        :raises MalformedPointError: if the public point does not lie on the
            curve

        :return: Initialised VerifyingKey object
        :rtype: VerifyingKey
        """
        self = cls(_error__please_use_generate=True)
        if not isinstance(point, ellipticcurve.PointJacobi):
            point = ellipticcurve.PointJacobi.from_affine(point)
        self.curve = curve
        self.default_hashfunc = hashfunc
        try:
            self.pubkey = ecdsa.Public_key(curve.generator, point,
                                           validate_point)
        except ecdsa.InvalidPointError:
            raise MalformedPointError("Point does not lie on the curve")
        self.pubkey.order = curve.order
        return self

    def precompute(self):
        self.pubkey.point = ellipticcurve.PointJacobi.from_affine(
            self.pubkey.point, True)

    @staticmethod
    def _from_raw_encoding(string, curve):
        """
        Decode public point from :term:`raw encoding`.

        :term:`raw encoding` is the same as the :term:`uncompressed` encoding,
        but without the 0x04 byte at the beginning.
        """
        order = curve.order
        # real assert, from_string() should not call us with different length
        assert len(string) == curve.verifying_key_length
        xs = string[:curve.baselen]
        ys = string[curve.baselen:]
        if len(xs) != curve.baselen:
            raise MalformedPointError("Unexpected length of encoded x")
        if len(ys) != curve.baselen:
            raise MalformedPointError("Unexpected length of encoded y")
        x = string_to_number(xs)
        y = string_to_number(ys)

        return ellipticcurve.PointJacobi(curve.curve, x, y, 1, order)

    @staticmethod
    def _from_compressed(string, curve):
        """Decode public point from compressed encoding."""
        if string[:1] not in (b('\x02'), b('\x03')):
            raise MalformedPointError("Malformed compressed point encoding")

        is_even = string[:1] == b('\x02')
        x = string_to_number(string[1:])
        order = curve.order
        p = curve.curve.p()
        alpha = (pow(x, 3, p) + (curve.curve.a() * x) + curve.curve.b()) % p
        try:
            beta = square_root_mod_prime(alpha, p)
        except SquareRootError as e:
            raise MalformedPointError(
                "Encoding does not correspond to a point on curve", e)
        if is_even == bool(beta & 1):
            y = p - beta
        else:
            y = beta
        return ellipticcurve.PointJacobi(curve.curve, x, y, 1, order)

    @classmethod
    def _from_hybrid(cls, string, curve, validate_point):
        """Decode public point from hybrid encoding."""
        # real assert, from_string() should not call us with different types
        assert string[:1] in (b('\x06'), b('\x07'))

        # primarily use the uncompressed as it's easiest to handle
        point = cls._from_raw_encoding(string[1:], curve)

        # but validate if it's self-consistent if we're asked to do that
        if validate_point \
                and (point.y() & 1 and string[:1] != b('\x07')
                     or (not point.y() & 1) and string[:1] != b('\x06')):
            raise MalformedPointError("Inconsistent hybrid point encoding")

        return point

    @classmethod
    def from_string(cls, string, curve=NIST192p, hashfunc=sha1,
                    validate_point=True):
        """
        Initialise the object from byte encoding of public key.

        The method does accept and automatically detect the type of point
        encoding used. It supports the :term:`raw encoding`,
        :term:`uncompressed`, :term:`compressed` and :term:`hybrid` encodings.

        Note, while the method is named "from_string" it's a misnomer from
        Python 2 days when there were no binary strings. In Python 3 the
        input needs to be a bytes-like object.

        :param string: single point encoding of the public key
        :type string: :term:`bytes-like object`
        :param curve: the curve on which the public key is expected to lie
        :type curve: ecdsa.curves.Curve
        :param hashfunc: The default hash function that will be used for
            verification, needs to implement the same interface as hashlib.sha1
        :type hashfunc: callable
        :param validate_point: whether to verify that the point lies on the
            provided curve or not, defaults to True
        :type validate_point: bool

        :raises MalformedPointError: if the public point does not lie on the
            curve or the encoding is invalid

        :return: Initialised VerifyingKey object
        :rtype: VerifyingKey
        """
        string = normalise_bytes(string)
        sig_len = len(string)
        if sig_len == curve.verifying_key_length:
            point = cls._from_raw_encoding(string, curve)
        elif sig_len == curve.verifying_key_length + 1:
            if string[:1] in (b('\x06'), b('\x07')):
                point = cls._from_hybrid(string, curve, validate_point)
            elif string[:1] == b('\x04'):
                point = cls._from_raw_encoding(string[1:], curve)
            else:
                raise MalformedPointError(
                    "Invalid X9.62 encoding of the public point")
        elif sig_len == curve.baselen + 1:
            point = cls._from_compressed(string, curve)
        else:
            raise MalformedPointError(
                "Length of string does not match lengths of "
                "any of the supported encodings of {0} "
                "curve.".format(curve.name))
        return cls.from_public_point(point, curve, hashfunc,
                                     validate_point)

    @classmethod
    def from_pem(cls, string, hashfunc=sha1):
        """
        Initialise from public key stored in :term:`PEM` format.

        The PEM header of the key should be ``BEGIN PUBLIC KEY``.

        See the :func:`~VerifyingKey.from_der()` method for details of the
        format supported.

        Note: only a single PEM object encoding is supported in provided
        string.

        :param string: text with PEM-encoded public ECDSA key
        :type string: str

        :return: Initialised VerifyingKey object
        :rtype: VerifyingKey
        """
        return cls.from_der(der.unpem(string), hashfunc=hashfunc)

    @classmethod
    def from_der(cls, string, hashfunc=sha1):
        """
        Initialise the key stored in :term:`DER` format.

        The expected format of the key is the SubjectPublicKeyInfo structure
        from RFC5912 (for RSA keys, it's known as the PKCS#1 format)::

           SubjectPublicKeyInfo {PUBLIC-KEY: IOSet} ::= SEQUENCE {
               algorithm        AlgorithmIdentifier {PUBLIC-KEY, {IOSet}},
               subjectPublicKey BIT STRING
           }

        Note: only public EC keys are supported by this method. The
        SubjectPublicKeyInfo.algorithm.algorithm field must specify
        id-ecPublicKey (see RFC3279).

        Only the named curve encoding is supported, thus the
        SubjectPublicKeyInfo.algorithm.parameters field needs to be an
        object identifier. A sequence in that field indicates an explicit
        parameter curve encoding, this format is not supported. A NULL object
        in that field indicates an "implicitlyCA" encoding, where the curve
        parameters come from CA certificate, those, again, are not supported.

        :param string: binary string with the DER encoding of public ECDSA key
        :type string: bytes-like object

        :return: Initialised VerifyingKey object
        :rtype: VerifyingKey
        """
        string = normalise_bytes(string)
        # [[oid_ecPublicKey,oid_curve], point_str_bitstring]
        s1, empty = der.remove_sequence(string)
        if empty != b"":
            raise der.UnexpectedDER("trailing junk after DER pubkey: %s" %
                                    binascii.hexlify(empty))
        s2, point_str_bitstring = der.remove_sequence(s1)
        # s2 = oid_ecPublicKey,oid_curve
        oid_pk, rest = der.remove_object(s2)
        oid_curve, empty = der.remove_object(rest)
        if empty != b"":
            raise der.UnexpectedDER("trailing junk after DER pubkey objects: %s" %
                                    binascii.hexlify(empty))
        if not oid_pk == oid_ecPublicKey:
            raise der.UnexpectedDER("Unexpected object identifier in DER "
                                    "encoding: {0!r}".format(oid_pk))
        curve = find_curve(oid_curve)
        point_str, empty = der.remove_bitstring(point_str_bitstring, 0)
        if empty != b"":
            raise der.UnexpectedDER("trailing junk after pubkey pointstring: %s" %
                                    binascii.hexlify(empty))
        # raw encoding of point is invalid in DER files
        if len(point_str) == curve.verifying_key_length:
            raise der.UnexpectedDER("Malformed encoding of public point")
        return cls.from_string(point_str, curve, hashfunc=hashfunc)

    @classmethod
    def from_public_key_recovery(cls, signature, data, curve, hashfunc=sha1,
                                 sigdecode=sigdecode_string):
        """
        Return keys that can be used as verifiers of the provided signature.

        Tries to recover the public key that can be used to verify the
        signature, usually returns two keys like that.

        :param signature: the byte string with the encoded signature
        :type signature: bytes-like object
        :param data: the data to be hashed for signature verification
        :type data: bytes-like object
        :param curve: the curve over which the signature was performed
        :type curve: ecdsa.curves.Curve
        :param hashfunc: The default hash function that will be used for
            verification, needs to implement the same interface as hashlib.sha1
        :type hashfunc: callable
        :param sigdecode: Callable to define the way the signature needs to
            be decoded to an object, needs to handle `signature` as the
            first parameter, the curve order (an int) as the second and return
            a tuple with two integers, "r" as the first one and "s" as the
            second one. See :func:`ecdsa.util.sigdecode_string` and
            :func:`ecdsa.util.sigdecode_der` for examples.
        :type sigdecode: callable

        :return: Initialised VerifyingKey objects
        :rtype: list of VerifyingKey
        """
        data = normalise_bytes(data)
        digest = hashfunc(data).digest()
        return cls.from_public_key_recovery_with_digest(
            signature, digest, curve, hashfunc=hashfunc,
            sigdecode=sigdecode)

    @classmethod
    def from_public_key_recovery_with_digest(
            cls, signature, digest, curve,
            hashfunc=sha1, sigdecode=sigdecode_string):
        """
        Return keys that can be used as verifiers of the provided signature.

        Tries to recover the public key that can be used to verify the
        signature, usually returns two keys like that.

        :param signature: the byte string with the encoded signature
        :type signature: bytes-like object
        :param digest: the hash value of the message signed by the signature
        :type digest: bytes-like object
        :param curve: the curve over which the signature was performed
        :type curve: ecdsa.curves.Curve
        :param hashfunc: The default hash function that will be used for
            verification, needs to implement the same interface as hashlib.sha1
        :type hashfunc: callable
        :param sigdecode: Callable to define the way the signature needs to
            be decoded to an object, needs to handle `signature` as the
            first parameter, the curve order (an int) as the second and return
            a tuple with two integers, "r" as the first one and "s" as the
            second one. See :func:`ecdsa.util.sigdecode_string` and
            :func:`ecdsa.util.sigdecode_der` for examples.
        :type sigdecode: callable


        :return: Initialised VerifyingKey object
        :rtype: VerifyingKey
        """
        generator = curve.generator
        r, s = sigdecode(signature, generator.order())
        sig = ecdsa.Signature(r, s)

        digest = normalise_bytes(digest)
        digest_as_number = string_to_number(digest)
        pks = sig.recover_public_keys(digest_as_number, generator)

        # Transforms the ecdsa.Public_key object into a VerifyingKey
        verifying_keys = [cls.from_public_point(pk.point, curve, hashfunc)
                          for pk in pks]
        return verifying_keys

    def _raw_encode(self):
        """Convert the public key to the :term:`raw encoding`."""
        order = self.pubkey.order
        x_str = number_to_string(self.pubkey.point.x(), order)
        y_str = number_to_string(self.pubkey.point.y(), order)
        return x_str + y_str

    def _compressed_encode(self):
        """Encode the public point into the compressed form."""
        order = self.pubkey.order
        x_str = number_to_string(self.pubkey.point.x(), order)
        if self.pubkey.point.y() & 1:
            return b('\x03') + x_str
        else:
            return b('\x02') + x_str

    def _hybrid_encode(self):
        """Encode the public point into the hybrid form."""
        raw_enc = self._raw_encode()
        if self.pubkey.point.y() & 1:
            return b('\x07') + raw_enc
        else:
            return b('\x06') + raw_enc

    def to_string(self, encoding="raw"):
        """
        Convert the public key to a byte string.

        The method by default uses the :term:`raw encoding` (specified
        by `encoding="raw"`. It can also output keys in :term:`uncompressed`,
        :term:`compressed` and :term:`hybrid` formats.

        Remember that the curve identification is not part of the encoding
        so to decode the point using :func:`~VerifyingKey.from_string`, curve
        needs to be specified.

        Note: while the method is called "to_string", it's a misnomer from
        Python 2 days when character strings and byte strings shared type.
        On Python 3 the returned type will be `bytes`.

        :return: :term:`raw encoding` of the public key (public point) on the
            curve
        :rtype: bytes
        """
        assert encoding in ("raw", "uncompressed", "compressed", "hybrid")
        if encoding == "raw":
            return self._raw_encode()
        elif encoding == "uncompressed":
            return b('\x04') + self._raw_encode()
        elif encoding == "hybrid":
            return self._hybrid_encode()
        else:
            return self._compressed_encode()

    def to_pem(self, point_encoding="uncompressed"):
        """
        Convert the public key to the :term:`PEM` format.

        The PEM header of the key will be ``BEGIN PUBLIC KEY``.

        The format of the key is described in the
        :func:`~VerifyingKey.from_der()` method.
        This method supports only "named curve" encoding of keys.

        :param str point_encoding: specification of the encoding format
            of public keys. "uncompressed" is most portable, "compressed" is
            smallest. "hybrid" is uncommon and unsupported by most
            implementations, it is as big as "uncompressed".

        :return: portable encoding of the public key
        :rtype: str
        """
        return der.topem(self.to_der(point_encoding), "PUBLIC KEY")

    def to_der(self, point_encoding="uncompressed"):
        """
        Convert the public key to the :term:`DER` format.

        The format of the key is described in the
        :func:`~VerifyingKey.from_der()` method.
        This method supports only "named curve" encoding of keys.

        :param str point_encoding: specification of the encoding format
            of public keys. "uncompressed" is most portable, "compressed" is
            smallest. "hybrid" is uncommon and unsupported by most
            implementations, it is as big as "uncompressed".

        :return: DER encoding of the public key
        :rtype: bytes
        """
        if point_encoding == "raw":
            raise ValueError("raw point_encoding not allowed in DER")
        point_str = self.to_string(point_encoding)
        return der.encode_sequence(der.encode_sequence(encoded_oid_ecPublicKey,
                                                       self.curve.encoded_oid),
                                   # 0 is the number of unused bits in the
                                   # bit string
                                   der.encode_bitstring(point_str, 0))

    def verify(self, signature, data, hashfunc=None,
               sigdecode=sigdecode_string):
        """
        Verify a signature made over provided data.

        Will hash `data` to verify the signature.

        By default expects signature in :term:`raw encoding`. Can also be used
        to verify signatures in ASN.1 DER encoding by using
        :func:`ecdsa.util.sigdecode_der`
        as the `sigdecode` parameter.

        :param signature: encoding of the signature
        :type signature: sigdecode method dependant
        :param data: data signed by the `signature`, will be hashed using
            `hashfunc`, if specified, or default hash function
        :type data: bytes like object
        :param hashfunc: The default hash function that will be used for
            verification, needs to implement the same interface as hashlib.sha1
        :type hashfunc: callable
        :param sigdecode: Callable to define the way the signature needs to
            be decoded to an object, needs to handle `signature` as the
            first parameter, the curve order (an int) as the second and return
            a tuple with two integers, "r" as the first one and "s" as the
            second one. See :func:`ecdsa.util.sigdecode_string` and
            :func:`ecdsa.util.sigdecode_der` for examples.
        :type sigdecode: callable

        :raises BadSignatureError: if the signature is invalid or malformed

        :return: True if the verification was successful
        :rtype: bool
        """
        # signature doesn't have to be a bytes-like-object so don't normalise
        # it, the decoders will do that
        data = normalise_bytes(data)

        hashfunc = hashfunc or self.default_hashfunc
        digest = hashfunc(data).digest()
        return self.verify_digest(signature, digest, sigdecode, True)

    def verify_digest(self, signature, digest, sigdecode=sigdecode_string,
                      allow_truncate=False):
        """
        Verify a signature made over provided hash value.

        By default expects signature in :term:`raw encoding`. Can also be used
        to verify signatures in ASN.1 DER encoding by using
        :func:`ecdsa.util.sigdecode_der`
        as the `sigdecode` parameter.

        :param signature: encoding of the signature
        :type signature: sigdecode method dependant
        :param digest: raw hash value that the signature authenticates.
        :type digest: bytes like object
        :param sigdecode: Callable to define the way the signature needs to
            be decoded to an object, needs to handle `signature` as the
            first parameter, the curve order (an int) as the second and return
            a tuple with two integers, "r" as the first one and "s" as the
            second one. See :func:`ecdsa.util.sigdecode_string` and
            :func:`ecdsa.util.sigdecode_der` for examples.
        :type sigdecode: callable
        :param bool allow_truncate: if True, the provided digest can have
            bigger bit-size than the order of the curve, the extra bits (at
            the end of the digest) will be truncated. Use it when verifying
            SHA-384 output using NIST256p or in similar situations.

        :raises BadSignatureError: if the signature is invalid or malformed
        :raises BadDigestError: if the provided digest is too big for the curve
            associated with this VerifyingKey and allow_truncate was not set

        :return: True if the verification was successful
        :rtype: bool
        """
        # signature doesn't have to be a bytes-like-object so don't normalise
        # it, the decoders will do that
        digest = normalise_bytes(digest)
        if allow_truncate:
            digest = digest[:self.curve.baselen]
        if len(digest) > self.curve.baselen:
            raise BadDigestError("this curve (%s) is too short "
                                 "for your digest (%d)" % (self.curve.name,
                                                           8 * len(digest)))
        number = string_to_number(digest)
        try:
            r, s = sigdecode(signature, self.pubkey.order)
        except (der.UnexpectedDER, MalformedSignature) as e:
            raise BadSignatureError("Malformed formatting of signature", e)
        sig = ecdsa.Signature(r, s)
        if self.pubkey.verifies(number, sig):
            return True
        raise BadSignatureError("Signature verification failed")


class SigningKey(object):
    """
    Class for handling keys that can create signatures (private keys).

    :ivar ecdsa.curves.Curve curve: The Curve over which all the cryptographic
        operations will take place
    :ivar default_hashfunc: the function that will be used for hashing the
        data. Should implement the same API as hashlib.sha1
    :ivar int baselen: the length of a :term:`raw encoding` of private key
    :ivar ecdsa.keys.VerifyingKey verifying_key: the public key
        associated with this private key
    :ivar ecdsa.ecdsa.Private_key privkey: the actual private key
    """

    def __init__(self, _error__please_use_generate=None):
        """Unsupported, please use one of the classmethods to initialise."""
        if not _error__please_use_generate:
            raise TypeError("Please use SigningKey.generate() to construct me")
        self.curve = None
        self.default_hashfunc = None
        self.baselen = None
        self.verifying_key = None
        self.privkey = None

    def __eq__(self, other):
        """Return True if the points are identical, False otherwise."""
        if isinstance(other, SigningKey):
            return self.curve == other.curve \
                and self.verifying_key == other.verifying_key \
                and self.privkey == other.privkey
        return NotImplemented

    @classmethod
    def generate(cls, curve=NIST192p, entropy=None, hashfunc=sha1):
        """
        Generate a random private key.

        :param curve: The curve on which the point needs to reside, defaults
            to NIST192p
        :type curve: ecdsa.curves.Curve
        :param entropy: Source of randomness for generating the private keys,
            should provide cryptographically secure random numbers if the keys
            need to be secure. Uses os.urandom() by default.
        :type entropy: callable
        :param hashfunc: The default hash function that will be used for
            signing, needs to implement the same interface
            as hashlib.sha1
        :type hashfunc: callable

        :return: Initialised SigningKey object
        :rtype: SigningKey
        """
        secexp = randrange(curve.order, entropy)
        return cls.from_secret_exponent(secexp, curve, hashfunc)

    @classmethod
    def from_secret_exponent(cls, secexp, curve=NIST192p, hashfunc=sha1):
        """
        Create a private key from a random integer.

        Note: it's a low level method, it's recommended to use the
        :func:`~SigningKey.generate` method to create private keys.

        :param int secexp: secret multiplier (the actual private key in ECDSA).
            Needs to be an integer between 1 and the curve order.
        :param curve: The curve on which the point needs to reside
        :type curve: ecdsa.curves.Curve
        :param hashfunc: The default hash function that will be used for
            signing, needs to implement the same interface
            as hashlib.sha1
        :type hashfunc: callable

        :raises MalformedPointError: when the provided secexp is too large
            or too small for the curve selected
        :raises RuntimeError: if the generation of public key from private
            key failed

        :return: Initialised SigningKey object
        :rtype: SigningKey
        """
        self = cls(_error__please_use_generate=True)
        self.curve = curve
        self.default_hashfunc = hashfunc
        self.baselen = curve.baselen
        n = curve.order
        if not 1 <= secexp < n:
            raise MalformedPointError(
                "Invalid value for secexp, expected integer between 1 and {0}"
                .format(n))
        pubkey_point = curve.generator * secexp
        if hasattr(pubkey_point, "scale"):
            pubkey_point = pubkey_point.scale()
        self.verifying_key = VerifyingKey.from_public_point(pubkey_point, curve,
                                                            hashfunc, False)
        pubkey = self.verifying_key.pubkey
        self.privkey = ecdsa.Private_key(pubkey, secexp)
        self.privkey.order = n
        return self

    @classmethod
    def from_string(cls, string, curve=NIST192p, hashfunc=sha1):
        """
        Decode the private key from :term:`raw encoding`.

        Note: the name of this method is a misnomer coming from days of
        Python 2, when binary strings and character strings shared a type.
        In Python 3, the expected type is `bytes`.

        :param string: the raw encoding of the private key
        :type string: bytes like object
        :param curve: The curve on which the point needs to reside
        :type curve: ecdsa.curves.Curve
        :param hashfunc: The default hash function that will be used for
            signing, needs to implement the same interface
            as hashlib.sha1
        :type hashfunc: callable

        :raises MalformedPointError: if the length of encoding doesn't match
            the provided curve or the encoded values is too large
        :raises RuntimeError: if the generation of public key from private
            key failed

        :return: Initialised SigningKey object
        :rtype: SigningKey
        """
        string = normalise_bytes(string)
        if len(string) != curve.baselen:
            raise MalformedPointError(
                "Invalid length of private key, received {0}, expected {1}"
                .format(len(string), curve.baselen))
        secexp = string_to_number(string)
        return cls.from_secret_exponent(secexp, curve, hashfunc)

    @classmethod
    def from_pem(cls, string, hashfunc=sha1):
        """
        Initialise from key stored in :term:`PEM` format.

        Note, the only PEM format supported is the un-encrypted RFC5915
        (the sslay format) supported by OpenSSL, the more common PKCS#8 format
        is NOT supported (see:
        https://github.com/warner/python-ecdsa/issues/113 )

        ``openssl ec -in pkcs8.pem -out sslay.pem`` can be used to
        convert PKCS#8 file to this legacy format.

        The legacy format files have the header with the string
        ``BEGIN EC PRIVATE KEY``.
        Encrypted files (ones that include the string
        ``Proc-Type: 4,ENCRYPTED``
        right after the PEM header) are not supported.

        See :func:`~SigningKey.from_der` for ASN.1 syntax of the objects in
        this files.

        :param string: text with PEM-encoded private ECDSA key
        :type string: str

        :raises MalformedPointError: if the length of encoding doesn't match
            the provided curve or the encoded values is too large
        :raises RuntimeError: if the generation of public key from private
            key failed
        :raises UnexpectedDER: if the encoding of the PEM file is incorrect

        :return: Initialised VerifyingKey object
        :rtype: VerifyingKey
        """
        # the privkey pem may have multiple sections, commonly it also has
        # "EC PARAMETERS", we need just "EC PRIVATE KEY".
        if PY3 and isinstance(string, str):
            string = string.encode()
        privkey_pem = string[string.index(b("-----BEGIN EC PRIVATE KEY-----")):]
        return cls.from_der(der.unpem(privkey_pem), hashfunc)

    @classmethod
    def from_der(cls, string, hashfunc=sha1):
        """
        Initialise from key stored in :term:`DER` format.

        Note, the only DER format supported is the RFC5915
        (the sslay format) supported by OpenSSL, the more common PKCS#8 format
        is NOT supported (see:
        https://github.com/warner/python-ecdsa/issues/113 )

        ``openssl ec -in pkcs8.pem -outform der -out sslay.der`` can be
        used to convert PKCS#8 file to this legacy format.

        The encoding of the ASN.1 object in those files follows following
        syntax specified in RFC5915::

            ECPrivateKey ::= SEQUENCE {
              version        INTEGER { ecPrivkeyVer1(1) }} (ecPrivkeyVer1),
              privateKey     OCTET STRING,
              parameters [0] ECParameters {{ NamedCurve }} OPTIONAL,
              publicKey  [1] BIT STRING OPTIONAL
            }

        The only format supported for the `parameters` field is the named
        curve method. Explicit encoding of curve parameters is not supported.

        While `parameters` field is defined as optional, this implementation
        requires its presence for correct parsing of the keys.

        `publicKey` field is ignored completely (errors, if any, in it will
        be undetected).

        :param string: binary string with DER-encoded private ECDSA key
        :type string: bytes like object

        :raises MalformedPointError: if the length of encoding doesn't match
            the provided curve or the encoded values is too large
        :raises RuntimeError: if the generation of public key from private
            key failed
        :raises UnexpectedDER: if the encoding of the DER file is incorrect

        :return: Initialised VerifyingKey object
        :rtype: VerifyingKey
        """
        string = normalise_bytes(string)
        s, empty = der.remove_sequence(string)
        if empty != b(""):
            raise der.UnexpectedDER("trailing junk after DER privkey: %s" %
                                    binascii.hexlify(empty))
        one, s = der.remove_integer(s)
        if one != 1:
            raise der.UnexpectedDER("expected '1' at start of DER privkey,"
                                    " got %d" % one)
        privkey_str, s = der.remove_octet_string(s)
        tag, curve_oid_str, s = der.remove_constructed(s)
        if tag != 0:
            raise der.UnexpectedDER("expected tag 0 in DER privkey,"
                                    " got %d" % tag)
        curve_oid, empty = der.remove_object(curve_oid_str)
        if empty != b(""):
            raise der.UnexpectedDER("trailing junk after DER privkey "
                                    "curve_oid: %s" % binascii.hexlify(empty))
        curve = find_curve(curve_oid)

        # we don't actually care about the following fields
        #
        # tag, pubkey_bitstring, s = der.remove_constructed(s)
        # if tag != 1:
        #     raise der.UnexpectedDER("expected tag 1 in DER privkey, got %d"
        #                             % tag)
        # pubkey_str = der.remove_bitstring(pubkey_bitstring, 0)
        # if empty != "":
        #     raise der.UnexpectedDER("trailing junk after DER privkey "
        #                             "pubkeystr: %s" % binascii.hexlify(empty))

        # our from_string method likes fixed-length privkey strings
        if len(privkey_str) < curve.baselen:
            privkey_str = b("\x00") * (curve.baselen - len(privkey_str)) + privkey_str
        return cls.from_string(privkey_str, curve, hashfunc)

    def to_string(self):
        """
        Convert the private key to :term:`raw encoding`.

        Note: while the method is named "to_string", its name comes from
        Python 2 days, when binary and character strings used the same type.
        The type used in Python 3 is `bytes`.

        :return: raw encoding of private key
        :rtype: bytes
        """
        secexp = self.privkey.secret_multiplier
        s = number_to_string(secexp, self.privkey.order)
        return s

    def to_pem(self, point_encoding="uncompressed"):
        """
        Convert the private key to the :term:`PEM` format.

        See :func:`~SigningKey.from_pem` method for format description.

        Only the named curve format is supported.
        The public key will be included in generated string.

        The PEM header will specify ``BEGIN EC PRIVATE KEY``

        :param str point_encoding: format to use for encoding public point

        :return: PEM encoded private key
        :rtype: str
        """
        # TODO: "BEGIN ECPARAMETERS"
        return der.topem(self.to_der(point_encoding), "EC PRIVATE KEY")

    def to_der(self, point_encoding="uncompressed"):
        """
        Convert the private key to the :term:`DER` format.

        See :func:`~SigningKey.from_der` method for format specification.

        Only the named curve format is supported.
        The public key will be included in the generated string.

        :param str point_encoding: format to use for encoding public point

        :return: DER encoded private key
        :rtype: bytes
        """
        # SEQ([int(1), octetstring(privkey),cont[0], oid(secp224r1),
        #      cont[1],bitstring])
        if point_encoding == "raw":
            raise ValueError("raw encoding not allowed in DER")
        encoded_vk = self.get_verifying_key().to_string(point_encoding)
        # the 0 in encode_bitstring specifies the number of unused bits
        # in the `encoded_vk` string
        return der.encode_sequence(
            der.encode_integer(1),
            der.encode_octet_string(self.to_string()),
            der.encode_constructed(0, self.curve.encoded_oid),
            der.encode_constructed(1, der.encode_bitstring(encoded_vk, 0)))

    def get_verifying_key(self):
        """
        Return the VerifyingKey associated with this private key.

        Equivalent to reading the `verifying_key` field of an instance.

        :return: a public key that can be used to verify the signatures made
            with this SigningKey
        :rtype: VerifyingKey
        """
        return self.verifying_key

    def sign_deterministic(self, data, hashfunc=None,
                           sigencode=sigencode_string,
                           extra_entropy=b''):
        """
        Create signature over data using the deterministic RFC6679 algorithm.

        The data will be hashed using the `hashfunc` function before signing.

        This is the recommended method for performing signatures when hashing
        of data is necessary.

        :param data: data to be hashed and computed signature over
        :type data: bytes like object
        :param hashfunc: hash function to use for computing the signature,
            if unspecified, the default hash function selected during
            object initialisation will be used (see
            `VerifyingKey.default_hashfunc`). The object needs to implement
            the same interface as hashlib.sha1.
        :type hashfunc: callable
        :param sigencode: function used to encode the signature.
            The function needs to accept three parameters: the two integers
            that are the signature and the order of the curve over which the
            signature was computed. It needs to return an encoded signature.
            See `ecdsa.util.sigencode_string` and `ecdsa.util.sigencode_der`
            as examples of such functions.
        :type sigencode: callable
        :param extra_entropy: additional data that will be fed into the random
            number generator used in the RFC6979 process. Entirely optional.
        :type extra_entropy: bytes like object

        :return: encoded signature over `data`
        :rtype: bytes or sigencode function dependant type
        """
        hashfunc = hashfunc or self.default_hashfunc
        data = normalise_bytes(data)
        extra_entropy = normalise_bytes(extra_entropy)
        digest = hashfunc(data).digest()

        return self.sign_digest_deterministic(
            digest, hashfunc=hashfunc, sigencode=sigencode,
            extra_entropy=extra_entropy, allow_truncate=True)

    def sign_digest_deterministic(self, digest, hashfunc=None,
                                  sigencode=sigencode_string,
                                  extra_entropy=b'', allow_truncate=False):
        """
        Create signature for digest using the deterministic RFC6679 algorithm.

        `digest` should be the output of cryptographically secure hash function
        like SHA256 or SHA-3-256.

        This is the recommended method for performing signatures when no
        hashing of data is necessary.

        :param digest: hash of data that will be signed
        :type digest: bytes like object
        :param hashfunc: hash function to use for computing the random "k"
            value from RFC6979 process,
            if unspecified, the default hash function selected during
            object initialisation will be used (see
            `VerifyingKey.default_hashfunc`). The object needs to implement
            the same interface as hashlib.sha1.
        :type hashfunc: callable
        :param sigencode: function used to encode the signature.
            The function needs to accept three parameters: the two integers
            that are the signature and the order of the curve over which the
            signature was computed. It needs to return an encoded signature.
            See `ecdsa.util.sigencode_string` and `ecdsa.util.sigencode_der`
            as examples of such functions.
        :type sigencode: callable
        :param extra_entropy: additional data that will be fed into the random
            number generator used in the RFC6979 process. Entirely optional.
        :type extra_entropy: bytes like object
        :param bool allow_truncate: if True, the provided digest can have
            bigger bit-size than the order of the curve, the extra bits (at
            the end of the digest) will be truncated. Use it when signing
            SHA-384 output using NIST256p or in similar situations.

        :return: encoded signature for the `digest` hash
        :rtype: bytes or sigencode function dependant type
        """
        secexp = self.privkey.secret_multiplier
        hashfunc = hashfunc or self.default_hashfunc
        digest = normalise_bytes(digest)
        extra_entropy = normalise_bytes(extra_entropy)

        def simple_r_s(r, s, order):
            return r, s, order

        retry_gen = 0
        while True:
            k = rfc6979.generate_k(
                self.curve.generator.order(), secexp, hashfunc, digest,
                retry_gen=retry_gen, extra_entropy=extra_entropy)
            try:
                r, s, order = self.sign_digest(digest,
                                               sigencode=simple_r_s,
                                               k=k,
                                               allow_truncate=allow_truncate)
                break
            except RSZeroError:
                retry_gen += 1

        return sigencode(r, s, order)

    def sign(self, data, entropy=None, hashfunc=None,
             sigencode=sigencode_string, k=None):
        """
        Create signature over data using the probabilistic ECDSA algorithm.

        This method uses the standard ECDSA algorithm that requires a
        cryptographically secure random number generator.

        It's recommended to use the :func:`~SigningKey.sign_deterministic`
        method instead of this one.

        :param data: data that will be hashed for signing
        :type data: bytes like object
        :param callable entropy: randomness source, os.urandom by default
        :param hashfunc: hash function to use for hashing the provided `data`.
            If unspecified the default hash function selected during
            object initialisation will be used (see
            `VerifyingKey.default_hashfunc`).
            Should behave like hashlib.sha1. The output length of the
            hash (in bytes) must not be longer than the length of the curve
            order (rounded up to the nearest byte), so using SHA256 with
            NIST256p is ok, but SHA256 with NIST192p is not. (In the 2**-96ish
            unlikely event of a hash output larger than the curve order, the
            hash will effectively be wrapped mod n).
            Use hashfunc=hashlib.sha1 to match openssl's -ecdsa-with-SHA1 mode,
            or hashfunc=hashlib.sha256 for openssl-1.0.0's -ecdsa-with-SHA256.
        :type hashfunc: callable
        :param sigencode: function used to encode the signature.
            The function needs to accept three parameters: the two integers
            that are the signature and the order of the curve over which the
            signature was computed. It needs to return an encoded signature.
            See `ecdsa.util.sigencode_string` and `ecdsa.util.sigencode_der`
            as examples of such functions.
        :type sigencode: callable
        :param int k: a pre-selected nonce for calculating the signature.
            In typical use cases, it should be set to None (the default) to
            allow its generation from an entropy source.

        :raises RSZeroError: in the unlikely event when "r" parameter or
            "s" parameter is equal 0 as that would leak the key. Calee should
            try a better entropy source or different 'k' in such case.

        :return: encoded signature of the hash of `data`
        :rtype: bytes or sigencode function dependant type
        """
        hashfunc = hashfunc or self.default_hashfunc
        data = normalise_bytes(data)
        h = hashfunc(data).digest()
        return self.sign_digest(h, entropy, sigencode, k, allow_truncate=True)

    def sign_digest(self, digest, entropy=None, sigencode=sigencode_string,
                    k=None, allow_truncate=False):
        """
        Create signature over digest using the probabilistic ECDSA algorithm.

        This method uses the standard ECDSA algorithm that requires a
        cryptographically secure random number generator.

        This method does not hash the input.

        It's recommended to use the
        :func:`~SigningKey.sign_digest_deterministic` method
        instead of this one.

        :param digest: hash value that will be signed
        :type digest: bytes like object
        :param callable entropy: randomness source, os.urandom by default
        :param sigencode: function used to encode the signature.
            The function needs to accept three parameters: the two integers
            that are the signature and the order of the curve over which the
            signature was computed. It needs to return an encoded signature.
            See `ecdsa.util.sigencode_string` and `ecdsa.util.sigencode_der`
            as examples of such functions.
        :type sigencode: callable
        :param int k: a pre-selected nonce for calculating the signature.
            In typical use cases, it should be set to None (the default) to
            allow its generation from an entropy source.
        :param bool allow_truncate: if True, the provided digest can have
            bigger bit-size than the order of the curve, the extra bits (at
            the end of the digest) will be truncated. Use it when signing
            SHA-384 output using NIST256p or in similar situations.

        :raises RSZeroError: in the unlikely event when "r" parameter or
            "s" parameter is equal 0 as that would leak the key. Calee should
            try a better entropy source in such case.

        :return: encoded signature for the `digest` hash
        :rtype: bytes or sigencode function dependant type
        """
        digest = normalise_bytes(digest)
        if allow_truncate:
            digest = digest[:self.curve.baselen]
        if len(digest) > self.curve.baselen:
            raise BadDigestError("this curve (%s) is too short "
                                 "for your digest (%d)" % (self.curve.name,
                                                           8 * len(digest)))
        number = string_to_number(digest)
        r, s = self.sign_number(number, entropy, k)
        return sigencode(r, s, self.privkey.order)

    def sign_number(self, number, entropy=None, k=None):
        """
        Sign an integer directly.

        Note, this is a low level method, usually you will want to use
        :func:`~SigningKey.sign_deterministic` or
        :func:`~SigningKey.sign_digest_deterministic`.

        :param int number: number to sign using the probabilistic ECDSA
            algorithm.
        :param callable entropy: entropy source, os.urandom by default
        :param int k: pre-selected nonce for signature operation. If unset
            it will be selected at random using the entropy source.

        :raises RSZeroError: in the unlikely event when "r" parameter or
            "s" parameter is equal 0 as that would leak the key. Calee should
            try a different 'k' in such case.

        :return: the "r" and "s" parameters of the signature
        :rtype: tuple of ints
        """
        order = self.privkey.order

        if k is not None:
            _k = k
        else:
            _k = randrange(order, entropy)

        assert 1 <= _k < order
        sig = self.privkey.sign(number, _k)
        return sig.r, sig.s

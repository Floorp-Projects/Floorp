try:
    import unittest2 as unittest
except ImportError:
    import unittest

try:
    buffer
except NameError:
    buffer = memoryview

import array
import six
import sys
import pytest
import hashlib

from .keys import VerifyingKey, SigningKey
from .der import unpem
from .util import sigencode_string, sigencode_der, sigencode_strings, \
    sigdecode_string, sigdecode_der, sigdecode_strings


class TestVerifyingKeyFromString(unittest.TestCase):
    """
    Verify that ecdsa.keys.VerifyingKey.from_string() can be used with
    bytes-like objects
    """

    @classmethod
    def setUpClass(cls):
        cls.key_bytes = (b'\x04L\xa2\x95\xdb\xc7Z\xd7\x1f\x93\nz\xcf\x97\xcf'
                       b'\xd7\xc2\xd9o\xfe8}X!\xae\xd4\xfah\xfa^\rpI\xba\xd1'
                       b'Y\xfb\x92xa\xebo+\x9cG\xfav\xca')
        cls.vk = VerifyingKey.from_string(cls.key_bytes)

    def test_bytes(self):
        self.assertIsNotNone(self.vk)
        self.assertIsInstance(self.vk, VerifyingKey)
        self.assertEqual(
            self.vk.pubkey.point.x(),
            105419898848891948935835657980914000059957975659675736097)
        self.assertEqual(
            self.vk.pubkey.point.y(),
            4286866841217412202667522375431381222214611213481632495306)

    def test_bytes_memoryview(self):
        vk = VerifyingKey.from_string(buffer(self.key_bytes))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytearray(self):
        vk = VerifyingKey.from_string(bytearray(self.key_bytes))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytesarray_memoryview(self):
        vk = VerifyingKey.from_string(buffer(bytearray(self.key_bytes)))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_array_array_of_bytes(self):
        arr = array.array('B', self.key_bytes)
        vk = VerifyingKey.from_string(arr)

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_array_array_of_bytes_memoryview(self):
        arr = array.array('B', self.key_bytes)
        vk = VerifyingKey.from_string(buffer(arr))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_array_array_of_ints(self):
        arr = array.array('I', self.key_bytes)
        vk = VerifyingKey.from_string(arr)

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_array_array_of_ints_memoryview(self):
        arr = array.array('I', self.key_bytes)
        vk = VerifyingKey.from_string(buffer(arr))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytes_uncompressed(self):
        vk = VerifyingKey.from_string(b'\x04' + self.key_bytes)

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytearray_uncompressed(self):
        vk = VerifyingKey.from_string(bytearray(b'\x04' + self.key_bytes))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytes_compressed(self):
        vk = VerifyingKey.from_string(b'\x02' + self.key_bytes[:24])

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytearray_compressed(self):
        vk = VerifyingKey.from_string(bytearray(b'\x02' + self.key_bytes[:24]))

        self.assertEqual(self.vk.to_string(), vk.to_string())


class TestVerifyingKeyFromDer(unittest.TestCase):
    """
    Verify that ecdsa.keys.VerifyingKey.from_der() can be used with
    bytes-like objects.
    """
    @classmethod
    def setUpClass(cls):
        prv_key_str = (
            "-----BEGIN EC PRIVATE KEY-----\n"
            "MF8CAQEEGF7IQgvW75JSqULpiQQ8op9WH6Uldw6xxaAKBggqhkjOPQMBAaE0AzIA\n"
            "BLiBd9CE7xf15FY5QIAoNg+fWbSk1yZOYtoGUdzkejWkxbRc9RWTQjqLVXucIJnz\n"
            "bA==\n"
            "-----END EC PRIVATE KEY-----\n")
        key_str = (
            "-----BEGIN PUBLIC KEY-----\n"
            "MEkwEwYHKoZIzj0CAQYIKoZIzj0DAQEDMgAEuIF30ITvF/XkVjlAgCg2D59ZtKTX\n"
            "Jk5i2gZR3OR6NaTFtFz1FZNCOotVe5wgmfNs\n"
            "-----END PUBLIC KEY-----\n")
        cls.key_pem = key_str

        cls.key_bytes = unpem(key_str)
        assert isinstance(cls.key_bytes, bytes)
        cls.vk = VerifyingKey.from_pem(key_str)
        cls.sk = SigningKey.from_pem(prv_key_str)

        key_str = (
            "-----BEGIN PUBLIC KEY-----\n"
            "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE4H3iRbG4TSrsSRb/gusPQB/4YcN8\n"
            "Poqzgjau4kfxBPyZimeRfuY/9g/wMmPuhGl4BUve51DsnKJFRr8psk0ieA==\n"
            "-----END PUBLIC KEY-----\n"
        )
        cls.vk2 = VerifyingKey.from_pem(key_str)

    def test_custom_hashfunc(self):
        vk = VerifyingKey.from_der(self.key_bytes, hashlib.sha256)

        self.assertIs(vk.default_hashfunc, hashlib.sha256)

    def test_from_pem_with_custom_hashfunc(self):
        vk = VerifyingKey.from_pem(self.key_pem, hashlib.sha256)

        self.assertIs(vk.default_hashfunc, hashlib.sha256)

    def test_bytes(self):
        vk = VerifyingKey.from_der(self.key_bytes)

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytes_memoryview(self):
        vk = VerifyingKey.from_der(buffer(self.key_bytes))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytearray(self):
        vk = VerifyingKey.from_der(bytearray(self.key_bytes))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_bytesarray_memoryview(self):
        vk = VerifyingKey.from_der(buffer(bytearray(self.key_bytes)))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_array_array_of_bytes(self):
        arr = array.array('B', self.key_bytes)
        vk = VerifyingKey.from_der(arr)

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_array_array_of_bytes_memoryview(self):
        arr = array.array('B', self.key_bytes)
        vk = VerifyingKey.from_der(buffer(arr))

        self.assertEqual(self.vk.to_string(), vk.to_string())

    def test_equality_on_verifying_keys(self):
        self.assertEqual(self.vk, self.sk.get_verifying_key())

    def test_inequality_on_verifying_keys(self):
        self.assertNotEqual(self.vk, self.vk2)

    def test_inequality_on_verifying_keys_not_implemented(self):
        self.assertNotEqual(self.vk, None)


class TestSigningKey(unittest.TestCase):
    """
    Verify that ecdsa.keys.SigningKey.from_der() can be used with
    bytes-like objects.
    """
    @classmethod
    def setUpClass(cls):
        prv_key_str = (
            "-----BEGIN EC PRIVATE KEY-----\n"
            "MF8CAQEEGF7IQgvW75JSqULpiQQ8op9WH6Uldw6xxaAKBggqhkjOPQMBAaE0AzIA\n"
            "BLiBd9CE7xf15FY5QIAoNg+fWbSk1yZOYtoGUdzkejWkxbRc9RWTQjqLVXucIJnz\n"
            "bA==\n"
            "-----END EC PRIVATE KEY-----\n")
        cls.sk1 = SigningKey.from_pem(prv_key_str) 
        
        prv_key_str = (
            "-----BEGIN EC PRIVATE KEY-----\n"
            "MHcCAQEEIKlL2EAm5NPPZuXwxRf4nXMk0A80y6UUbiQ17be/qFhRoAoGCCqGSM49\n"
            "AwEHoUQDQgAE4H3iRbG4TSrsSRb/gusPQB/4YcN8Poqzgjau4kfxBPyZimeRfuY/\n"
            "9g/wMmPuhGl4BUve51DsnKJFRr8psk0ieA==\n"
            "-----END EC PRIVATE KEY-----\n")
        cls.sk2 = SigningKey.from_pem(prv_key_str) 
        
    def test_equality_on_signing_keys(self):        
        sk = SigningKey.from_secret_exponent(self.sk1.privkey.secret_multiplier, self.sk1.curve)
        self.assertEqual(self.sk1, sk)
        
    def test_inequality_on_signing_keys(self):
        self.assertNotEqual(self.sk1, self.sk2)
        
    def test_inequality_on_signing_keys_not_implemented(self):
        self.assertNotEqual(self.sk1, None)

# test VerifyingKey.verify()
prv_key_str = (
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MF8CAQEEGF7IQgvW75JSqULpiQQ8op9WH6Uldw6xxaAKBggqhkjOPQMBAaE0AzIA\n"
    "BLiBd9CE7xf15FY5QIAoNg+fWbSk1yZOYtoGUdzkejWkxbRc9RWTQjqLVXucIJnz\n"
    "bA==\n"
    "-----END EC PRIVATE KEY-----\n")
key_bytes = unpem(prv_key_str)
assert isinstance(key_bytes, bytes)
sk = SigningKey.from_der(key_bytes)
vk = sk.verifying_key

data = (b"some string for signing"
        b"contents don't really matter"
        b"but do include also some crazy values: "
        b"\x00\x01\t\r\n\x00\x00\x00\xff\xf0")
assert len(data) % 4 == 0
sha1 = hashlib.sha1()
sha1.update(data)
data_hash = sha1.digest()
assert isinstance(data_hash, bytes)
sig_raw = sk.sign(data, sigencode=sigencode_string)
assert isinstance(sig_raw, bytes)
sig_der = sk.sign(data, sigencode=sigencode_der)
assert isinstance(sig_der, bytes)
sig_strings = sk.sign(data, sigencode=sigencode_strings)
assert isinstance(sig_strings[0], bytes)

verifiers = []
for modifier, fun in [
    ("bytes", lambda x: x),
    ("bytes memoryview", lambda x: buffer(x)),
    ("bytearray", lambda x: bytearray(x)),
    ("bytearray memoryview", lambda x: buffer(bytearray(x))),
    ("array.array of bytes", lambda x: array.array('B', x)),
    ("array.array of bytes memoryview", lambda x: buffer(array.array('B', x))),
    ("array.array of ints", lambda x: array.array('I', x)),
    ("array.array of ints memoryview", lambda x: buffer(array.array('I', x)))
    ]:
    if "ints" in modifier:
        conv = lambda x: x
    else:
        conv = fun
    for sig_format, signature, decoder, mod_apply in [
        ("raw", sig_raw, sigdecode_string, lambda x: conv(x)),
        ("der", sig_der, sigdecode_der, lambda x: conv(x)),
        ("strings", sig_strings, sigdecode_strings, lambda x:
            tuple(conv(i) for i in x))
        ]:
        for method_name, vrf_mthd, vrf_data in [
            ("verify", vk.verify, data),
            ("verify_digest", vk.verify_digest, data_hash)
            ]:
            verifiers.append(pytest.param(
                signature, decoder, mod_apply, fun, vrf_mthd, vrf_data,
                id="{2}-{0}-{1}".format(modifier, sig_format, method_name)))

@pytest.mark.parametrize(
    "signature,decoder,mod_apply,fun,vrf_mthd,vrf_data",
    verifiers)
def test_VerifyingKey_verify(
        signature, decoder, mod_apply, fun, vrf_mthd, vrf_data):
    sig = mod_apply(signature)

    assert vrf_mthd(sig, fun(vrf_data), sigdecode=decoder)


# test SigningKey.from_string()
prv_key_bytes = (b'^\xc8B\x0b\xd6\xef\x92R\xa9B\xe9\x89\x04<\xa2'
                 b'\x9fV\x1f\xa5%w\x0e\xb1\xc5')
assert len(prv_key_bytes) == 24
converters = []
for modifier, convert in [
        ("bytes", lambda x: x),
        ("bytes memoryview", buffer),
        ("bytearray", bytearray),
        ("bytearray memoryview", lambda x: buffer(bytearray(x))),
        ("array.array of bytes", lambda x: array.array('B', x)),
        ("array.array of bytes memoryview",
         lambda x: buffer(array.array('B', x))),
        ("array.array of ints", lambda x: array.array('I', x)),
        ("array.array of ints memoryview",
         lambda x: buffer(array.array('I', x)))
        ]:
    converters.append(pytest.param(
        convert,
        id=modifier))

@pytest.mark.parametrize("convert", converters)
def test_SigningKey_from_string(convert):
    key = convert(prv_key_bytes)
    sk = SigningKey.from_string(key)

    assert sk.to_string() == prv_key_bytes


# test SigningKey.from_der()
prv_key_str = (
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MF8CAQEEGF7IQgvW75JSqULpiQQ8op9WH6Uldw6xxaAKBggqhkjOPQMBAaE0AzIA\n"
    "BLiBd9CE7xf15FY5QIAoNg+fWbSk1yZOYtoGUdzkejWkxbRc9RWTQjqLVXucIJnz\n"
    "bA==\n"
    "-----END EC PRIVATE KEY-----\n")
key_bytes = unpem(prv_key_str)
assert isinstance(key_bytes, bytes)

# last two converters are for array.array of ints, those require input
# that's multiple of 4, which no curve we support produces
@pytest.mark.parametrize("convert", converters[:-2])
def test_SigningKey_from_der(convert):
    key = convert(key_bytes)
    sk = SigningKey.from_der(key)

    assert sk.to_string() == prv_key_bytes


# test SigningKey.sign_deterministic()
extra_entropy=b'\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11'

@pytest.mark.parametrize("convert", converters)
def test_SigningKey_sign_deterministic(convert):
    sig = sk.sign_deterministic(
        convert(data),
        extra_entropy=convert(extra_entropy))

    vk.verify(sig, data)


# test SigningKey.sign_digest_deterministic()
@pytest.mark.parametrize("convert", converters)
def test_SigningKey_sign_digest_deterministic(convert):
    sig = sk.sign_digest_deterministic(
        convert(data_hash),
        extra_entropy=convert(extra_entropy))

    vk.verify(sig, data)


@pytest.mark.parametrize("convert", converters)
def test_SigningKey_sign(convert):
    sig = sk.sign(convert(data))

    vk.verify(sig, data)


@pytest.mark.parametrize("convert", converters)
def test_SigningKey_sign_digest(convert):
    sig = sk.sign_digest(convert(data_hash))

    vk.verify(sig, data)

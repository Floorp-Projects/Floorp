from __future__ import division

import math
import re
from binascii import unhexlify
from datetime import datetime, timedelta
from decimal import Decimal
from email.message import Message
from fractions import Fraction
from io import BytesIO
from uuid import UUID

import pytest

from cbor2.compat import timezone
from cbor2.decoder import loads, CBORDecodeError, load, CBORDecoder
from cbor2.types import CBORTag, undefined, CBORSimpleValue


@pytest.mark.parametrize('payload, expected', [
    ('00', 0),
    ('01', 1),
    ('0a', 10),
    ('17', 23),
    ('1818', 24),
    ('1819', 25),
    ('1864', 100),
    ('1903e8', 1000),
    ('1a000f4240', 1000000),
    ('1b000000e8d4a51000', 1000000000000),
    ('1bffffffffffffffff', 18446744073709551615),
    ('c249010000000000000000', 18446744073709551616),
    ('3bffffffffffffffff', -18446744073709551616),
    ('c349010000000000000000', -18446744073709551617),
    ('20', -1),
    ('29', -10),
    ('3863', -100),
    ('3903e7', -1000)
])
def test_integer(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


def test_invalid_integer_subtype():
    exc = pytest.raises(CBORDecodeError, loads, b'\x1c')
    assert str(exc.value).endswith('unknown unsigned integer subtype 0x1c')


@pytest.mark.parametrize('payload, expected', [
    ('f90000', 0.0),
    ('f98000', -0.0),
    ('f93c00', 1.0),
    ('fb3ff199999999999a', 1.1),
    ('f93e00', 1.5),
    ('f97bff', 65504.0),
    ('fa47c35000', 100000.0),
    ('fa7f7fffff', 3.4028234663852886e+38),
    ('fb7e37e43c8800759c', 1.0e+300),
    ('f90001', 5.960464477539063e-8),
    ('f90400', 0.00006103515625),
    ('f9c400', -4.0),
    ('fbc010666666666666', -4.1),
    ('f97c00', float('inf')),
    ('f9fc00', float('-inf')),
    ('fa7f800000', float('inf')),
    ('faff800000', float('-inf')),
    ('fb7ff0000000000000', float('inf')),
    ('fbfff0000000000000', float('-inf'))
])
def test_float(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


@pytest.mark.parametrize('payload', ['f97e00', 'fa7fc00000', 'fb7ff8000000000000'])
def test_float_nan(payload):
    decoded = loads(unhexlify(payload))
    assert math.isnan(decoded)


@pytest.mark.parametrize('payload, expected', [
    ('f4', False),
    ('f5', True),
    ('f6', None),
    ('f7', undefined)
])
def test_special(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded is expected


@pytest.mark.parametrize('payload, expected', [
    ('40', b''),
    ('4401020304', b'\x01\x02\x03\x04'),
])
def test_binary(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


@pytest.mark.parametrize('payload, expected', [
    ('60', u''),
    ('6161', u'a'),
    ('6449455446', u'IETF'),
    ('62225c', u'\"\\'),
    ('62c3bc', u'\u00fc'),
    ('63e6b0b4', u'\u6c34')
])
def test_string(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


@pytest.mark.parametrize('payload, expected', [
    ('80', []),
    ('83010203', [1, 2, 3]),
    ('8301820203820405', [1, [2, 3], [4, 5]]),
    ('98190102030405060708090a0b0c0d0e0f101112131415161718181819', list(range(1, 26)))
])
def test_array(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


@pytest.mark.parametrize('payload, expected', [
    ('a0', {}),
    ('a201020304', {1: 2, 3: 4})
])
def test_map(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


@pytest.mark.parametrize('payload, expected', [
    ('a26161016162820203', {'a': 1, 'b': [2, 3]}),
    ('826161a161626163', ['a', {'b': 'c'}]),
    ('a56161614161626142616361436164614461656145',
     {'a': 'A', 'b': 'B', 'c': 'C', 'd': 'D', 'e': 'E'})
])
def test_mixed_array_map(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


@pytest.mark.parametrize('payload, expected', [
    ('5f42010243030405ff', b'\x01\x02\x03\x04\x05'),
    ('7f657374726561646d696e67ff', 'streaming'),
    ('9fff', []),
    ('9f018202039f0405ffff', [1, [2, 3], [4, 5]]),
    ('9f01820203820405ff', [1, [2, 3], [4, 5]]),
    ('83018202039f0405ff', [1, [2, 3], [4, 5]]),
    ('83019f0203ff820405', [1, [2, 3], [4, 5]]),
    ('9f0102030405060708090a0b0c0d0e0f101112131415161718181819ff', list(range(1, 26))),
    ('bf61610161629f0203ffff', {'a': 1, 'b': [2, 3]}),
    ('826161bf61626163ff', ['a', {'b': 'c'}]),
    ('bf6346756ef563416d7421ff', {'Fun': True, 'Amt': -2}),
])
def test_streaming(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


@pytest.mark.parametrize('payload, expected', [
    ('e0', 0),
    ('e2', 2),
    ('f3', 19),
    ('f820', 32),
    ('e0', CBORSimpleValue(0)),
    ('e2', CBORSimpleValue(2)),
    ('f3', CBORSimpleValue(19)),
    ('f820', CBORSimpleValue(32))
])
def test_simple_value(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


#
# Tests for extension tags
#


@pytest.mark.parametrize('payload, expected', [
    ('c074323031332d30332d32315432303a30343a30305a',
     datetime(2013, 3, 21, 20, 4, 0, tzinfo=timezone.utc)),
    ('c0781b323031332d30332d32315432303a30343a30302e3338303834315a',
     datetime(2013, 3, 21, 20, 4, 0, 380841, tzinfo=timezone.utc)),
    ('c07819323031332d30332d32315432323a30343a30302b30323a3030',
     datetime(2013, 3, 21, 22, 4, 0, tzinfo=timezone(timedelta(hours=2)))),
    ('c11a514b67b0', datetime(2013, 3, 21, 20, 4, 0, tzinfo=timezone.utc)),
    ('c11a514b67b0', datetime(2013, 3, 21, 22, 4, 0, tzinfo=timezone(timedelta(hours=2))))
], ids=['datetime/utc', 'datetime+micro/utc', 'datetime/eet', 'timestamp/utc', 'timestamp/eet'])
def test_datetime(payload, expected):
    decoded = loads(unhexlify(payload))
    assert decoded == expected


def test_bad_datetime():
    exc = pytest.raises(CBORDecodeError, loads, unhexlify('c06b303030302d3132332d3031'))
    assert str(exc.value).endswith('invalid datetime string: 0000-123-01')


def test_fraction():
    decoded = loads(unhexlify('c48221196ab3'))
    assert decoded == Decimal('273.15')


def test_bigfloat():
    decoded = loads(unhexlify('c5822003'))
    assert decoded == Decimal('1.5')


def test_rational():
    decoded = loads(unhexlify('d81e820205'))
    assert decoded == Fraction(2, 5)


def test_regex():
    decoded = loads(unhexlify('d8236d68656c6c6f2028776f726c6429'))
    expr = re.compile(u'hello (world)')
    assert decoded == expr


def test_mime():
    decoded = loads(unhexlify(
        'd824787b436f6e74656e742d547970653a20746578742f706c61696e3b20636861727365743d2269736f2d38'
        '3835392d3135220a4d494d452d56657273696f6e3a20312e300a436f6e74656e742d5472616e736665722d45'
        '6e636f64696e673a2071756f7465642d7072696e7461626c650a0a48656c6c6f203d413475726f'))
    assert isinstance(decoded, Message)
    assert decoded.get_payload() == 'Hello =A4uro'


def test_uuid():
    decoded = loads(unhexlify('d825505eaffac8b51e480581277fdcc7842faf'))
    assert decoded == UUID(hex='5eaffac8b51e480581277fdcc7842faf')


def test_bad_shared_reference():
    exc = pytest.raises(CBORDecodeError, loads, unhexlify('d81d05'))
    assert str(exc.value).endswith('shared reference 5 not found')


def test_uninitialized_shared_reference():
    fp = BytesIO(unhexlify('d81d00'))
    decoder = CBORDecoder(fp)
    decoder._shareables.append(None)
    exc = pytest.raises(CBORDecodeError, decoder.decode)
    assert str(exc.value).endswith('shared value 0 has not been initialized')


def test_cyclic_array():
    decoded = loads(unhexlify('d81c81d81d00'))
    assert decoded == [decoded]


def test_cyclic_map():
    decoded = loads(unhexlify('d81ca100d81d00'))
    assert decoded == {0: decoded}


def test_unhandled_tag():
    """
    Test that a tag is simply ignored and its associated value returned if there is no special
    handling available for it.

    """
    decoded = loads(unhexlify('d917706548656c6c6f'))
    assert decoded == CBORTag(6000, u'Hello')


def test_premature_end_of_stream():
    """
    Test that the decoder detects a situation where read() returned fewer than expected bytes.

    """
    exc = pytest.raises(CBORDecodeError, loads, unhexlify('437879'))
    exc.match('premature end of stream \(expected to read 3 bytes, got 2 instead\)')


def test_tag_hook():
    def reverse(decoder, tag, fp, shareable_index=None):
        return tag.value[::-1]

    decoded = loads(unhexlify('d917706548656c6c6f'), tag_hook=reverse)
    assert decoded == u'olleH'


def test_tag_hook_cyclic():
    class DummyType(object):
        def __init__(self, value):
            self.value = value

    def unmarshal_dummy(decoder, tag, shareable_index=None):
        instance = DummyType.__new__(DummyType)
        decoder.set_shareable(shareable_index, instance)
        instance.value = decoder.decode_from_bytes(tag.value)
        return instance

    decoded = loads(unhexlify('D81CD90BB849D81CD90BB843D81D00'), tag_hook=unmarshal_dummy)
    assert isinstance(decoded, DummyType)
    assert decoded.value.value is decoded


def test_object_hook():
    class DummyType(object):
        def __init__(self, state):
            self.state = state

    payload = unhexlify('A2616103616205')
    decoded = loads(payload, object_hook=lambda decoder, value: DummyType(value))
    assert isinstance(decoded, DummyType)
    assert decoded.state == {'a': 3, 'b': 5}


def test_error_major_type():
    exc = pytest.raises(CBORDecodeError, loads, b'')
    assert str(exc.value).startswith('error reading major type at index 0: ')


def test_load_from_file(tmpdir):
    path = tmpdir.join('testdata.cbor')
    path.write_binary(b'\x82\x01\x0a')
    with path.open('rb') as fp:
        obj = load(fp)

    assert obj == [1, 10]

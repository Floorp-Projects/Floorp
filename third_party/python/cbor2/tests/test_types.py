import pytest

from cbor2.types import CBORTag, CBORSimpleValue


def test_tag_repr():
    assert repr(CBORTag(600, 'blah')) == "CBORTag(600, 'blah')"


def test_tag_equals():
    tag1 = CBORTag(500, ['foo'])
    tag2 = CBORTag(500, ['foo'])
    tag3 = CBORTag(500, ['bar'])
    assert tag1 == tag2
    assert not tag1 == tag3
    assert not tag1 == 500


def test_simple_value_repr():
    assert repr(CBORSimpleValue(1)) == "CBORSimpleValue(1)"


def test_simple_value_equals():
    tag1 = CBORSimpleValue(1)
    tag2 = CBORSimpleValue(1)
    tag3 = CBORSimpleValue(21)
    assert tag1 == tag2
    assert tag1 == 1
    assert not tag1 == tag3
    assert not tag1 == 21
    assert not tag2 == "21"


def test_simple_value_too_big():
    exc = pytest.raises(TypeError, CBORSimpleValue, 256)
    assert str(exc.value) == 'simple value too big'

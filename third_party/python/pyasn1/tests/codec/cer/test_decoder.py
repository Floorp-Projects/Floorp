#
# This file is part of pyasn1 software.
#
# Copyright (c) 2005-2019, Ilya Etingof <etingof@gmail.com>
# License: http://snmplabs.com/pyasn1/license.html
#
import sys

try:
    import unittest2 as unittest
except ImportError:
    import unittest

from tests.base import BaseTestCase

from pyasn1.type import tag
from pyasn1.type import namedtype
from pyasn1.type import opentype
from pyasn1.type import univ
from pyasn1.codec.cer import decoder
from pyasn1.compat.octets import ints2octs, str2octs, null
from pyasn1.error import PyAsn1Error


class BooleanDecoderTestCase(BaseTestCase):
    def testTrue(self):
        assert decoder.decode(ints2octs((1, 1, 255))) == (1, null)

    def testFalse(self):
        assert decoder.decode(ints2octs((1, 1, 0))) == (0, null)

    def testEmpty(self):
        try:
            decoder.decode(ints2octs((1, 0)))
        except PyAsn1Error:
            pass

    def testOverflow(self):
        try:
            decoder.decode(ints2octs((1, 2, 0, 0)))
        except PyAsn1Error:
            pass

class BitStringDecoderTestCase(BaseTestCase):
    def testShortMode(self):
        assert decoder.decode(
            ints2octs((3, 3, 6, 170, 128))
        ) == (((1, 0) * 5), null)

    def testLongMode(self):
        assert decoder.decode(
            ints2octs((3, 127, 6) + (170,) * 125 + (128,))
        ) == (((1, 0) * 501), null)

    # TODO: test failures on short chunked and long unchunked substrate samples


class OctetStringDecoderTestCase(BaseTestCase):
    def testShortMode(self):
        assert decoder.decode(
            ints2octs((4, 15, 81, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110, 32, 102, 111, 120)),
        ) == (str2octs('Quick brown fox'), null)

    def testLongMode(self):
        assert decoder.decode(
            ints2octs((36, 128, 4, 130, 3, 232) + (81,) * 1000 + (4, 1, 81, 0, 0))
        ) == (str2octs('Q' * 1001), null)

    # TODO: test failures on short chunked and long unchunked substrate samples


class SequenceDecoderWithUntaggedOpenTypesTestCase(BaseTestCase):
    def setUp(self):
        openType = opentype.OpenType(
            'id',
            {1: univ.Integer(),
             2: univ.OctetString()}
        )
        self.s = univ.Sequence(
            componentType=namedtype.NamedTypes(
                namedtype.NamedType('id', univ.Integer()),
                namedtype.NamedType('blob', univ.Any(), openType=openType)
            )
        )

    def testDecodeOpenTypesChoiceOne(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 1, 2, 1, 12, 0, 0)),
            asn1Spec=self.s,
            decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 1
        assert s[1] == 12

    def testDecodeOpenTypesChoiceTwo(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 2, 4, 11, 113, 117, 105, 99, 107, 32, 98,
                114, 111, 119, 110, 0, 0)), asn1Spec=self.s,
            decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 2
        assert s[1] == univ.OctetString('quick brown')

    def testDecodeOpenTypesUnknownType(self):
        try:
            s, r = decoder.decode(
                ints2octs((48, 128, 6, 1, 1, 2, 1, 12, 0, 0)), asn1Spec=self.s,
                decodeOpenTypes=True
            )

        except PyAsn1Error:
            pass

        else:
            assert False, 'unknown open type tolerated'

    def testDecodeOpenTypesUnknownId(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 3, 6, 1, 12, 0, 0)), asn1Spec=self.s,
            decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 3
        assert s[1] == univ.OctetString(hexValue='06010c')

    def testDontDecodeOpenTypesChoiceOne(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 1, 2, 1, 12, 0, 0)), asn1Spec=self.s
        )
        assert not r
        assert s[0] == 1
        assert s[1] == ints2octs((2, 1, 12))

    def testDontDecodeOpenTypesChoiceTwo(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 2, 4, 11, 113, 117, 105, 99, 107, 32, 98,
                114, 111, 119, 110, 0, 0)), asn1Spec=self.s
        )
        assert not r
        assert s[0] == 2
        assert s[1] == ints2octs((4, 11, 113, 117, 105, 99, 107, 32, 98, 114, 111, 119, 110))


class SequenceDecoderWithImplicitlyTaggedOpenTypesTestCase(BaseTestCase):
    def setUp(self):
        openType = opentype.OpenType(
            'id',
            {1: univ.Integer(),
             2: univ.OctetString()}
        )
        self.s = univ.Sequence(
            componentType=namedtype.NamedTypes(
                namedtype.NamedType('id', univ.Integer()),
                namedtype.NamedType(
                    'blob', univ.Any().subtype(implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 3)), openType=openType
                )
            )
        )

    def testDecodeOpenTypesChoiceOne(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 1, 163, 128, 2, 1, 12, 0, 0, 0, 0)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 1
        assert s[1] == 12

    def testDecodeOpenTypesUnknownId(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 3, 163, 128, 2, 1, 12, 0, 0, 0, 0)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 3
        assert s[1] == univ.OctetString(hexValue='02010C')


class SequenceDecoderWithExplicitlyTaggedOpenTypesTestCase(BaseTestCase):
    def setUp(self):
        openType = opentype.OpenType(
            'id',
            {1: univ.Integer(),
             2: univ.OctetString()}
        )
        self.s = univ.Sequence(
            componentType=namedtype.NamedTypes(
                namedtype.NamedType('id', univ.Integer()),
                namedtype.NamedType(
                    'blob', univ.Any().subtype(explicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 3)), openType=openType
                )
            )
        )

    def testDecodeOpenTypesChoiceOne(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 1, 163, 128, 2, 1, 12, 0, 0, 0, 0)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 1
        assert s[1] == 12

    def testDecodeOpenTypesUnknownId(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 3, 163, 128, 2, 1, 12, 0, 0, 0, 0)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 3
        assert s[1] == univ.OctetString(hexValue='02010C')


class SequenceDecoderWithUntaggedSetOfOpenTypesTestCase(BaseTestCase):
    def setUp(self):
        openType = opentype.OpenType(
            'id',
            {1: univ.Integer(),
             2: univ.OctetString()}
        )
        self.s = univ.Sequence(
            componentType=namedtype.NamedTypes(
                namedtype.NamedType('id', univ.Integer()),
                namedtype.NamedType('blob', univ.SetOf(componentType=univ.Any()),
                                    openType=openType)
            )
        )

    def testDecodeOpenTypesChoiceOne(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 1, 49, 128, 2, 1, 12, 0, 0, 0, 0)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 1
        assert s[1][0] == 12

    def testDecodeOpenTypesChoiceTwo(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 2, 49, 128, 4, 11, 113, 117, 105, 99,
                       107, 32, 98, 114, 111, 119, 110, 0, 0, 0, 0)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 2
        assert s[1][0] == univ.OctetString('quick brown')

    def testDecodeOpenTypesUnknownType(self):
        try:
            s, r = decoder.decode(
                ints2octs((48, 128, 6, 1, 1, 49, 128, 2, 1, 12, 0, 0, 0, 0)),
                asn1Spec=self.s, decodeOpenTypes=True
            )

        except PyAsn1Error:
            pass

        else:
            assert False, 'unknown open type tolerated'

    def testDecodeOpenTypesUnknownId(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 3, 49, 128, 2, 1, 12, 0, 0, 0, 0)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 3
        assert s[1][0] == univ.OctetString(hexValue='02010c')

    def testDontDecodeOpenTypesChoiceOne(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 1, 49, 128, 2, 1, 12, 0, 0, 0, 0)),
            asn1Spec=self.s
        )
        assert not r
        assert s[0] == 1
        assert s[1][0] == ints2octs((2, 1, 12))

    def testDontDecodeOpenTypesChoiceTwo(self):
        s, r = decoder.decode(
            ints2octs((48, 128, 2, 1, 2, 49, 128, 4, 11, 113, 117, 105, 99, 107, 32,
                98, 114, 111, 119, 110, 0, 0, 0, 0)), asn1Spec=self.s
        )
        assert not r
        assert s[0] == 2
        assert s[1][0] == ints2octs((4, 11, 113, 117, 105, 99, 107, 32, 98, 114,
                                     111, 119, 110))


class SequenceDecoderWithImplicitlyTaggedSetOfOpenTypesTestCase(BaseTestCase):
    def setUp(self):
        openType = opentype.OpenType(
            'id',
            {1: univ.Integer(),
             2: univ.OctetString()}
        )
        self.s = univ.Sequence(
            componentType=namedtype.NamedTypes(
                namedtype.NamedType('id', univ.Integer()),
                namedtype.NamedType(
                    'blob', univ.SetOf(
                        componentType=univ.Any().subtype(
                            implicitTag=tag.Tag(
                                tag.tagClassContext, tag.tagFormatSimple, 3))),
                    openType=openType
                )
            )
        )

    def testDecodeOpenTypesChoiceOne(self):
        s, r = decoder.decode(
            ints2octs((48, 10, 2, 1, 1, 49, 5, 131, 3, 2, 1, 12)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 1
        assert s[1][0] == 12

    def testDecodeOpenTypesUnknownId(self):
        s, r = decoder.decode(
            ints2octs((48, 10, 2, 1, 3, 49, 5, 131, 3, 2, 1, 12)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 3
        assert s[1][0] == univ.OctetString(hexValue='02010C')


class SequenceDecoderWithExplicitlyTaggedSetOfOpenTypesTestCase(BaseTestCase):
    def setUp(self):
        openType = opentype.OpenType(
            'id',
            {1: univ.Integer(),
             2: univ.OctetString()}
        )
        self.s = univ.Sequence(
            componentType=namedtype.NamedTypes(
                namedtype.NamedType('id', univ.Integer()),
                namedtype.NamedType(
                    'blob', univ.SetOf(
                        componentType=univ.Any().subtype(
                            explicitTag=tag.Tag(
                                tag.tagClassContext, tag.tagFormatSimple, 3))),
                    openType=openType
                )
            )
        )

    def testDecodeOpenTypesChoiceOne(self):
        s, r = decoder.decode(
            ints2octs((48, 10, 2, 1, 1, 49, 5, 131, 3, 2, 1, 12)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 1
        assert s[1][0] == 12

    def testDecodeOpenTypesUnknownId(self):
        s, r = decoder.decode(
            ints2octs( (48, 10, 2, 1, 3, 49, 5, 131, 3, 2, 1, 12)),
            asn1Spec=self.s, decodeOpenTypes=True
        )
        assert not r
        assert s[0] == 3
        assert s[1][0] == univ.OctetString(hexValue='02010C')


suite = unittest.TestLoader().loadTestsFromModule(sys.modules[__name__])

if __name__ == '__main__':
    unittest.TextTestRunner(verbosity=2).run(suite)

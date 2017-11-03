#
# This file is part of pyasn1 software.
#
# Copyright (c) 2005-2017, Ilya Etingof <etingof@gmail.com>
# License: http://pyasn1.sf.net/license.html
#
import sys
from pyasn1.type import univ, tag
from pyasn1 import error


__all__ = ['NumericString', 'PrintableString', 'TeletexString', 'T61String', 'VideotexString',
           'IA5String', 'GraphicString', 'VisibleString', 'ISO646String',
           'GeneralString', 'UniversalString', 'BMPString', 'UTF8String']

NoValue = univ.NoValue
noValue = univ.noValue


class AbstractCharacterString(univ.OctetString):
    """Creates |ASN.1| type or object.

    |ASN.1| objects are immutable and duck-type Python 2 :class:`unicode` or Python 3 :class:`str`.
    When used in octet-stream context, |ASN.1| type assumes "|encoding|" encoding.

    Parameters
    ----------
    value: :class:`unicode`, :class:`str`, :class:`bytes` or |ASN.1| object
        unicode object (Python 2) or string (Python 3), alternatively string
        (Python 2) or bytes (Python 3) representing octet-stream of serialized
        unicode string (note `encoding` parameter) or |ASN.1| class instance.

    tagSet: :py:class:`~pyasn1.type.tag.TagSet`
        Object representing non-default ASN.1 tag(s)

    subtypeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
        Object representing non-default ASN.1 subtype constraint(s)

    encoding: :py:class:`str`
        Unicode codec ID to encode/decode :class:`unicode` (Python 2) or
        :class:`str` (Python 3) the payload when |ASN.1| object is used
        in octet-stream context.

    Raises
    ------
    : :py:class:`pyasn1.error.PyAsn1Error`
        On constraint violation or bad initializer.
    """

    if sys.version_info[0] <= 2:
        def __str__(self):
            try:
                return self._value.encode(self.encoding)
            except UnicodeEncodeError:
                raise error.PyAsn1Error(
                    "Can't encode string '%s' with codec %s" % (self._value, self.encoding)
                )

        def __unicode__(self):
            return unicode(self._value)

        def prettyIn(self, value):
            try:
                if isinstance(value, unicode):
                    return value
                elif isinstance(value, str):
                    return value.decode(self.encoding)
                elif isinstance(value, (tuple, list)):
                    return self.prettyIn(''.join([chr(x) for x in value]))
                elif isinstance(value, univ.OctetString):
                    return value.asOctets().decode(self.encoding)
                else:
                    return unicode(value)

            except (UnicodeDecodeError, LookupError):
                raise error.PyAsn1Error(
                    "Can't decode string '%s' with codec %s" % (value, self.encoding)
                )

        def asOctets(self, padding=True):
            return str(self)

        def asNumbers(self, padding=True):
            return tuple([ord(x) for x in str(self)])

    else:
        def __str__(self):
            return str(self._value)

        def __bytes__(self):
            try:
                return self._value.encode(self.encoding)
            except UnicodeEncodeError:
                raise error.PyAsn1Error(
                    "Can't encode string '%s' with codec %s" % (self._value, self.encoding)
                )

        def prettyIn(self, value):
            try:
                if isinstance(value, str):
                    return value
                elif isinstance(value, bytes):
                    return value.decode(self.encoding)
                elif isinstance(value, (tuple, list)):
                    return self.prettyIn(bytes(value))
                elif isinstance(value, univ.OctetString):
                    return value.asOctets().decode(self.encoding)
                else:
                    return str(value)

            except (UnicodeDecodeError, LookupError):
                raise error.PyAsn1Error(
                    "Can't decode string '%s' with codec %s" % (value, self.encoding)
                )

        def asOctets(self, padding=True):
            return bytes(self)

        def asNumbers(self, padding=True):
            return tuple(bytes(self))

    def prettyOut(self, value):
        return value

    def __reversed__(self):
        return reversed(self._value)

    def clone(self, value=noValue, **kwargs):
        """Creates a copy of a |ASN.1| type or object.

        Any parameters to the *clone()* method will replace corresponding
        properties of the |ASN.1| object.

        Parameters
        ----------
        value: :class:`unicode`, :class:`str`, :class:`bytes` or |ASN.1| object
            unicode object (Python 2) or string (Python 3), alternatively string
            (Python 2) or bytes (Python 3) representing octet-stream of serialized
            unicode string (note `encoding` parameter) or |ASN.1| class instance.

        tagSet: :py:class:`~pyasn1.type.tag.TagSet`
            Object representing non-default ASN.1 tag(s)

        subtypeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
            Object representing non-default ASN.1 subtype constraint(s)

        encoding: :py:class:`str`
            Unicode codec ID to encode/decode :py:class:`unicode` (Python 2) or
            :py:class:`str` (Python 3) the payload when |ASN.1| object is used
            in octet-stream context.

        Returns
        -------
        :
            new instance of |ASN.1| type/value

        """
        return univ.OctetString.clone(self, value, **kwargs)

    def subtype(self, value=noValue, **kwargs):
        """Creates a copy of a |ASN.1| type or object.

        Any parameters to the *subtype()* method will be added to the corresponding
        properties of the |ASN.1| object.

        Parameters
        ----------
        value: :class:`unicode`, :class:`str`, :class:`bytes` or |ASN.1| object
            unicode object (Python 2) or string (Python 3), alternatively string
            (Python 2) or bytes (Python 3) representing octet-stream of serialized
            unicode string (note `encoding` parameter) or |ASN.1| class instance.

        implicitTag: :py:class:`~pyasn1.type.tag.Tag`
            Implicitly apply given ASN.1 tag object to caller's
            :py:class:`~pyasn1.type.tag.TagSet`, then use the result as
            new object's ASN.1 tag(s).

        explicitTag: :py:class:`~pyasn1.type.tag.Tag`
            Explicitly apply given ASN.1 tag object to caller's
            :py:class:`~pyasn1.type.tag.TagSet`, then use the result as
            new object's ASN.1 tag(s).

        subtypeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
            Object representing non-default ASN.1 subtype constraint(s)

        encoding: :py:class:`str`
            Unicode codec ID to encode/decode :py:class:`unicode` (Python 2) or
            :py:class:`str` (Python 3) the payload when |ASN.1| object is used
            in octet-stream context.

        Returns
        -------
        :
            new instance of |ASN.1| type/value

        """
        return univ.OctetString.subtype(self, value, **kwargs)

class NumericString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 18)
    )
    encoding = 'us-ascii'

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class PrintableString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 19)
    )
    encoding = 'us-ascii'

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class TeletexString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 20)
    )
    encoding = 'iso-8859-1'

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class T61String(TeletexString):
    __doc__ = TeletexString.__doc__

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class VideotexString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 21)
    )
    encoding = 'iso-8859-1'

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class IA5String(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 22)
    )
    encoding = 'us-ascii'

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class GraphicString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 25)
    )
    encoding = 'iso-8859-1'

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class VisibleString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 26)
    )
    encoding = 'us-ascii'

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class ISO646String(VisibleString):
    __doc__ = VisibleString.__doc__

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()

class GeneralString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 27)
    )
    encoding = 'iso-8859-1'

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class UniversalString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 28)
    )
    encoding = "utf-32-be"

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class BMPString(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 30)
    )
    encoding = "utf-16-be"

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()


class UTF8String(AbstractCharacterString):
    __doc__ = AbstractCharacterString.__doc__

    #: Set (on class, not on instance) or return a
    #: :py:class:`~pyasn1.type.tag.TagSet` object representing ASN.1 tag(s)
    #: associated with |ASN.1| type.
    tagSet = AbstractCharacterString.tagSet.tagImplicitly(
        tag.Tag(tag.tagClassUniversal, tag.tagFormatSimple, 12)
    )
    encoding = "utf-8"

    # Optimization for faster codec lookup
    typeId = AbstractCharacterString.getTypeId()

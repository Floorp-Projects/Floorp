
Documentation
=============

.. toctree::
   :maxdepth: 2

Data model for ASN.1 types
--------------------------

All ASN.1 types could be categorized into two groups: scalar (also
called simple or primitive) and constructed. The first group is
populated by well-known types like Integer or String. Members of
constructed group hold other types (simple or constructed) as their
inner components, thus they are semantically close to a programming
language records or lists.

In pyasn1, all ASN.1 types and values are implemented as Python
objects.  The same pyasn1 object can represent either ASN.1 type
and/or value depending of the presense of value initializer on object
instantiation.  We will further refer to these as *pyasn1 type object*
versus *pyasn1 value object*.

Primitive ASN.1 types are implemented as immutable scalar objects.
There values could be used just like corresponding native Python
values (integers, strings/bytes etc) and freely mixed with them in
expressions.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> asn1IntegerValue = univ.Integer(12)
   >>> asn1IntegerValue - 2
   10
   >>> univ.OctetString('abc') == 'abc'
   True   # Python 2
   >>> univ.OctetString(b'abc') == b'abc'
   True   # Python 3

It would be an error to perform an operation on a pyasn1 type object
as it holds no value to deal with:

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> asn1IntegerType = univ.Integer()
   >>> asn1IntegerType - 2
   ...
   pyasn1.error.PyAsn1Error: No value for __coerce__()


Scalar types
------------

In the sub-sections that follow we will explain pyasn1 mapping to
those primitive ASN.1 types. Both, ASN.1 notation and corresponding
pyasn1 syntax will be given in each case.

Boolean type
++++++++++++

*BOOLEAN* is the simplest type those values could be either True or
False.

.. code-block:: bash

   ;; type specification
   FunFactorPresent ::= BOOLEAN

   ;; values declaration and assignment
   pythonFunFactor FunFactorPresent ::= TRUE
   cobolFunFactor FunFactorPresent :: FALSE

And here's pyasn1 version of :py:class:`~pyasn1.type.univ.Boolean`:

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> class FunFactorPresent(univ.Boolean): pass
   ... 
   >>> pythonFunFactor = FunFactorPresent(True)
   >>> cobolFunFactor = FunFactorPresent(False)
   >>> pythonFunFactor
   FunFactorPresent('True(1)')
   >>> cobolFunFactor
   FunFactorPresent('False(0)')
   >>> pythonFunFactor == cobolFunFactor
   False
   >>>

Null type
+++++++++

The *NULL* type is sometimes used to express the absense of
information.

.. code-block:: bash

   ;; type specification
   Vote ::= CHOICE {
     agreed BOOLEAN,
     skip NULL
   }

   ;; value declaration and assignment
   myVote Vote ::= skip:NULL

We will explain the CHOICE type later on, meanwhile the
:py:class:`~pyasn1.type.univ.Null` type:

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> skip = univ.Null()
   >>> skip
   Null('')
   >>>

Integer type
++++++++++++

ASN.1 defines the values of *INTEGER* type as negative or positive of
whatever length. This definition plays nicely with Python as the
latter places no limit on Integers. However, some ASN.1
implementations may impose certain limits of integer value ranges.
Keep that in mind when designing new data structures.

.. code-block:: bash

   ;; values specification
   age-of-universe INTEGER ::= 13750000000
   mean-martian-surface-temperature INTEGER ::= -63

A rather strigntforward mapping into pyasn1 -
:py:class:`~pyasn1.type.univ.Integer`:

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> ageOfUniverse = univ.Integer(13750000000)
   >>> ageOfUniverse
   Integer(13750000000)
   >>>
   >>> meanMartianSurfaceTemperature = univ.Integer(-63)
   >>> meanMartianSurfaceTemperature
   Integer(-63)
   >>>

ASN.1 allows to assign human-friendly names to particular values of
an INTEGER type.

.. code-block:: bash

   Temperature ::= INTEGER {
     freezing(0),
     boiling(100)
   }

The Temperature type expressed in pyasn1:

.. code-block:: pycon

   >>> from pyasn1.type import univ, namedval
   >>> class Temperature(univ.Integer):
   ...   namedValues = namedval.NamedValues(('freezing', 0), ('boiling', 100))
   ...
   >>> t = Temperature(0)
   >>> t
   Temperature('freezing(0)')
   >>> t + 1
   Temperature(1)
   >>> t + 100
   Temperature('boiling(100)')
   >>> t = Temperature('boiling')
   >>> t
   Temperature('boiling(100)')
   >>> Temperature('boiling') / 2
   Temperature(50)
   >>> -1 < Temperature('freezing')
   True
   >>> 47 > Temperature('boiling')
   False

These values labels have no effect on Integer type operations, any value
still could be assigned to a type (information on value constraints will
follow further in the documentation).

Enumerated type
+++++++++++++++

ASN.1 *ENUMERATED* type differs from an Integer type in a number of
ways.  Most important is that its instance can only hold a value that
belongs to a set of values specified on type declaration.

.. code-block:: bash

   error-status ::= ENUMERATED {
     no-error(0),
     authentication-error(10),
     authorization-error(20),
     general-failure(51)
   }

When constructing :py:class:`~pyasn1.type.univ.Enumerated` type we
will use two pyasn1 features: values labels (as mentioned above) and
value constraint (will be described in more details later on).

.. code-block:: pycon

   >>> from pyasn1.type import univ, namedval, constraint
   >>> class ErrorStatus(univ.Enumerated):
   ...   namedValues = namedval.NamedValues(
   ...        ('no-error', 0),
   ...        ('authentication-error', 10),
   ...        ('authorization-error', 20),
   ...        ('general-failure', 51)
   ...   )
   ...   subtypeSpec = univ.Enumerated.subtypeSpec + \
   ...                    constraint.SingleValueConstraint(0, 10, 20, 51)
   ...
   >>> errorStatus = univ.ErrorStatus('no-error')
   >>> errorStatus
   ErrorStatus('no-error(0)')
   >>> errorStatus == univ.ErrorStatus('general-failure')
   False
   >>> univ.ErrorStatus('non-existing-state')
   Traceback (most recent call last):
   ...
   pyasn1.error.PyAsn1Error: Can't coerce non-existing-state into integer
   >>>

Particular integer values associated with Enumerated value states have
no meaning. They should not be used as such or in any kind of math
operation. Those integer values are only used by codecs to transfer
state from one entity to another.

Real type
+++++++++

Values of the *REAL* type are a three-component tuple of mantissa,
base and exponent. All three are integers.

.. code-block:: bash

   pi ::= REAL { mantissa 314159, base 10, exponent -5 }

Corresponding pyasn1 :py:class:`~pyasn1.type.univ.Real` objects can be
initialized with either a three-component tuple or a Python float.
Infinite values could be expressed in a way, compatible with Python
float type.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> pi = univ.Real((314159, 10, -5))
   >>> pi
   Real((314159, 10,-5))
   >>> float(pi)
   3.14159
   >>> pi == univ.Real(3.14159)
   True
   >>> univ.Real('inf')
   Real('inf')
   >>> univ.Real('-inf') == float('-inf')
   True
   >>>

If a Real object is initialized from a Python float or yielded by a math
operation, the base is set to decimal 10 (what affects encoding).

Bit string type
+++++++++++++++

ASN.1 *BIT STRING* type holds opaque binary data of an arbitrarily
length.  A BIT STRING value could be initialized by either a binary
(base 2) or hex (base 16) value.

.. code-block:: bash

   public-key BIT STRING ::= '1010111011110001010110101101101
                              1011000101010000010110101100010
                              0110101010000111101010111111110'B

   signature  BIT STRING ::= 'AF01330CD932093392100B39FF00DE0'H

The pyasn1 :py:class:`~pyasn1.type.univ.BitString` objects can
initialize from native ASN.1 notation (base 2 or base 16 strings) or
from a Python tuple of binary components.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> publicKey = univ.BitString(
   ...          binValue='1010111011110001010110101101101'
   ...                   '1011000101010000010110101100010'
   ...                   '0110101010000111101010111111110'
   )
   >>> publicKey
   BitString(binValue='101011101111000101011010110110110110001010100000101101011000100110101010000111101010111111110')
   >>> signature = univ.BitString(
   ...          hexValue='AF01330CD932093392100B39FF00DE0'
   ... )
   >>> signature
   BitString(binValue='1010111100000001001100110000110011011001001100100000100100110011100100100001000000001011001110011111111100000000110111100000')
   >>> fingerprint = univ.BitString(
   ...          (1, 0, 1, 1 ,0, 1, 1, 1, 0, 1, 0, 1)
   ... )
   >>> fingerprint
   BitString(binValue='101101110101')
   >>>

Another BIT STRING initialization method supported by ASN.1 notation
is to specify only 1-th bits along with their human-friendly label and
bit offset relative to the beginning of the bit string. With this
method, all not explicitly mentioned bits are doomed to be zeros.

.. code-block:: bash

   bit-mask  BIT STRING ::= {
     read-flag(0),
     write-flag(2),
     run-flag(4)
   }

To express this in pyasn1, we will employ the named values feature (as
with Enumeration type).

.. code-block:: pycon

   >>> from pyasn1.type import univ, namedval
   >>> class BitMask(univ.BitString):
   ...   namedValues = namedval.NamedValues(
   ...        ('read-flag', 0),
   ...        ('write-flag', 2),
   ...        ('run-flag', 4)
   ... )
   >>> bitMask = BitMask('read-flag,run-flag')
   >>> bitMask
   BitMask(binValue='10001')
   >>> tuple(bitMask)
   (1, 0, 0, 0, 1)
   >>> bitMask[4]
   1
   >>>

The BitString objects mimic the properties of Python tuple type in
part of immutable sequence object protocol support.

OctetString type
++++++++++++++++

The *OCTET STRING* type is a confusing subject. According to ASN.1
specification, this type is similar to BIT STRING, the major
difference is that the former operates in 8-bit chunks of data. What
is important to note, is that OCTET STRING was NOT designed to handle
text strings - the standard provides many other types specialized for
text content. For that reason, ASN.1 forbids to initialize OCTET
STRING values with "quoted text strings", only binary or hex
initializers, similar to BIT STRING ones, are allowed.

.. code-block:: bash

   thumbnail OCTET STRING ::= '1000010111101110101111000000111011'B
   thumbnail OCTET STRING ::= 'FA9823C43E43510DE3422'H

However, ASN.1 users (e.g. protocols designers) seem to ignore the
original purpose of the OCTET STRING type - they used it for handling
all kinds of data, including text strings.

.. code-block:: bash

   welcome-message OCTET STRING ::= "Welcome to ASN.1 wilderness!"

In pyasn1, we have taken a liberal approach and allowed both BIT
STRING style and quoted text initializers for the
:py:class:`~pyasn1.type.univ.OctetString` objects.  To avoid possible
collisions, quoted text is the default initialization syntax.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> thumbnail = univ.OctetString(
   ...    binValue='1000010111101110101111000000111011'
   ... )
   >>> thumbnail
   OctetString(hexValue='85eebcec0')
   >>> thumbnail = univ.OctetString(
   ...    hexValue='FA9823C43E43510DE3422'
   ... )
   >>> thumbnail
   OctetString(hexValue='fa9823c43e4351de34220')
   >>>

Most frequent usage of the OctetString class is to instantiate it with
a text string.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> welcomeMessage = univ.OctetString('Welcome to ASN.1 wilderness!')
   >>> welcomeMessage
   OctetString(b'Welcome to ASN.1 wilderness!')
   >>> print('%s' % welcomeMessage)
   Welcome to ASN.1 wilderness!
   >>> welcomeMessage[11:16]
   OctetString(b'ASN.1')
   >>> 

OctetString objects support the immutable sequence object protocol.
In other words, they behave like Python 3 bytes (or Python 2 strings).
When running pyasn1 on Python 3, it's better to use the bytes objects for
OctetString instantiation, as it's more reliable and efficient.

Additionally, OctetString's can also be instantiated with a sequence of
8-bit integers (ASCII codes).

.. code-block:: pycon

   >>> univ.OctetString((77, 101, 101, 103, 111))
   OctetString(b'Meego')

It is sometimes convenient to express OctetString instances as 8-bit
characters (Python 3 bytes or Python 2 strings) or 8-bit integers.

.. code-block:: pycon

   >>> octetString = univ.OctetString('ABCDEF')
   >>> octetString.asNumbers()
   (65, 66, 67, 68, 69, 70)
   >>> octetString.asOctets()
   b'ABCDEF'

ObjectIdentifier type
+++++++++++++++++++++

Values of the *OBJECT IDENTIFIER* type are sequences of integers that
could be used to identify virtually anything in the world. Various
ASN.1-based protocols employ OBJECT IDENTIFIERs for their own
identification needs.

.. code-block:: bash

   internet-id OBJECT IDENTIFIER ::= {
     iso(1) identified-organization(3) dod(6) internet(1)
   }

One of the natural ways to map OBJECT IDENTIFIER type into a Python
one is to use Python tuples of integers. So this approach is taken by
pyasn1's :py:class:`~pyasn1.type.univ.ObjectIdentifier` class.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> internetId = univ.ObjectIdentifier((1, 3, 6, 1))
   >>> internetId
   ObjectIdentifier('1.3.6.1')
   >>> internetId[2]
   6
   >>> internetId[1:3]
   ObjectIdentifier('3.6')

A more human-friendly "dotted" notation is also supported.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> univ.ObjectIdentifier('1.3.6.1')
   ObjectIdentifier('1.3.6.1')

Symbolic names of the arcs of object identifier, sometimes present in
ASN.1 specifications, are not preserved and used in pyasn1 objects.

The ObjectIdentifier objects mimic the properties of Python tuple type in
part of immutable sequence object protocol support.

Any type
++++++++

The ASN.1 ANY type is a kind of wildcard or placeholder that matches
any other type without knowing it in advance. ANY has no base tag.

.. code-block:: bash

   Error ::= SEQUENCE {
     code      INTEGER,
     parameter ANY DEFINED BY code
   }

The ANY type is frequently used in specifications, where exact type is
not yet agreed upon between communicating parties or the number of
possible alternatives of a type is infinite.  Sometimes an auxiliary
selector is kept around to help parties indicate the kind of ANY
payload in effect ("code" in the example above).

Values of the ANY type contain serialized ASN.1 value(s) in form of an
octet string. Therefore pyasn1 :py:class:`~pyasn1.type.univ.Any` value
object share the properties of pyasn1 OctetString object.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> someValue = univ.Any(b'\x02\x01\x01')
   >>> someValue
   Any(b'\x02\x01\x01')
   >>> str(someValue)
   '\x02\x01\x01'
   >>> bytes(someValue)
   b'\x02\x01\x01'
   >>>

Receiving application is supposed to explicitly deserialize the
content of Any value object, possibly using auxiliary selector for
figuring out its ASN.1 type to pick appropriate decoder.

There will be some more talk and code snippets covering Any type in
the codecs chapters that follow.

Character string types
++++++++++++++++++++++

ASN.1 standard introduces a diverse set of text-specific types. All of
them were designed to handle various types of characters. Some of
these types seem be obsolete nowdays, as their target technologies are
gone. Another issue to be aware of is that raw OCTET STRING type is
sometimes used in practice by ASN.1 users instead of specialized
character string types, despite explicit prohibition imposed by ASN.1
specification.

The two types are specific to ASN.1 are NumericString and PrintableString.

.. code-block:: bash

   welcome-message ::= PrintableString {
     "Welcome to ASN.1 text types"
   }

   dial-pad-numbers ::= NumericString {
     "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
   }

Their pyasn1 implementations are
:py:class:`~pyasn1.type.char.PrintableString` and
:py:class:`~pyasn1.type.char.NumericString`:

.. code-block:: pycon

   >>> from pyasn1.type import char
   >>> '%s' % char.PrintableString("Welcome to ASN.1 text types")
   'Welcome to ASN.1 text types'
   >>> dialPadNumbers = char.NumericString(
     "0" "1" "2" "3" "4" "5" "6" "7" "8" "9"
   )
   >>> dialPadNumbers
   NumericString(b'0123456789')
   >>>

The :py:class:`~pyasn1.type.char.VisibleString`,
:py:class:`~pyasn1.type.char.IA5String`,
:py:class:`~pyasn1.type.char.TeletexString`,
:py:class:`~pyasn1.type.char.VideotexString`,
:py:class:`~pyasn1.type.char.GraphicString` and
:py:class:`~pyasn1.type.char.GeneralString` types came to ASN.1 from
ISO standards on character sets.

.. code-block:: pycon

   >>> from pyasn1.type import char
   >>> char.VisibleString("abc")
   VisibleString(b'abc')
   >>> char.IA5String('abc')
   IA5String(b'abc')
   >>> char.TeletexString('abc')
   TeletexString(b'abc')
   >>> char.VideotexString('abc')
   VideotexString(b'abc')
   >>> char.GraphicString('abc')
   GraphicString(b'abc')
   >>> char.GeneralString('abc')
   GeneralString(b'abc')
   >>>

The last three types are relatively recent addition to the family of
character string types: :py:class:`~pyasn1.type.char.UniversalString`,
:py:class:`~pyasn1.type.char.BMPString` and
:py:class:`~pyasn1.type.char.UTF8String`.

.. code-block:: pycon

   >>> from pyasn1.type import char
   >>> char.UniversalString("abc")
   UniversalString(b'abc')
   >>> char.BMPString('abc')
   BMPString(b'abc')
   >>> char.UTF8String('abc')
   UTF8String(b'abc')
   >>> utf8String = char.UTF8String('У попа была собака')
   >>> utf8String
   UTF8String(b'\xd0\xa3 \xd0\xbf\xd0\xbe\xd0\xbf\xd0\xb0 \xd0\xb1\xd1\x8b\xd0\xbb\xd0\xb0\xd1\x81\xd0\xbe\xd0\xb1\xd0\xb0\xd0\xba\xd0\xb0')
   >>> print(utf8String)
   У попа была собака
   >>>

In pyasn1, all character type objects behave like Python strings. None
of them is currently constrained in terms of valid alphabet so it's up
to the data source to keep an eye on data validation for these types.

Useful types
++++++++++++

There are three so-called useful types defined in the standard:
:py:class:`~pyasn1.type.useful.ObjectDescriptor`,
:py:class:`~pyasn1.type.useful.GeneralizedTime` and
:py:class:`~pyasn1.type.useful.UTCTime`. They all are subtypes of
GraphicString or VisibleString types therefore useful types are
character string types.

It's advised by the ASN.1 standard to have an instance of
ObjectDescriptor type holding a human-readable description of
corresponding instance of OBJECT IDENTIFIER type. There are no formal
linkage between these instances and provision for ObjectDescriptor
uniqueness in the standard.

.. code-block:: pycon

   >>> from pyasn1.type import useful
   >>> descrBER = useful.ObjectDescriptor(
         "Basic encoding of a single ASN.1 type"
   )
   >>> 

GeneralizedTime and UTCTime types are designed to hold a
human-readable timestamp in a universal and unambiguous form. The
former provides more flexibility in notation while the latter is more
strict but has Y2K issues.

.. code-block:: bash

   ;; Mar 8 2010 12:00:00 MSK
   moscow-time GeneralizedTime ::= "20110308120000.0"
   ;; Mar 8 2010 12:00:00 UTC
   utc-time GeneralizedTime ::= "201103081200Z"
   ;; Mar 8 1999 12:00:00 UTC
   utc-time UTCTime ::= "9803081200Z"

In pyasn1 parlance:

.. code-block:: pycon

   >>> from pyasn1.type import useful
   >>> moscowTime = useful.GeneralizedTime("20110308120000.0")
   >>> utcTime = useful.UTCTime("9803081200Z")
   >>> 

Despite their intended use, these types possess no special, time-related,
handling in pyasn1. They are just printable strings.

Tagging
-------

In order to proceed to the Constructed ASN.1 types, we will first have
to introduce the concept of tagging (and its pyasn1 implementation), as
some of the Constructed types rely upon the tagging feature.

When a value is coming into an ASN.1-based system (received from a network
or read from some storage), the receiving entity has to determine the
type of the value to interpret and verify it accordingly.

Historically, the first data serialization protocol introduced in
ASN.1 was BER (Basic Encoding Rules). According to BER, any serialized
value is packed into a triplet of (Type, Length, Value) where Type is a 
code that identifies the value (which is called *tag* in ASN.1),
length is the number of bytes occupied by the value in its serialized form
and value is ASN.1 value in a form suitable for serial transmission or storage.
For that reason almost every ASN.1 type has a tag (which is actually a
BER type) associated with it by default.

An ASN.1 tag could be viewed as a tuple of three numbers:
(Class, Format, Number). While Number identifies a tag, Class component 
is used to create scopes for Numbers. Four scopes are currently defined:
UNIVERSAL, context-specific, APPLICATION and PRIVATE. The Format component
is actually a one-bit flag - zero for tags associated with scalar types,
and one for constructed types (will be discussed later on).

.. code-block:: bash

   MyIntegerType ::= [12] INTEGER
   MyOctetString ::= [APPLICATION 0] OCTET STRING

In pyasn1, tags are implemented as immutable, tuple-like objects:

.. code-block:: pycon

   >>> from pyasn1.type import tag
   >>> myTag = tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 10)
   >>> myTag
   Tag(tagClass=128, tagFormat=0, tagId=10)
   >>> tuple(myTag)
   (128, 0, 10)
   >>> myTag[2]
   10
   >>> myTag == tag.Tag(tag.tagClassApplication, tag.tagFormatSimple, 10)
   False
   >>>

Default tag, associated with any ASN.1 type, could be extended or
replaced to make new type distinguishable from its ancestor. The
standard provides two modes of tag mangling - IMPLICIT and EXPLICIT.

EXPLICIT mode works by appending new tag to the existing ones thus
creating an ordered set of tags. This set will be considered as a
whole for type identification and encoding purposes. Important
property of EXPLICIT tagging mode is that it preserves base type
information in encoding what makes it possible to completely recover
type information from encoding.

When tagging in IMPLICIT mode, the outermost existing tag is dropped
and replaced with a new one.

.. code-block:: bash

   MyIntegerType ::= [12] IMPLICIT INTEGER
   MyOctetString ::= [APPLICATION 0] EXPLICIT OCTET STRING

To model both modes of tagging, a specialized container TagSet object
(holding zero, one or more Tag objects) is used in pyasn1.

.. code-block:: pycon

   >>> from pyasn1.type import tag
   >>> tagSet = tag.TagSet(
   ...   # base tag (OBSOLETE AND NOT USED ANYMORE)
   ...   (),
   ...   # effective tag
   ...   tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 10)
   ... )
   >>> tagSet
   TagSet((), Tag(tagClass=128, tagFormat=0, tagId=10))
   >>> tagSet.getBaseTag()
   Tag(tagClass=128, tagFormat=0, tagId=10)
   >>> tagSet = tagSet.tagExplicitly(tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 20))
   >>> tagSet
   TagSet((), Tag(tagClass=128, tagFormat=0, tagId=10), 
   Tag(tagClass=128, tagFormat=32, tagId=20))
   >>> tagSet = tagSet.tagExplicitly(tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 30))
   >>> tagSet
   TagSet((), Tag(tagClass=128, tagFormat=0, tagId=10), 
   Tag(tagClass=128, tagFormat=32, tagId=20), 
   Tag(tagClass=128, tagFormat=32, tagId=30))
   >>> tagSet = tagSet.tagImplicitly(tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 40))
   >>> tagSet
   TagSet((), Tag(tagClass=128, tagFormat=0, tagId=10),
   Tag(tagClass=128, tagFormat=32, tagId=20),
   Tag(tagClass=128, tagFormat=32, tagId=40))
   >>> 

As a side note: the "base tag" concept is now obsolete and not used.
The "effective tag" is the one that always appears in encoding and is
used on tagSets comparation.

Any two TagSet objects could be compared to see if one is a derivative
of the other. Figuring this out is also useful in cases when a type-specific
data processing algorithms are to be chosen.

.. code-block:: pycon

   >>> from pyasn1.type import tag
   >>> tagSet1 = tag.TagSet(
   ...   # base tag (OBSOLETE AND NOT USED ANYMORE)
   ...   (),
   ...   # effective tag
   ...   tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 10)
   ... )
   >>> tagSet2 = tagSet1.tagExplicitly(tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 20))
   >>> tagSet1.isSuperTagSetOf(tagSet2)
   True
   >>> tagSet2.isSuperTagSetOf(tagSet1)
   False
   >>> 

We will complete this discussion on tagging with a real-world example. The
following ASN.1 tagged type:

.. code-block:: bash

   MyIntegerType ::= [12] EXPLICIT INTEGER

could be expressed in pyasn1 like this:

.. code-block:: pycon

   >>> from pyasn1.type import univ, tag
   >>> class MyIntegerType(univ.Integer):
   ...   tagSet = univ.Integer.tagSet.tagExplicitly(tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 12))
   >>> myInteger = MyIntegerType(12345)
   >>> myInteger.tagSet
   TagSet((), Tag(tagClass=0, tagFormat=0, tagId=2), 
   Tag(tagClass=128, tagFormat=32, tagId=12))
   >>>

Referring to the above code, the tagSet class attribute is a property
of any pyasn1 type object that assigns default tagSet to a pyasn1
value object. This default tagSet specification can be ignored and
effectively replaced by some other tagSet value passed on object
instantiation.

It's important to understand that the tag set property of pyasn1 type/value
object can never be modifed in place. In other words, a pyasn1 type/value
object can never change its tags. The only way is to create a new pyasn1
type/value object and associate different tag set with it.

Constructed types
-----------------

Besides scalar types, ASN.1 specifies so-called constructed ones - these
are capable of holding one or more values of other types, both scalar
and constructed.

In pyasn1 implementation, constructed ASN.1 types behave like 
Python sequences, and also support additional component addressing methods,
specific to particular constructed type.

Sequence and Set types
++++++++++++++++++++++

The *SEQUENCE* and *SET* types have many similar properties:

* Both can hold any number of inner components of different types.
* Every component has a human-friendly identifier.
* Any component can have a default value.
* Some components can be absent.

However, :py:class:`~pyasn1.type.univ.Sequence` type guarantees the
ordering of Sequence value components to match their declaration
order. By contrast, components of the
:py:class:`~pyasn1.type.univ.Set` type can be ordered to best suite
application's needs.

.. code-block:: bash

   Record ::= SEQUENCE {
     id        INTEGER,
     room  [0] INTEGER OPTIONAL,
     house [1] INTEGER DEFAULT 0
   }

Up to this moment, the only method we used for creating new pyasn1
types is Python sub-classing. With this method, a new, named Python
class is created what mimics type derivation in ASN.1 grammar.
However, ASN.1 also allows for defining anonymous subtypes (room and
house components in the example above).  To support anonymous
subtyping in pyasn1, a cloning operation on an existing pyasn1 type
object can be invoked what creates a new instance of original object
with possibly modified properties.

.. code-block:: pycon

   >>> from pyasn1.type import univ, namedtype, tag
   >>> class Record(univ.Sequence):
   ...   componentType = namedtype.NamedTypes(
   ...     namedtype.NamedType('id', univ.Integer()),
   ...     namedtype.OptionalNamedType(
   ...       'room',
   ...       univ.Integer().subtype(
   ...         implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 0)
   ...       )
   ...     ),
   ...     namedtype.DefaultedNamedType(
   ...       'house', 
   ...       univ.Integer(0).subtype(
   ...         implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 1)
   ...       )
   ...     )
   ...   )
   >>>

All pyasn1 constructed type classes have a class attribute
**componentType** that represent default type specification. Its
value is a NamedTypes object.

The NamedTypes class instance holds a sequence of NameType,
OptionalNamedType or DefaultedNamedType objects which, in turn, refer
to pyasn1 type objects that represent inner SEQUENCE components
specification.

Finally, invocation of a subtype() method of pyasn1 type objects in
the code above returns an implicitly tagged copy of original object.

Once a SEQUENCE or SET type is decleared with pyasn1, it can be
instantiated and initialized (continuing the above code):

.. code-block:: pycon

   >>> record = Record()
   >>> record['id'] = 123
   >>> print(record.prettyPrint())
   Record:
    id=123
   >>> 
   >>> record[1] = 321
   >>> print(record.prettyPrint())
   Record:
    id=123
    room=321
   >>>
   >>> record.setDefaultComponents()
   >>> print(record.prettyPrint())
   Record:
    id=123
    room=321
    house=0

Inner components of pyasn1 Sequence/Set objects could be accessed
using the following methods:

.. code-block:: pycon

   >>> record['id']
   Integer(123)
   >>> record[1]
   Integer(321)
   >>> record[2]
   Integer(0)
   >>> for idx, field in enumerate(record):
   ...   print(record.componentType[idx].name, field)
   id 123
   room 321
   house 0
   >>>

The Set type share all the properties of Sequence type, and additionally
support by-tag component addressing (as all Set components have distinct
types).

.. code-block:: pycon

   >>> from pyasn1.type import univ, namedtype, tag
   >>> class Gamer(univ.Set):
   ...   componentType = namedtype.NamedTypes(
   ...     namedtype.NamedType('score', univ.Integer()),
   ...     namedtype.NamedType('player', univ.OctetString()),
   ...     namedtype.NamedType('id', univ.ObjectIdentifier())
   ...   )
   >>> gamer = Gamer()
   >>> gamer.setComponentByType(univ.Integer().tagSet, 121343)
   >>> gamer.setComponentByType(univ.OctetString().tagSet, 'Pascal')
   >>> gamer.setComponentByType(univ.ObjectIdentifier().tagSet, (1,3,7,2))
   >>> print(gamer.prettyPrint())
   Gamer:
    score=121343
    player=b'Pascal'
    id=1.3.7.2

SequenceOf and SetOf types
++++++++++++++++++++++++++

Both, *SEQUENCE OF* and *SET OF* types resemble an unlimited size list of
components.  All the components must be of the same type.

.. code-block:: bash

   Progression ::= SEQUENCE OF INTEGER

   arithmeticProgression Progression ::= { 1, 3, 5, 7 }

:py:class:`~pyasn1.type.univ.SequenceOf` and
:py:class:`~pyasn1.type.univ.SetOf` types are expressed by the very
similar pyasn1 `list` type objects. Their components can only be addressed by
position and they both have a property of automatic resize.

To specify inner component type, the **componentType** class
attribute should refer to another pyasn1 type object.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> class Progression(univ.SequenceOf):
   ...   componentType = univ.Integer()
   >>> arithmeticProgression = Progression()
   >>> arithmeticProgression[1] = 111
   >>> print(arithmeticProgression.prettyPrint())
   Progression:
   -empty- 111
   >>> arithmeticProgression[0] = 100
   >>> print(arithmeticProgression.prettyPrint())
   Progression:
   100 111
   >>>
   >>> for element in arithmeticProgression:
   ...    element
   Integer(100)
   Integer(111)
   >>>

Any scalar or constructed pyasn1 type object can serve as an inner
component.  Missing components are prohibited in SequenceOf/SetOf
value objects.

Choice type
+++++++++++

Values of ASN.1 *CHOICE* type can contain only a single value of a type
from a list of possible alternatives. Alternatives must be ASN.1 types
with distinct tags for the whole structure to remain unambiguous.
Unlike most other types, CHOICE is an untagged one, e.g. it has no
base tag of its own.

.. code-block:: bash

   CodeOrMessage ::= CHOICE {
     code    INTEGER,
     message OCTET STRING
   }

In pyasn1 implementation,
:py:class:`~pyasn1.type.univ.Choice` object behaves like Set but
accepts only a single inner component at a time. It also offers a few
additional methods specific to its behaviour.

.. code-block:: pycon

   >>> from pyasn1.type import univ, namedtype
   >>> class CodeOrMessage(univ.Choice):
   ...   componentType = namedtype.NamedTypes(
   ...     namedtype.NamedType('code', univ.Integer()),
   ...     namedtype.NamedType('message', univ.OctetString())
   ...   )
   >>>
   >>> codeOrMessage = CodeOrMessage()
   >>> print(codeOrMessage.prettyPrint())
   CodeOrMessage:
   >>> codeOrMessage['code'] = 123
   >>> print(codeOrMessage.prettyPrint())
   CodeOrMessage:
    code=123
   >>> codeOrMessage['message'] = 'my string value'
   >>> print(codeOrMessage.prettyPrint())
   CodeOrMessage:
    message=b'my string value'
   >>>

Since there could be only a single inner component value in the pyasn1
Choice value object, either of the following methods could be used for
fetching it (continuing previous code):

.. code-block:: pycon

   >>> codeOrMessage.getName()
   'message'
   >>> codeOrMessage.getComponent()
   OctetString(b'my string value')
   >>>

Subtype constraints
-------------------

Most ASN.1 types can correspond to an infinite set of values. To adapt
to particular application's data model and needs, ASN.1 provides a
mechanism for limiting the infinite set to values, that make sense in
particular case.  Imposing value constraints on an ASN.1 type can also
be seen as creating a subtype from its base type.

In pyasn1, constraints take shape of immutable objects capable
of evaluating given value against constraint-specific requirements.
Constraint object is a property of pyasn1 type. Like TagSet property,
associated with every pyasn1 type, constraints can never be modified
in place. The only way to modify pyasn1 type constraint is to associate
new constraint object to a new pyasn1 type object.

A handful of different flavors of *constraints* are defined in
ASN.1.  We will discuss them one by one in the following chapters and
also explain how to combine and apply them to types.

Single value constraint
+++++++++++++++++++++++

This kind of constraint allows for limiting type to a finite, specified set
of values.

.. code-block:: bash

   DialButton ::= OCTET STRING (
     "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
   )

Its pyasn1 implementation would look like:

.. code-block:: pycon

   >>> from pyasn1.type import constraint
   >>> c = constraint.SingleValueConstraint('0','1','2','3','4','5','6','7','8','9')
   >>> c
   SingleValueConstraint(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)
   >>> c('0')
   >>> c('A')
   Traceback (most recent call last):
   ...
   ValueConstraintError:
     SingleValueConstraint(0, 1, 2, 3, 4, 5, 6, 7, 8, 9) failed at: A
   >>>

As can be seen in the snippet above, if a value violates the
constraint, an exception will be thrown. A constrainted pyasn1 type
object holds a reference to a constraint object (or their combination,
as will be explained later) and calls it for value verification.

.. code-block:: pycon

   >>> from pyasn1.type import univ, constraint
   >>> class DialButton(univ.OctetString):
   ...   subtypeSpec = constraint.SingleValueConstraint(
   ...       '0','1','2','3','4','5','6','7','8','9'
   ...   )
   >>> DialButton('0')
   DialButton(b'0')
   >>> DialButton('A')
   Traceback (most recent call last):
   ...
   ValueConstraintError:
     SingleValueConstraint(0, 1, 2, 3, 4, 5, 6, 7, 8, 9) failed at: A
   >>> 

Constrained pyasn1 value object can never hold a violating value.

Value range constraint
++++++++++++++++++++++

A pair of values, compliant to a type to be constrained, denote low
and upper bounds of allowed range of values of a type.

.. code-block:: bash

   Teenagers ::= INTEGER (13..19)

And in pyasn1 terms:

.. code-block:: pycon

   >>> from pyasn1.type import univ, constraint
   >>> class Teenagers(univ.Integer):
   ...   subtypeSpec = constraint.ValueRangeConstraint(13, 19)
   >>> Teenagers(14)
   Teenagers(14)
   >>> Teenagers(20)
   Traceback (most recent call last):
   ...
   ValueConstraintError:
     ValueRangeConstraint(13, 19) failed at: 20
   >>> 

ASN.1 MIN and MAX operands can be substituted with floating point
infinity values.

.. code-block:: bash

   NegativeInt ::= INTEGER (MIN..-1)
   PositiveInt ::= INTEGER (1..MAX)

And in pyasn1 terms:

.. code-block:: pycon

   >>> from pyasn1.type import univ, constraint
   >>> class NegativeInt(univ.Integer):
   ...   subtypeSpec = constraint.ValueRangeConstraint(float('-inf'), -1)
   >>> NegativeInt(-1)
   NegativeInt(-1)
   >>> NegativeInt(0)
   Traceback (most recent call last):
   ...
   ValueConstraintError:
     ValueConstraintError: ValueRangeConstraint() failed at: "0" at NegativeInt
   >>> class PositiveInt(univ.Integer):
   ...   subtypeSpec = constraint.ValueRangeConstraint(1, float('inf'))
   >> PositiveInt(1)
   PositiveInt(1)
   >> PositiveInt(4)
   PositiveInt(4)
   >> PositiveInt(-1)
   Traceback (most recent call last):
   ...
   ValueConstraintError:
     ValueConstraintError: ValueRangeConstraint() failed at: "-1" at PositiveInt

Value range constraint usually applies to numeric types.

Size constraint
+++++++++++++++

It is sometimes convenient to set or limit the allowed size of a data
item to be sent from one application to another to manage bandwidth
and memory consumption issues. Size constraint specifies the lower and
upper bounds of the size of a valid value.

.. code-block:: bash

   TwoBits ::= BIT STRING (SIZE (2))

Express the same grammar in pyasn1:

.. code-block:: pycon

   >>> from pyasn1.type import univ, constraint
   >>> class TwoBits(univ.BitString):
   ...   subtypeSpec = constraint.ValueSizeConstraint(2, 2)
   >>> TwoBits((1,1))
   TwoBits("'11'B")
   >>> TwoBits((1,1,0))
   Traceback (most recent call last):
   ...
   ValueConstraintError: ValueSizeConstraint(2, 2) failed at: (1, 1, 0)
   >>> 

Size constraint can be applied to potentially massive values - bit or
octet strings, SEQUENCE OF/SET OF values.

Alphabet constraint
+++++++++++++++++++

The permitted alphabet constraint is similar to Single value
constraint but constraint applies to individual characters of a value. 

.. code-block:: bash

   MorseCode ::= PrintableString (FROM ("."|"-"|" "))

And in pyasn1:

.. code-block:: pycon

   >>> from pyasn1.type import char, constraint
   >>> class MorseCode(char.PrintableString):
   ...   subtypeSpec = constraint.PermittedAlphabetConstraint(".", "-", " ")
   >>> MorseCode("...---...")
   MorseCode('...---...')
   >>> MorseCode("?")
   Traceback (most recent call last):
   ...
   ValueConstraintError: PermittedAlphabetConstraint(".", "-", " ") failed at: "?"
   >>> 

Current implementation does not handle ranges of characters in
constraint (FROM "A".."Z" syntax), one has to list the whole set in a
range.

Constraint combinations
+++++++++++++++++++++++

Up to this moment, we used a single constraint per ASN.1 type. The
standard, however, allows for combining multiple individual
constraints into intersections, unions and exclusions.

In pyasn1 data model, all of these methods of constraint combinations
are implemented as constraint-like objects holding individual
constraint (or combination) objects. Like terminal constraint objects,
combination objects are capable to perform value verification at its
set of enclosed constraints according to the logic of particular
combination.

Constraints intersection verification succeeds only if a value is
compliant to each constraint in a set. To begin with, the following
specification will constitute a valid telephone number:

.. code-block:: bash

   PhoneNumber ::= NumericString (FROM ("0".."9")) (SIZE 11)

Constraint intersection object serves the logic above:

.. code-block:: pycon

   >>> from pyasn1.type import char, constraint
   >>> class PhoneNumber(char.NumericString):
   ...   subtypeSpec = constraint.ConstraintsIntersection(
   ...     constraint.PermittedAlphabetConstraint('0','1','2','3','4','5','6', '7','8','9'),
   ...     constraint.ValueSizeConstraint(11, 11)
   ...   )
   >>> PhoneNumber('79039343212')
   PhoneNumber('79039343212')
   >>> PhoneNumber('?9039343212')
   Traceback (most recent call last):
   ...
   ValueConstraintError: ConstraintsIntersection(PermittedAlphabetConstraint('0','1','2','3','4','5','6','7','8','9'), ValueSizeConstraint(11, 11)) failed at: PermittedAlphabetConstraint('0','1','2','3','4','5','6','7','8','9') failed at: "?039343212"
   >>> PhoneNumber('9343212')
   Traceback (most recent call last):
   ...
   ValueConstraintError:
     ConstraintsIntersection(PermittedAlphabetConstraint('0','1','2','3','4','5','6','7','8','9'), ValueSizeConstraint(11, 11)) failed at: ValueSizeConstraint(10, 10) failed at: "9343212"
   >>>

Union of constraints works by making sure that a value is compliant
to any of the constraint in a set. For instance:

.. code-block:: bash

   CapitalOrSmall ::= IA5String (FROM ('A','B','C') | FROM ('a','b','c'))

It's important to note, that a value must fully comply to any single
constraint in a set. In the specification above, a value of all small
or all capital letters is compliant, but a mix of small&capitals is
not.  Here's its pyasn1 analogue:

.. code-block:: pycon

   >>> from pyasn1.type import char, constraint
   >>> class CapitalOrSmall(char.IA5String):
   ...   subtypeSpec = constraint.ConstraintsUnion(
   ...     constraint.PermittedAlphabetConstraint('A','B','C'),
   ...     constraint.PermittedAlphabetConstraint('a','b','c')
   ...   )
   >>> CapitalOrSmall('ABBA')
   CapitalOrSmall('ABBA')
   >>> CapitalOrSmall('abba')
   CapitalOrSmall('abba')
   >>> CapitalOrSmall('Abba')
   Traceback (most recent call last):
   ...
   ValueConstraintError: ConstraintsUnion(PermittedAlphabetConstraint('A', 'B', 'C'), PermittedAlphabetConstraint('a', 'b', 'c')) failed at: failed for "Abba"
   >>>

Finally, the exclusion constraint simply negates the logic of value
verification at a constraint. In the following example, any integer
value is allowed in a type but not zero.

.. code-block:: bash

   NoZero ::= INTEGER (ALL EXCEPT 0)

In pyasn1 the above definition would read:

.. code-block:: pycon

   >>> from pyasn1.type import univ, constraint
   >>> class NoZero(univ.Integer):
   ...   subtypeSpec = constraint.ConstraintsExclusion(
   ...     constraint.SingleValueConstraint(0)
   ...   )
   >>> NoZero(1)
   NoZero(1)
   >>> NoZero(0)
   Traceback (most recent call last):
   ...
   ValueConstraintError: ConstraintsExclusion(SingleValueConstraint(0)) failed at: 0
   >>>

The depth of such a constraints tree, built with constraint
combination objects at its nodes, has not explicit limit. Value
verification is performed in a recursive manner till a definite
solution is found.

Types relationships
+++++++++++++++++++

In the course of data processing in an application, it is sometimes
convenient to figure out the type relationships between pyasn1 type or
value objects. Formally, two things influence pyasn1 types
relationship: *tag set* and *subtype constraints*. One
pyasn1 type is considered to be a derivative of another if their
TagSet and Constraint objects are a derivation of one another.

The following example illustrates the concept (we use the same tagset
but different constraints for simplicity):

.. code-block:: pycon

   >>> from pyasn1.type import univ, constraint
   >>> i1 = univ.Integer(subtypeSpec=constraint.ValueRangeConstraint(3,8))
   >>> i2 = univ.Integer(subtypeSpec=constraint.ConstraintsIntersection(
   ...    constraint.ValueRangeConstraint(3,8),
   ...    constraint.ValueRangeConstraint(4,7)
   ... ) )
   >>> i1.isSameTypeWith(i2)
   False
   >>> i1.isSuperTypeOf(i2)
   True
   >>> i1.isSuperTypeOf(i1)
   True
   >>> i2.isSuperTypeOf(i1)
   False
   >>>

As can be seen in the above code snippet, there are two methods of any
pyasn1 type/value object that test types for their relationship:
*isSameTypeWith()* and *isSuperTypeOf()*. The former is
self-descriptive while the latter yields true if the argument appears
to be a pyasn1 object which has tagset and constraints derived from
those of the object being called.

Serialization codecs
--------------------

In ASN.1 context, `codec <http://en.wikipedia.org/wiki/Codec>`_
is a program that transforms between concrete data structures and a stream
of octets, suitable for transmission over the wire. This serialized form of
data is sometimes called *substrate* or *essence*.

In pyasn1 implementation, substrate takes shape of Python 3 bytes or 
Python 2 string objects.

One of the properties of a codec is its ability to cope with
incomplete data and/or substrate what implies codec to be stateful. In
other words, when decoder runs out of substrate and data item being
recovered is still incomplete, stateful codec would suspend and
complete data item recovery whenever the rest of substrate becomes
available. Similarly, stateful encoder would encode data items in
multiple steps waiting for source data to arrive. Codec restartability
is especially important when application deals with large volumes of
data and/or runs on low RAM. For an interesting discussion on codecs
options and design choices, refer to `Apache ASN.1 project
<http://directory.apache.org/subprojects/asn1/>`_ .

As of this writing, codecs implemented in pyasn1 are all stateless,
mostly to keep the code simple.

The pyasn1 package currently supports 
`BER <http://en.wikipedia.org/wiki/Basic_encoding_rules>`_ codec and
its variations -- 
`CER <http://en.wikipedia.org/wiki/Canonical_encoding_rules>`_ and
`DER <http://en.wikipedia.org/wiki/Distinguished_encoding_rules>`_.
More ASN.1 codecs are planned for implementation in the future.

Encoders
++++++++

Encoder is used for transforming pyasn1 value objects into substrate.
Only pyasn1 value objects could be serialized, attempts to process
pyasn1 type objects will cause encoder failure.

The following code will create a pyasn1 Integer object and serialize
it with BER encoder:

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> from pyasn1.codec.ber import encoder
   >>> encoder.encode(univ.Integer(123456))
   b'\x02\x03\x01\xe2@'
   >>>

BER standard also defines a so-called *indefinite length*
encoding form which makes large data items processing more memory
efficient. It is mostly useful when encoder does not have the whole
value all at once and the length of the value can not be determined at
the beginning of encoding.

*Constructed encoding* is another feature of BER closely related to
the indefinite length form. In essence, a large scalar value (such as
ASN.1 character BitString type) could be chopped into smaller chunks
by encoder and transmitted incrementally to limit memory consumption.
Unlike indefinite length case, the length of the whole value must be
known in advance when using constructed, definite length encoding
form.

Since pyasn1 codecs are not restartable, pyasn1 encoder may only
encode data item all at once. However, even in this case, generating
indefinite length encoding may help a low-memory receiver, running a
restartable decoder, to process a large data item.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> from pyasn1.codec.ber import encoder
   >>> encoder.encode(
   ...   univ.OctetString('The quick brown fox jumps over the lazy dog'),
   ...   defMode=False,
   ...   maxChunkSize=8
   ... )
   b'$\x80\x04\x08The quic\x04\x08k brown \x04\x08fox jump\x04\x08s over \t\x04\x08he lazy \x04\x03dog\x00\x00'
   >>>
   >>> encoder.encode(
   ...   univ.OctetString('The quick brown fox jumps over the lazy dog'),
   ...   maxChunkSize=8
   ... )
   b'$7\x04\x08The quic\x04\x08k brown \x04\x08fox jump\x04\x08s over \t\x04\x08he lazy \x04\x03dog'

The *defMode* encoder parameter disables definite length encoding
mode, while the optional *maxChunkSize* parameter specifies desired
substrate chunk size that influences memory requirements at the
decoder's end.

To use CER or DER encoders one needs to explicitly import and call them - the
APIs are all compatible.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> from pyasn1.codec.ber import encoder as ber_encoder
   >>> from pyasn1.codec.cer import encoder as cer_encoder
   >>> from pyasn1.codec.der import encoder as der_encoder
   >>> ber_encoder.encode(univ.Boolean(True))
   b'\x01\x01\x01'
   >>> cer_encoder.encode(univ.Boolean(True))
   b'\x01\x01\xff'
   >>> der_encoder.encode(univ.Boolean(True))
   b'\x01\x01\xff'
   >>>

Decoders
++++++++

In the process of decoding, pyasn1 value objects are created and
linked to each other, based on the information containted in the
substrate. Thus, the original pyasn1 value object(s) are recovered.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> from pyasn1.codec.ber import encoder, decoder
   >>> substrate = encoder.encode(univ.Boolean(True))
   >>> decoder.decode(substrate)
   (Boolean('True(1)'), b'')
   >>>

Commenting on the code snippet above, pyasn1 decoder accepts substrate
as an argument and returns a tuple of pyasn1 value object (possibly a
top-level one in case of constructed object) and unprocessed part of
input substrate.

All pyasn1 decoders can handle both definite and indefinite length
encoding modes automatically, explicit switching into one mode to
another is not required.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> from pyasn1.codec.ber import encoder, decoder
   >>> substrate = encoder.encode(
   ...   univ.OctetString('The quick brown fox jumps over the lazy dog'),
   ...   defMode=False,
   ...   maxChunkSize=8
   ... )
   >>> decoder.decode(substrate)
   (OctetString(b'The quick brown fox jumps over the lazy dog'), b'')
   >>>

Speaking of BER/CER/DER encoding, in many situations substrate may not
contain all necessary information needed for complete and accurate
ASN.1 values recovery. The most obvious cases include implicitly
tagged ASN.1 types and constrained types.

As discussed earlier in this tutorial, when an ASN.1 type is implicitly
tagged, previous outermost tag is lost and never appears in substrate.
If it is the base tag that gets lost, decoder is unable to pick type-specific
value decoder at its table of built-in types, and therefore recover
the value part, based only on the information contained in substrate. The
approach taken by pyasn1 decoder is to use a prototype pyasn1 type object (or
a set of them) to *guide* the decoding process by matching [possibly
incomplete] tags recovered from substrate with those found in prototype pyasn1
type objects (also called pyasn1 specification object further in this
document).

.. code-block:: pycon

   >>> from pyasn1.codec.ber import decoder
   >>> decoder.decode(b'\x02\x01\x0c', asn1Spec=univ.Integer())
   Integer(12), b''
   >>>

Decoder would neither modify pyasn1 specification object nor use its
current values (if it's a pyasn1 value object), but rather use it as a
hint for choosing proper decoder and as a pattern for creating new
objects:

.. code-block:: pycon

   >>> from pyasn1.type import univ, tag
   >>> from pyasn1.codec.ber import encoder, decoder
   >>> i = univ.Integer(12345).subtype(
   ...   implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 40)
   ... )
   >>> substrate = encoder.encode(i)
   >>> substrate
   b'\x9f(\x0209'
   >>> decoder.decode(substrate)
   Traceback (most recent call last):
   ...
   pyasn1.error.PyAsn1Error: TagSet(Tag(tagClass=128, tagFormat=0, tagId=40)) not in asn1Spec
   >>> decoder.decode(substrate, asn1Spec=i)
   (Integer(12345), b'')
   >>>

Notice in the example above, that an attempt to run decoder without
passing pyasn1 specification object fails because recovered tag does
not belong to any of the built-in types.

Another important feature of guided decoder operation is the use of
values constraints possibly present in pyasn1 specification object.
To explain this, we will decode a random integer object into generic Integer
and the constrained one.

.. code-block:: pycon

   >>> from pyasn1.type import univ, constraint
   >>> from pyasn1.codec.ber import encoder, decoder
   >>> class DialDigit(univ.Integer):
   ...   subtypeSpec = constraint.ValueRangeConstraint(0,9)
   >>> substrate = encoder.encode(univ.Integer(13))
   >>> decoder.decode(substrate)
   (Integer(13), b'')
   >>> decoder.decode(substrate, asn1Spec=DialDigit())
   Traceback (most recent call last):
   ...
   ValueConstraintError:
     ValueRangeConstraint(0, 9) failed at: 13
   >>> 

Similarily to encoders, to use CER or DER decoders application has to
explicitly import and call them - all APIs are compatible.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> from pyasn1.codec.ber import encoder as ber_encoder
   >>> substrate = ber_encoder.encode(univ.OctetString('http://pyasn1.sf.net'))
   >>>
   >>> from pyasn1.codec.ber import decoder as ber_decoder
   >>> from pyasn1.codec.cer import decoder as cer_decoder
   >>> from pyasn1.codec.der import decoder as der_decoder
   >>> 
   >>> ber_decoder.decode(substrate)
   (OctetString(b'http://pyasn1.sf.net'), b'')
   >>> cer_decoder.decode(substrate)
   (OctetString(b'http://pyasn1.sf.net'), b'')
   >>> der_decoder.decode(substrate)
   (OctetString(b'http://pyasn1.sf.net'), b'')
   >>> 

Advanced topics
---------------

Certain, non-trivial, ASN.1 data structures may require special
treatment, especially when running deserialization. 

Decoding untagged types
+++++++++++++++++++++++

It has already been mentioned, that ASN.1 has two "special case"
types: CHOICE and ANY. They are different from other types in part of
tagging - unless these two are additionally tagged, neither of them
will have their own tag. Therefore these types become invisible in
substrate and can not be recovered without passing pyasn1
specification object to decoder.

To explain the issue, we will first prepare a Choice object to deal with:

.. code-block:: pycon

   >>> from pyasn1.type import univ, namedtype
   >>> class CodeOrMessage(univ.Choice):
   ...   componentType = namedtype.NamedTypes(
   ...     namedtype.NamedType('code', univ.Integer()),
   ...     namedtype.NamedType('message', univ.OctetString())
   ...   )
   >>>
   >>> codeOrMessage = CodeOrMessage()
   >>> codeOrMessage['message'] = 'my string value'
   >>> print(codeOrMessage.prettyPrint())
   CodeOrMessage:
    message=b'my string value'
   >>>

Let's now encode this Choice object and then decode its substrate
with and without pyasn1 specification object:

.. code-block:: pycon

   >>> from pyasn1.codec.ber import encoder, decoder
   >>> substrate = encoder.encode(codeOrMessage)
   >>> substrate
   b'\x04\x0fmy string value'
   >>> encoder.encode(univ.OctetString('my string value'))
   b'\x04\x0fmy string value'
   >>>
   >>> decoder.decode(substrate)
   (OctetString(b'my string value'), b'')
   >>> codeOrMessage, substrate = decoder.decode(substrate,
   asn1Spec=CodeOrMessage())
   >>> print(codeOrMessage.prettyPrint())
   CodeOrMessage:
    message=b'my string value'
   >>>

First thing to notice in the listing above is that the substrate
produced for our Choice value object is equivalent to the substrate
for an OctetString object initialized to the same value. In other
words, any information about the Choice component is absent in
encoding.

Sure enough, that kind of substrate will decode into an OctetString
object, unless original Choice type object is passed to decoder to
guide the decoding process.

Similarily untagged ANY type behaves differently on decoding phase -
when decoder bumps into an Any object in pyasn1 specification, it
stops decoding and puts all the substrate into a new Any value object
in form of an octet string. Concerned application could then re-run
decoder with an additional, more exact pyasn1 specification object to
recover the contents of Any object.

As it was mentioned elsewhere in this documentation, Any type allows
for incomplete or changing ASN.1 specification to be handled
gracefully by decoder and applications.

To illustrate the working of Any type, we'll have to make the stage by
encoding a pyasn1 object and then putting its substrate into an any
object.

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> from pyasn1.codec.ber import encoder, decoder
   >>> innerSubstrate = encoder.encode(univ.Integer(1234))
   >>> innerSubstrate
   b'\x02\x02\x04\xd2'
   >>> any = univ.Any(innerSubstrate)
   >>> any
   Any(b'\x02\x02\x04\xd2')
   >>> substrate = encoder.encode(any)
   >>> substrate
   b'\x02\x02\x04\xd2'
   >>>

As with Choice type encoding, there is no traces of Any type in
substrate.  Obviously, the substrate we are dealing with, will decode
into the inner [Integer] component, unless pyasn1 specification is
given to guide the decoder. Continuing previous code:

.. code-block:: pycon

   >>> from pyasn1.type import univ
   >>> from pyasn1.codec.ber import encoder, decoder

   >>> decoder.decode(substrate)
   (Integer(1234), b'')
   >>> any, substrate = decoder.decode(substrate, asn1Spec=univ.Any())
   >>> any
   Any(b'\x02\x02\x04\xd2')
   >>> decoder.decode(str(any))
   (Integer(1234), b'')
   >>>

Both CHOICE and ANY types are widely used in practice. Reader is welcome to
take a look at 
`ASN.1 specifications of X.509 applications
<http://www.cs.auckland.ac.nz/~pgut001/pubs/x509guide.txt>`_
for more information.

Ignoring unknown types
++++++++++++++++++++++

When dealing with a loosely specified ASN.1 structure, the receiving
end may not be aware of some types present in the substrate. It may be
convenient then to turn decoder into a recovery mode. Whilst there,
decoder will not bail out when hit an unknown tag but rather treat it
as an Any type.

.. code-block:: pycon

   >>> from pyasn1.type import univ, tag
   >>> from pyasn1.codec.ber import encoder, decoder
   >>> taggedInt = univ.Integer(12345).subtype(
   ...   implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 40)
   ... )
   >>> substrate = encoder.encode(taggedInt)
   >>> decoder.decode(substrate)
   Traceback (most recent call last):
   ...
   pyasn1.error.PyAsn1Error: TagSet(Tag(tagClass=128, tagFormat=0, tagId=40))
   not in asn1Spec
   >>>
   >>> decoder.decode.defaultErrorState = decoder.stDumpRawValue
   >>> decoder.decode(substrate)
   (Any(b'\x9f(\x0209'), '')
   >>>

It's also possible to configure a custom decoder, to handle unknown
tags found in substrate. This can be done by means of
*defaultRawDecoder* attribute holding a reference to type decoder
object. Refer to the source for API details.

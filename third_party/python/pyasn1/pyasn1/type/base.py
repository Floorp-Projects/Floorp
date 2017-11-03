#
# This file is part of pyasn1 software.
#
# Copyright (c) 2005-2017, Ilya Etingof <etingof@gmail.com>
# License: http://pyasn1.sf.net/license.html
#
import sys
from pyasn1.type import constraint, tagmap, tag
from pyasn1.compat import calling
from pyasn1 import error

__all__ = ['Asn1Item', 'Asn1ItemBase', 'AbstractSimpleAsn1Item', 'AbstractConstructedAsn1Item']


class Asn1Item(object):
    @classmethod
    def getTypeId(cls, increment=1):
        try:
            Asn1Item._typeCounter += increment
        except AttributeError:
            Asn1Item._typeCounter = increment
        return Asn1Item._typeCounter


class Asn1ItemBase(Asn1Item):
    #: Set or return a :py:class:`~pyasn1.type.tag.TagSet` object representing
    #: ASN.1 tag(s) associated with |ASN.1| type.
    tagSet = tag.TagSet()

    #: Default :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
    #: object imposing constraints on initialization values.
    subtypeSpec = constraint.ConstraintsIntersection()

    # Disambiguation ASN.1 types identification
    typeId = None

    def __init__(self, **kwargs):
        readOnly = {
            'tagSet': self.tagSet,
            'subtypeSpec': self.subtypeSpec
        }

        readOnly.update(kwargs)

        self.__dict__.update(readOnly)

        self._readOnly = readOnly

    def __setattr__(self, name, value):
        if name[0] != '_' and name in self._readOnly:
            raise error.PyAsn1Error('read-only instance attribute "%s"' % name)

        self.__dict__[name] = value

    @property
    def readOnly(self):
        return self._readOnly

    @property
    def effectiveTagSet(self):
        """For |ASN.1| type is equivalent to *tagSet*
        """
        return self.tagSet  # used by untagged types

    @property
    def tagMap(self):
        """Return a :class:`~pyasn1.type.tagmap.TagMap` object mapping ASN.1 tags to ASN.1 objects within callee object.
        """
        return tagmap.TagMap({self.tagSet: self})

    def isSameTypeWith(self, other, matchTags=True, matchConstraints=True):
        """Examine |ASN.1| type for equality with other ASN.1 type.

        ASN.1 tags (:py:mod:`~pyasn1.type.tag`) and constraints
        (:py:mod:`~pyasn1.type.constraint`) are examined when carrying
        out ASN.1 types comparison.

        No Python inheritance relationship between PyASN1 objects is considered.

        Parameters
        ----------
        other: a pyasn1 type object
            Class instance representing ASN.1 type.

        Returns
        -------
        : :class:`bool`
            :class:`True` if *other* is |ASN.1| type,
            :class:`False` otherwise.
        """
        return (self is other or
                (not matchTags or self.tagSet == other.tagSet) and
                (not matchConstraints or self.subtypeSpec == other.subtypeSpec))

    def isSuperTypeOf(self, other, matchTags=True, matchConstraints=True):
        """Examine |ASN.1| type for subtype relationship with other ASN.1 type.
        
        ASN.1 tags (:py:mod:`~pyasn1.type.tag`) and constraints
        (:py:mod:`~pyasn1.type.constraint`) are examined when carrying
        out ASN.1 types comparison.

        No Python inheritance relationship between PyASN1 objects is considered.


        Parameters
        ----------
            other: a pyasn1 type object
                Class instance representing ASN.1 type. 

        Returns
        -------
            : :class:`bool`
                :class:`True` if *other* is a subtype of |ASN.1| type,
                :class:`False` otherwise.
        """
        return (not matchTags or
                (self.tagSet.isSuperTagSetOf(other.tagSet)) and
                 (not matchConstraints or self.subtypeSpec.isSuperTypeOf(other.subtypeSpec)))

    @staticmethod
    def isNoValue(*values):
        for value in values:
            if value is not None and value is not noValue:
                return False
        return True

    # backward compatibility

    def getTagSet(self):
        return self.tagSet

    def getEffectiveTagSet(self):
        return self.effectiveTagSet

    def getTagMap(self):
        return self.tagMap

    def getSubtypeSpec(self):
        return self.subtypeSpec

    def hasValue(self):
        return self.isValue


class NoValue(object):
    """Create a singleton instance of NoValue class.

    NoValue object can be used as an initializer on PyASN1 type class
    instantiation to represent ASN.1 type rather than ASN.1 data value.

    No operations other than type comparison can be performed on
    a PyASN1 type object.
    """
    skipMethods = ('__getattribute__', '__getattr__', '__setattr__', '__delattr__',
                   '__class__', '__init__', '__del__', '__new__', '__repr__', 
                   '__qualname__', '__objclass__', 'im_class', '__sizeof__')

    _instance = None

    def __new__(cls):
        if cls._instance is None:
            def getPlug(name):
                def plug(self, *args, **kw):
                    raise error.PyAsn1Error('Uninitialized ASN.1 value ("%s" attribute looked up)' % name)
                return plug

            op_names = [name
                        for typ in (str, int, list, dict)
                        for name in dir(typ)
                        if (name not in cls.skipMethods and
                            name.startswith('__') and
                            name.endswith('__') and
                            calling.callable(getattr(typ, name)))]

            for name in set(op_names):
                setattr(cls, name, getPlug(name))

            cls._instance = object.__new__(cls)

        return cls._instance

    def __getattr__(self, attr):
        if attr in self.skipMethods:
            raise AttributeError('attribute %s not present' % attr)
        raise error.PyAsn1Error('No value for "%s"' % attr)

    def __repr__(self):
        return '%s()' % self.__class__.__name__

noValue = NoValue()


# Base class for "simple" ASN.1 objects. These are immutable.
class AbstractSimpleAsn1Item(Asn1ItemBase):
    #: Default payload value
    defaultValue = noValue

    def __init__(self, value=noValue, **kwargs):
        Asn1ItemBase.__init__(self, **kwargs)
        if value is noValue or value is None:
            value = self.defaultValue
        else:
            value = self.prettyIn(value)
            try:
                self.subtypeSpec(value)

            except error.PyAsn1Error:
                exType, exValue, exTb = sys.exc_info()
                raise exType('%s at %s' % (exValue, self.__class__.__name__))

        self._value = value

    def __repr__(self):
        representation = []
        if self._value is not self.defaultValue:
            representation.append(self.prettyOut(self._value))
        if self.tagSet is not self.__class__.tagSet:
            representation.append('tagSet=%r' % (self.tagSet,))
        if self.subtypeSpec is not self.__class__.subtypeSpec:
            representation.append('subtypeSpec=%r' % (self.subtypeSpec,))
        return '%s(%s)' % (self.__class__.__name__, ', '.join(representation))

    def __str__(self):
        return str(self._value)

    def __eq__(self, other):
        return self is other and True or self._value == other

    def __ne__(self, other):
        return self._value != other

    def __lt__(self, other):
        return self._value < other

    def __le__(self, other):
        return self._value <= other

    def __gt__(self, other):
        return self._value > other

    def __ge__(self, other):
        return self._value >= other

    if sys.version_info[0] <= 2:
        def __nonzero__(self):
            return self._value and True or False
    else:
        def __bool__(self):
            return self._value and True or False

    def __hash__(self):
        return hash(self._value)

    @property
    def isValue(self):
        """Indicate if |ASN.1| object represents ASN.1 type or ASN.1 value.

        In other words, if *isValue* is `True`, then the ASN.1 object is
        initialized.

        Returns
        -------
        : :class:`bool`
            :class:`True` if object represents ASN.1 value and type,
            :class:`False` if object represents just ASN.1 type.

        Note
        ----
        There is an important distinction between PyASN1 type and value objects.
        The PyASN1 type objects can only participate in ASN.1 type
        operations (subtyping, comparison etc) and serve as a
        blueprint for serialization codecs to resolve ambiguous types.

        The PyASN1 value objects can additionally participate in most
        of built-in Python operations.
        """
        return self._value is not noValue

    def clone(self, value=noValue, **kwargs):
        """Create a copy of a |ASN.1| type or object.

          Any parameters to the *clone()* method will replace corresponding
          properties of the |ASN.1| object.

          Parameters
          ----------
          value: :class:`tuple`, :class:`str` or |ASN.1| object
              Initialization value to pass to new ASN.1 object instead of
              inheriting one from the caller.

          tagSet: :py:class:`~pyasn1.type.tag.TagSet`
              Object representing ASN.1 tag(s) to use in new object instead of inheriting from the caller

          subtypeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
              Object representing ASN.1 subtype constraint(s) to use in new object instead of inheriting from the caller

          Returns
          -------
          :
              new instance of |ASN.1| type/value
        """
        if value is noValue or value is None:
            if not kwargs:
                return self

            value = self._value

        initilaizers = self.readOnly.copy()
        initilaizers.update(kwargs)

        return self.__class__(value, **initilaizers)

    def subtype(self, value=noValue, **kwargs):
        """Create a copy of a |ASN.1| type or object.

         Any parameters to the *subtype()* method will be added to the corresponding
         properties of the |ASN.1| object.

         Parameters
         ----------
         value: :class:`tuple`, :class:`str` or |ASN.1| object
             Initialization value to pass to new ASN.1 object instead of
             inheriting one from the caller.

         implicitTag: :py:class:`~pyasn1.type.tag.Tag`
             Implicitly apply given ASN.1 tag object to caller's
             :py:class:`~pyasn1.type.tag.TagSet`, then use the result as
             new object's ASN.1 tag(s).

         explicitTag: :py:class:`~pyasn1.type.tag.Tag`
             Explicitly apply given ASN.1 tag object to caller's
             :py:class:`~pyasn1.type.tag.TagSet`, then use the result as
             new object's ASN.1 tag(s).

         subtypeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
             Add ASN.1 constraints object to one of the caller, then
             use the result as new object's ASN.1 constraints.

         Returns
         -------
         :
             new instance of |ASN.1| type/value
        """
        if value is noValue or value is None:
            if not kwargs:
                return self

            value = self._value

        initializers = self.readOnly.copy()

        implicitTag = kwargs.pop('implicitTag', None)
        if implicitTag is not None:
            initializers['tagSet'] = self.tagSet.tagImplicitly(implicitTag)

        explicitTag = kwargs.pop('explicitTag', None)
        if explicitTag is not None:
            initializers['tagSet'] = self.tagSet.tagExplicitly(explicitTag)

        for arg, option in kwargs.items():
            initializers[arg] += option

        return self.__class__(value, **initializers)

    def prettyIn(self, value):
        return value

    def prettyOut(self, value):
        return str(value)

    def prettyPrint(self, scope=0):
        """Provide human-friendly printable object representation.

        Returns
        -------
        : :class:`str`
            human-friendly type and/or value representation.
        """
        if self.isValue:
            return self.prettyOut(self._value)
        else:
            return '<no value>'

    # XXX Compatibility stub
    def prettyPrinter(self, scope=0):
        return self.prettyPrint(scope)

    # noinspection PyUnusedLocal
    def prettyPrintType(self, scope=0):
        return '%s -> %s' % (self.tagSet, self.__class__.__name__)

#
# Constructed types:
# * There are five of them: Sequence, SequenceOf/SetOf, Set and Choice
# * ASN1 types and values are represened by Python class instances
# * Value initialization is made for defaulted components only
# * Primary method of component addressing is by-position. Data model for base
#   type is Python sequence. Additional type-specific addressing methods
#   may be implemented for particular types.
# * SequenceOf and SetOf types do not implement any additional methods
# * Sequence, Set and Choice types also implement by-identifier addressing
# * Sequence, Set and Choice types also implement by-asn1-type (tag) addressing
# * Sequence and Set types may include optional and defaulted
#   components
# * Constructed types hold a reference to component types used for value
#   verification and ordering.
# * Component type is a scalar type for SequenceOf/SetOf types and a list
#   of types for Sequence/Set/Choice.
#

def setupComponent():
    """Returns a sentinel value.

     Indicates to a constructed type to set up its inner component so that it
     can be referred to. This is useful in situation when you want to populate
     descendants of a constructed type what requires being able to refer to
     their parent types along the way.

     Example
     -------

     >>> constructed['record'] = setupComponent()
     >>> constructed['record']['scalar'] = 42
    """
    return noValue


class AbstractConstructedAsn1Item(Asn1ItemBase):

    #: If `True`, requires exact component type matching,
    #: otherwise subtype relation is only enforced
    strictConstraints = False

    componentType = None
    sizeSpec = None

    def __init__(self, **kwargs):
        readOnly = {
            'componentType': self.componentType,
            'sizeSpec': self.sizeSpec
        }
        readOnly.update(kwargs)

        Asn1ItemBase.__init__(self, **readOnly)

        self._componentValues = []

    def __repr__(self):
        representation = []
        if self.componentType is not self.__class__.componentType:
            representation.append('componentType=%r' % (self.componentType,))
        if self.tagSet is not self.__class__.tagSet:
            representation.append('tagSet=%r' % (self.tagSet,))
        if self.subtypeSpec is not self.__class__.subtypeSpec:
            representation.append('subtypeSpec=%r' % (self.subtypeSpec,))
        representation = '%s(%s)' % (self.__class__.__name__, ', '.join(representation))
        if self._componentValues:
            for idx, component in enumerate(self._componentValues):
                if component is None or component is noValue:
                    continue
                representation += '.setComponentByPosition(%d, %s)' % (idx, repr(component))
        return representation

    def __eq__(self, other):
        return self is other and True or self._componentValues == other

    def __ne__(self, other):
        return self._componentValues != other

    def __lt__(self, other):
        return self._componentValues < other

    def __le__(self, other):
        return self._componentValues <= other

    def __gt__(self, other):
        return self._componentValues > other

    def __ge__(self, other):
        return self._componentValues >= other

    if sys.version_info[0] <= 2:
        def __nonzero__(self):
            return self._componentValues and True or False
    else:
        def __bool__(self):
            return self._componentValues and True or False

    def _cloneComponentValues(self, myClone, cloneValueFlag):
        pass

    def clone(self, **kwargs):
        """Create a copy of a |ASN.1| type or object.

        Any parameters to the *clone()* method will replace corresponding
        properties of the |ASN.1| object.

        Parameters
        ----------
        tagSet: :py:class:`~pyasn1.type.tag.TagSet`
            Object representing non-default ASN.1 tag(s)

        subtypeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
            Object representing non-default ASN.1 subtype constraint(s)

        sizeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
            Object representing non-default ASN.1 size constraint(s)

        Returns
        -------
        :
            new instance of |ASN.1| type/value

        """
        cloneValueFlag = kwargs.pop('cloneValueFlag', False)

        initilaizers = self.readOnly.copy()
        initilaizers.update(kwargs)

        clone = self.__class__(**initilaizers)

        if cloneValueFlag:
            self._cloneComponentValues(clone, cloneValueFlag)

        return clone

    def subtype(self, **kwargs):
        """Create a copy of a |ASN.1| type or object.

        Any parameters to the *subtype()* method will be added to the corresponding
        properties of the |ASN.1| object.

        Parameters
        ----------
        tagSet: :py:class:`~pyasn1.type.tag.TagSet`
            Object representing non-default ASN.1 tag(s)

        subtypeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
            Object representing non-default ASN.1 subtype constraint(s)

        sizeSpec: :py:class:`~pyasn1.type.constraint.ConstraintsIntersection`
            Object representing non-default ASN.1 size constraint(s)

        Returns
        -------
        :
            new instance of |ASN.1| type/value

        """

        initializers = self.readOnly.copy()

        cloneValueFlag = kwargs.pop('cloneValueFlag', False)

        implicitTag = kwargs.pop('implicitTag', None)
        if implicitTag is not None:
            initializers['tagSet'] = self.tagSet.tagImplicitly(implicitTag)

        explicitTag = kwargs.pop('explicitTag', None)
        if explicitTag is not None:
            initializers['tagSet'] = self.tagSet.tagExplicitly(explicitTag)

        for arg, option in kwargs.items():
            initializers[arg] += option

        clone = self.__class__(**initializers)

        if cloneValueFlag:
            self._cloneComponentValues(clone, cloneValueFlag)

        return clone

    def verifySizeSpec(self):
        self.sizeSpec(self)

    def getComponentByPosition(self, idx):
        raise error.PyAsn1Error('Method not implemented')

    def setComponentByPosition(self, idx, value, verifyConstraints=True):
        raise error.PyAsn1Error('Method not implemented')

    def setComponents(self, *args, **kwargs):
        for idx, value in enumerate(args):
            self[idx] = value
        for k in kwargs:
            self[k] = kwargs[k]
        return self

    def __len__(self):
        return len(self._componentValues)

    def clear(self):
        self._componentValues = []

    # backward compatibility

    def setDefaultComponents(self):
        pass

    def getComponentType(self):
        return self.componentType

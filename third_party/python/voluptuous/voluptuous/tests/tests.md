Error reporting should be accurate:

    >>> from voluptuous import *
    >>> schema = Schema(['one', {'two': 'three', 'four': ['five'],
    ...                          'six': {'seven': 'eight'}}])
    >>> schema(['one'])
    ['one']
    >>> schema([{'two': 'three'}])
    [{'two': 'three'}]

It should show the exact index and container type, in this case a list
value:

    >>> try:
    ...   schema(['one', 'two'])
    ...   raise AssertionError('MultipleInvalid not raised')
    ... except MultipleInvalid as e:
    ...   exc = e
    >>> str(exc) == 'expected a dictionary @ data[1]'
    True

It should also be accurate for nested values:

    >>> try:
    ...   schema([{'two': 'nine'}])
    ...   raise AssertionError('MultipleInvalid not raised')
    ... except MultipleInvalid as e:
    ...   exc = e
    >>> str(exc)
    "not a valid value for dictionary value @ data[0]['two']"

    >>> try:
    ...   schema([{'four': ['nine']}])
    ...   raise AssertionError('MultipleInvalid not raised')
    ... except MultipleInvalid as e:
    ...   exc = e
    >>> str(exc)
    "not a valid value @ data[0]['four'][0]"

    >>> try:
    ...   schema([{'six': {'seven': 'nine'}}])
    ...   raise AssertionError('MultipleInvalid not raised')
    ... except MultipleInvalid as e:
    ...   exc = e
    >>> str(exc)
    "not a valid value for dictionary value @ data[0]['six']['seven']"

Errors should be reported depth-first:

    >>> validate = Schema({'one': {'two': 'three', 'four': 'five'}})
    >>> try:
    ...   validate({'one': {'four': 'six'}})
    ... except Invalid as e:
    ...   print(e)
    ...   print(e.path)
    not a valid value for dictionary value @ data['one']['four']
    ['one', 'four']

Voluptuous supports validation when extra fields are present in the
data:

    >>> schema = Schema({'one': 1, Extra: object})
    >>> schema({'two': 'two', 'one': 1}) == {'two': 'two', 'one': 1}
    True
    >>> schema = Schema({'one': 1})
    >>> try:
    ...   schema({'two': 2})
    ...   raise AssertionError('MultipleInvalid not raised')
    ... except MultipleInvalid as e:
    ...   exc = e
    >>> str(exc)
    "extra keys not allowed @ data['two']"

dict, list, and tuple should be available as type validators:

    >>> Schema(dict)({'a': 1, 'b': 2}) == {'a': 1, 'b': 2}
    True
    >>> Schema(list)([1,2,3])
    [1, 2, 3]
    >>> Schema(tuple)((1,2,3))
    (1, 2, 3)

Validation should return instances of the right types when the types are
subclasses of dict or list:

    >>> class Dict(dict):
    ...   pass
    >>>
    >>> d = Schema(dict)(Dict(a=1, b=2))
    >>> d == {'a': 1, 'b': 2}
    True
    >>> type(d) is Dict
    True
    >>> class List(list):
    ...   pass
    >>>
    >>> l = Schema(list)(List([1,2,3]))
    >>> l
    [1, 2, 3]
    >>> type(l) is List
    True

Multiple errors are reported:

    >>> schema = Schema({'one': 1, 'two': 2})
    >>> try:
    ...   schema({'one': 2, 'two': 3, 'three': 4})
    ... except MultipleInvalid as e:
    ...   errors = sorted(e.errors, key=lambda k: str(k))
    ...   print([str(i) for i in errors])  # doctest: +NORMALIZE_WHITESPACE
    ["extra keys not allowed @ data['three']",
     "not a valid value for dictionary value @ data['one']",
     "not a valid value for dictionary value @ data['two']"]
    >>> schema = Schema([[1], [2], [3]])
    >>> try:
    ...   schema([1, 2, 3])
    ... except MultipleInvalid as e:
    ...   print([str(i) for i in e.errors])  # doctest: +NORMALIZE_WHITESPACE
    ['expected a list @ data[0]',
     'expected a list @ data[1]',
     'expected a list @ data[2]']

Required fields in dictionary which are invalid should not have required :

    >>> from voluptuous import *
    >>> schema = Schema({'one': {'two': 3}}, required=True)
    >>> try:
    ...   schema({'one': {'two': 2}})
    ... except MultipleInvalid as e:
    ...   errors = e.errors
    >>> 'required' in ' '.join([x.msg for x in errors])
    False

Multiple errors for nested fields in dicts and objects:

> \>\>\> from collections import namedtuple \>\>\> validate = Schema({
> ... 'anobject': Object({ ... 'strfield': str, ... 'intfield': int ...
> }) ... }) \>\>\> try: ... SomeObj = namedtuple('SomeObj', ('strfield',
> 'intfield')) ... validate({'anobject': SomeObj(strfield=123,
> intfield='one')}) ... except MultipleInvalid as e: ...
> print(sorted(str(i) for i in e.errors)) \# doctest:
> +NORMALIZE\_WHITESPACE ["expected int for object value @
> data['anobject']['intfield']", "expected str for object value @
> data['anobject']['strfield']"]

Custom classes validate as schemas:

    >>> class Thing(object):
    ...     pass
    >>> schema = Schema(Thing)
    >>> t = schema(Thing())
    >>> type(t) is Thing
    True

Classes with custom metaclasses should validate as schemas:

    >>> class MyMeta(type):
    ...     pass
    >>> class Thing(object):
    ...     __metaclass__ = MyMeta
    >>> schema = Schema(Thing)
    >>> t = schema(Thing())
    >>> type(t) is Thing
    True

Schemas built with All() should give the same error as the original
validator (Issue \#26):

    >>> schema = Schema({
    ...   Required('items'): All([{
    ...     Required('foo'): str
    ...   }])
    ... })

    >>> try:
    ...   schema({'items': [{}]})
    ...   raise AssertionError('MultipleInvalid not raised')
    ... except MultipleInvalid as e:
    ...   exc = e
    >>> str(exc)
    "required key not provided @ data['items'][0]['foo']"

Validator should return same instance of the same type for object:

    >>> class Structure(object):
    ...     def __init__(self, q=None):
    ...         self.q = q
    ...     def __repr__(self):
    ...         return '{0.__name__}(q={1.q!r})'.format(type(self), self)
    ...
    >>> schema = Schema(Object({'q': 'one'}, cls=Structure))
    >>> type(schema(Structure(q='one'))) is Structure
    True

Object validator should treat cls argument as optional. In this case it
shouldn't check object type:

    >>> from collections import namedtuple
    >>> NamedTuple = namedtuple('NamedTuple', ('q',))
    >>> schema = Schema(Object({'q': 'one'}))
    >>> named = NamedTuple(q='one')
    >>> schema(named) == named
    True
    >>> schema(named)
    NamedTuple(q='one')

If cls argument passed to object validator we should check object type:

    >>> schema = Schema(Object({'q': 'one'}, cls=Structure))
    >>> schema(NamedTuple(q='one'))  # doctest: +IGNORE_EXCEPTION_DETAIL
    Traceback (most recent call last):
    ...
    MultipleInvalid: expected a <class 'Structure'>
    >>> schema = Schema(Object({'q': 'one'}, cls=NamedTuple))
    >>> schema(NamedTuple(q='one'))
    NamedTuple(q='one')

Ensure that objects with \_\_slots\_\_ supported properly:

    >>> class SlotsStructure(Structure):
    ...     __slots__ = ['q']
    ...
    >>> schema = Schema(Object({'q': 'one'}))
    >>> schema(SlotsStructure(q='one'))
    SlotsStructure(q='one')
    >>> class DictStructure(object):
    ...     __slots__ = ['q', '__dict__']
    ...     def __init__(self, q=None, page=None):
    ...         self.q = q
    ...         self.page = page
    ...     def __repr__(self):
    ...         return '{0.__name__}(q={1.q!r}, page={1.page!r})'.format(type(self), self)
    ...
    >>> structure = DictStructure(q='one')
    >>> structure.page = 1
    >>> try:
    ...   schema(structure)
    ...   raise AssertionError('MultipleInvalid not raised')
    ... except MultipleInvalid as e:
    ...   exc = e
    >>> str(exc)
    "extra keys not allowed @ data['page']"

    >>> schema = Schema(Object({'q': 'one', Extra: object}))
    >>> schema(structure)
    DictStructure(q='one', page=1)

Ensure that objects can be used with other validators:

    >>> schema = Schema({'meta': Object({'q': 'one'})})
    >>> schema({'meta': Structure(q='one')})
    {'meta': Structure(q='one')}

Ensure that subclasses of Invalid of are raised as is.

    >>> class SpecialInvalid(Invalid):
    ...   pass
    ...
    >>> def custom_validator(value):
    ...   raise SpecialInvalid('boom')
    ...
    >>> schema = Schema({'thing': custom_validator})
    >>> try:
    ...   schema({'thing': 'not an int'})
    ... except MultipleInvalid as e:
    ...   exc = e
    >>> exc.errors[0].__class__.__name__
    'SpecialInvalid'

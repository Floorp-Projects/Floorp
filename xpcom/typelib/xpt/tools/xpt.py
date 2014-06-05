#!/usr/bin/env python
# Copyright 2010,2011 Mozilla Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE MOZILLA FOUNDATION ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE MOZILLA FOUNDATION OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation
# are those of the authors and should not be interpreted as representing
# official policies, either expressed or implied, of the Mozilla
# Foundation.

"""
A module for working with XPCOM Type Libraries.

The XPCOM Type Library File Format is described at:
http://www.mozilla.org/scriptable/typelib_file.html . It is used
to provide type information for calling methods on XPCOM objects
from scripting languages such as JavaScript.

This module provides a set of classes representing the parts of
a typelib in a high-level manner, as well as methods for reading
and writing them from files.

The usable public interfaces are currently:
Typelib.read(input_file) - read a typelib from a file on disk or file-like
                           object, return a Typelib object.

xpt_dump(filename)     - read a typelib from a file on disk, dump
                         the contents to stdout in a human-readable
                         format.

Typelib()              - construct a new Typelib object
Interface()            - construct a new Interface object
Method()               - construct a new object representing a method
                         defined on an Interface
Constant()             - construct a new object representing a constant
                         defined on an Interface
Param()                - construct a new object representing a parameter
                         to a method
SimpleType()           - construct a new object representing a simple
                         data type
InterfaceType()        - construct a new object representing a type that
                         is an IDL-defined interface

"""

from __future__ import with_statement
import os, sys
import struct
import operator
import itertools

# header magic
XPT_MAGIC = "XPCOM\nTypeLib\r\n\x1a"
TYPELIB_VERSION = (1, 2)

class FileFormatError(Exception):
    pass

class DataError(Exception):
    pass

# Magic for creating enums
def M_add_class_attribs(attribs):
    def foo(name, bases, dict_):
        for v, k in attribs:
            dict_[k] = v
        return type(name, bases, dict_)
    return foo

def enum(*names):
    class Foo(object):
        __metaclass__ = M_add_class_attribs(enumerate(names))
        def __setattr__(self, name, value):  # this makes it read-only
            raise NotImplementedError
    return Foo()

# Descriptor types as described in the spec
class Type(object):
    """
    Data type of a method parameter or return value. Do not instantiate
    this class directly. Rather, use one of its subclasses.
    
    """
    _prefixdescriptor = struct.Struct(">B")
    Tags = enum(
        # The first 18 entries are SimpleTypeDescriptor
        'int8',
        'int16',
        'int32',
        'int64',
        'uint8',
        'uint16',
        'uint32',
        'uint64',
        'float',
        'double',
        'boolean',
        'char',
        'wchar_t',
        'void',
        # the following four values are only valid as pointers
        'nsIID',
        'DOMString',
        'char_ptr',
        'wchar_t_ptr',
        # InterfaceTypeDescriptor
        'Interface',
        # InterfaceIsTypeDescriptor
        'InterfaceIs',
        # ArrayTypeDescriptor
        'Array',
        # StringWithSizeTypeDescriptor
        'StringWithSize',
        # WideStringWithSizeTypeDescriptor
        'WideStringWithSize',
        #XXX: These are also SimpleTypes (but not in the spec)
        # http://hg.mozilla.org/mozilla-central/annotate/0e0e2516f04e/xpcom/typelib/xpt/tools/xpt_dump.c#l69
        'UTF8String',
        'CString',
        'AString',
        'jsval',
        )

    def __init__(self, pointer=False, reference=False):
        self.pointer = pointer
        self.reference = reference
        if reference and not pointer:
            raise Exception("If reference is True pointer must be True too")

    @staticmethod
    def decodeflags(byte):
        """
        Given |byte|, an unsigned uint8 containing flag bits,
        decode the flag bits as described in
        http://www.mozilla.org/scriptable/typelib_file.html#TypeDescriptor
        and return a dict of flagname: (True|False) suitable
        for passing to Type.__init__ as **kwargs.
        
        """
        return {'pointer': bool(byte & 0x80),
                'reference': bool(byte & 0x20),
                }

    def encodeflags(self):
        """
        Encode the flag bits of this Type object. Returns a byte.

        """
        flags = 0
        if self.pointer:
            flags |= 0x80
        if self.reference:
            flags |= 0x20
        return flags

    @staticmethod
    def read(typelib, map, data_pool, offset):
        """
        Read a TypeDescriptor at |offset| from the mmaped file |map| with
        data pool offset |data_pool|. Returns (Type, next offset),
        where |next offset| is an offset suitable for reading the data
        following this TypeDescriptor.
        
        """
        start = data_pool + offset - 1
        (data,) = Type._prefixdescriptor.unpack_from(map, start)
        # first three bits are the flags
        flags = data & 0xE0
        flags = Type.decodeflags(flags)
        # last five bits is the tag
        tag = data & 0x1F
        offset += Type._prefixdescriptor.size
        t = None
        if tag <= Type.Tags.wchar_t_ptr or tag >= Type.Tags.UTF8String:
            t = SimpleType.get(data, tag, flags)
        elif tag == Type.Tags.Interface:
            t, offset = InterfaceType.read(typelib, map, data_pool, offset, flags)
        elif tag == Type.Tags.InterfaceIs:
            t, offset = InterfaceIsType.read(typelib, map, data_pool, offset, flags)
        elif tag == Type.Tags.Array:
            t, offset = ArrayType.read(typelib, map, data_pool, offset, flags)
        elif tag == Type.Tags.StringWithSize:
            t, offset = StringWithSizeType.read(typelib, map, data_pool, offset, flags)
        elif tag == Type.Tags.WideStringWithSize:
            t, offset = WideStringWithSizeType.read(typelib, map, data_pool, offset, flags)
        return t, offset

    def write(self, typelib, file):
        """
        Write a TypeDescriptor to |file|, which is assumed
        to be seeked to the proper position. For types other than
        SimpleType, this is not sufficient for writing the TypeDescriptor,
        and the subclass method must be called.

        """
        file.write(Type._prefixdescriptor.pack(self.encodeflags() | self.tag))

class SimpleType(Type):
    """
    A simple data type. (SimpleTypeDescriptor from the typelib specification.)

    """
    _cache = {}

    def __init__(self, tag, **kwargs):
        Type.__init__(self, **kwargs)
        self.tag = tag

    @staticmethod
    def get(data, tag, flags):
        """
        Get a SimpleType object representing |data| (a TypeDescriptorPrefix).
        May return an already-created object. If no cached object is found,
        construct one with |tag| and |flags|.
        
        """
        if data not in SimpleType._cache:
            SimpleType._cache[data] = SimpleType(tag, **flags)
        return SimpleType._cache[data]

    def __str__(self):
        s = "unknown"
        if self.tag == Type.Tags.char_ptr and self.pointer:
            return "string"
        if self.tag == Type.Tags.wchar_t_ptr and self.pointer:
            return "wstring"
        for t in dir(Type.Tags):
            if self.tag == getattr(Type.Tags, t):
                s = t
                break

        if self.pointer:
            if self.reference:
                s += " &"
            else:
                s += " *"
        return s

class InterfaceType(Type):
    """
    A type representing a pointer to an IDL-defined interface.
    (InterfaceTypeDescriptor from the typelib specification.)

    """
    _descriptor = struct.Struct(">H")

    def __init__(self, iface, pointer=True, **kwargs):
        if not pointer:
            raise DataError, "InterfaceType is not valid with pointer=False"
        Type.__init__(self, pointer=pointer, **kwargs)
        self.iface = iface
        self.tag = Type.Tags.Interface

    @staticmethod
    def read(typelib, map, data_pool, offset, flags):
        """
        Read an InterfaceTypeDescriptor at |offset| from the mmaped
        file |map| with data pool offset |data_pool|.
        Returns (InterfaceType, next offset),
        where |next offset| is an offset suitable for reading the data
        following this InterfaceTypeDescriptor.
        
        """
        if not flags['pointer']:
            return None, offset
        start = data_pool + offset - 1
        (iface_index,) = InterfaceType._descriptor.unpack_from(map, start)
        offset += InterfaceType._descriptor.size
        iface = None
        # interface indices are 1-based
        if iface_index > 0 and iface_index <= len(typelib.interfaces):
            iface = typelib.interfaces[iface_index - 1]
        return InterfaceType(iface, **flags), offset

    def write(self, typelib, file):
        """
        Write an InterfaceTypeDescriptor to |file|, which is assumed
        to be seeked to the proper position.

        """
        Type.write(self, typelib, file)
        # write out the interface index (1-based)
        file.write(InterfaceType._descriptor.pack(typelib.interfaces.index(self.iface) + 1))

    def __str__(self):
        if self.iface:
            return self.iface.name
        return "unknown interface"

class InterfaceIsType(Type):
    """
    A type representing an interface described by one of the other
    arguments to the method. (InterfaceIsTypeDescriptor from the
    typelib specification.)
    
    """
    _descriptor = struct.Struct(">B")
    _cache = {}

    def __init__(self, param_index, pointer=True, **kwargs):
        if not pointer:
            raise DataError, "InterfaceIsType is not valid with pointer=False"
        Type.__init__(self, pointer=pointer, **kwargs)
        self.param_index = param_index
        self.tag = Type.Tags.InterfaceIs

    @staticmethod
    def read(typelib, map, data_pool, offset, flags):
        """
        Read an InterfaceIsTypeDescriptor at |offset| from the mmaped
        file |map| with data pool offset |data_pool|.
        Returns (InterfaceIsType, next offset),
        where |next offset| is an offset suitable for reading the data
        following this InterfaceIsTypeDescriptor.
        May return a cached value.
        
        """
        if not flags['pointer']:
            return None, offset
        start = data_pool + offset - 1
        (param_index,) = InterfaceIsType._descriptor.unpack_from(map, start)
        offset += InterfaceIsType._descriptor.size
        if param_index not in InterfaceIsType._cache:
            InterfaceIsType._cache[param_index] = InterfaceIsType(param_index, **flags)
        return InterfaceIsType._cache[param_index], offset

    def write(self, typelib, file):
        """
        Write an InterfaceIsTypeDescriptor to |file|, which is assumed
        to be seeked to the proper position.

        """
        Type.write(self, typelib, file)
        file.write(InterfaceIsType._descriptor.pack(self.param_index))

    def __str__(self):
        return "InterfaceIs *"

class ArrayType(Type):
    """
    A type representing an Array of elements of another type, whose
    size and length are passed as separate parameters to a method.
    (ArrayTypeDescriptor from the typelib specification.)
    
    """
    _descriptor = struct.Struct(">BB")

    def __init__(self, element_type, size_is_arg_num, length_is_arg_num,
                 pointer=True, **kwargs):
        if not pointer:
            raise DataError, "ArrayType is not valid with pointer=False"
        Type.__init__(self, pointer=pointer, **kwargs)
        self.element_type = element_type
        self.size_is_arg_num = size_is_arg_num
        self.length_is_arg_num = length_is_arg_num
        self.tag = Type.Tags.Array

    @staticmethod
    def read(typelib, map, data_pool, offset, flags):
        """
        Read an ArrayTypeDescriptor at |offset| from the mmaped
        file |map| with data pool offset |data_pool|.
        Returns (ArrayType, next offset),
        where |next offset| is an offset suitable for reading the data
        following this ArrayTypeDescriptor.
        """
        if not flags['pointer']:
            return None, offset
        start = data_pool + offset - 1
        (size_is_arg_num, length_is_arg_num) = ArrayType._descriptor.unpack_from(map, start)
        offset += ArrayType._descriptor.size
        t, offset = Type.read(typelib, map, data_pool, offset)
        return ArrayType(t, size_is_arg_num, length_is_arg_num, **flags), offset

    def write(self, typelib, file):
        """
        Write an ArrayTypeDescriptor to |file|, which is assumed
        to be seeked to the proper position.

        """
        Type.write(self, typelib, file)
        file.write(ArrayType._descriptor.pack(self.size_is_arg_num,
                                              self.length_is_arg_num))
        self.element_type.write(typelib, file)

    def __str__(self):
        return "%s []" % str(self.element_type)

class StringWithSizeType(Type):
    """
    A type representing a UTF-8 encoded string whose size and length
    are passed as separate arguments to a method. (StringWithSizeTypeDescriptor
    from the typelib specification.)

    """
    _descriptor = struct.Struct(">BB")

    def __init__(self, size_is_arg_num, length_is_arg_num,
                 pointer=True, **kwargs):
        if not pointer:
            raise DataError, "StringWithSizeType is not valid with pointer=False"
        Type.__init__(self, pointer=pointer, **kwargs)
        self.size_is_arg_num = size_is_arg_num
        self.length_is_arg_num = length_is_arg_num
        self.tag = Type.Tags.StringWithSize

    @staticmethod
    def read(typelib, map, data_pool, offset, flags):
        """
        Read an StringWithSizeTypeDescriptor at |offset| from the mmaped
        file |map| with data pool offset |data_pool|.
        Returns (StringWithSizeType, next offset),
        where |next offset| is an offset suitable for reading the data
        following this StringWithSizeTypeDescriptor.
        """
        if not flags['pointer']:
            return None, offset
        start = data_pool + offset - 1
        (size_is_arg_num, length_is_arg_num) = StringWithSizeType._descriptor.unpack_from(map, start)
        offset += StringWithSizeType._descriptor.size
        return StringWithSizeType(size_is_arg_num, length_is_arg_num, **flags), offset

    def write(self, typelib, file):
        """
        Write a StringWithSizeTypeDescriptor to |file|, which is assumed
        to be seeked to the proper position.

        """
        Type.write(self, typelib, file)
        file.write(StringWithSizeType._descriptor.pack(self.size_is_arg_num,
                                                       self.length_is_arg_num))

    def __str__(self):
        return "string_s"

class WideStringWithSizeType(Type):
    """
    A type representing a UTF-16 encoded string whose size and length
    are passed as separate arguments to a method.
    (WideStringWithSizeTypeDescriptor from the typelib specification.)

    """
    _descriptor = struct.Struct(">BB")

    def __init__(self, size_is_arg_num, length_is_arg_num,
                 pointer=True, **kwargs):
        if not pointer:
            raise DataError, "WideStringWithSizeType is not valid with pointer=False"
        Type.__init__(self, pointer=pointer, **kwargs)
        self.size_is_arg_num = size_is_arg_num
        self.length_is_arg_num = length_is_arg_num
        self.tag = Type.Tags.WideStringWithSize

    @staticmethod
    def read(typelib, map, data_pool, offset, flags):
        """
        Read an WideStringWithSizeTypeDescriptor at |offset| from the mmaped
        file |map| with data pool offset |data_pool|.
        Returns (WideStringWithSizeType, next offset),
        where |next offset| is an offset suitable for reading the data
        following this WideStringWithSizeTypeDescriptor.
        """
        if not flags['pointer']:
            return None, offset
        start = data_pool + offset - 1
        (size_is_arg_num, length_is_arg_num) = WideStringWithSizeType._descriptor.unpack_from(map, start)
        offset += WideStringWithSizeType._descriptor.size
        return WideStringWithSizeType(size_is_arg_num, length_is_arg_num, **flags), offset

    def write(self, typelib, file):
        """
        Write a WideStringWithSizeTypeDescriptor to |file|, which is assumed
        to be seeked to the proper position.

        """
        Type.write(self, typelib, file)
        file.write(WideStringWithSizeType._descriptor.pack(self.size_is_arg_num,
                                                           self.length_is_arg_num))

    def __str__(self):
        return "wstring_s"

class Param(object):
    """
    A parameter to a method, or the return value of a method.
    (ParamDescriptor from the typelib specification.)

    """
    _descriptorstart = struct.Struct(">B")

    def __init__(self, type, in_=True, out=False, retval=False,
                 shared=False, dipper=False, optional=False):
        """
        Construct a Param object with the specified |type| and
        flags. Params default to "in".

        """

        self.type = type
        self.in_ = in_
        self.out = out
        self.retval = retval
        self.shared = shared
        self.dipper = dipper
        self.optional = optional

    @staticmethod
    def decodeflags(byte):
        """
        Given |byte|, an unsigned uint8 containing flag bits,
        decode the flag bits as described in
        http://www.mozilla.org/scriptable/typelib_file.html#ParamDescriptor
        and return a dict of flagname: (True|False) suitable
        for passing to Param.__init__ as **kwargs
        """
        return {'in_': bool(byte & 0x80),
                'out': bool(byte & 0x40),
                'retval': bool(byte & 0x20),
                'shared': bool(byte & 0x10),
                'dipper': bool(byte & 0x08),
                #XXX: Not in the spec, see:
                # http://hg.mozilla.org/mozilla-central/annotate/0e0e2516f04e/xpcom/typelib/xpt/public/xpt_struct.h#l456
                'optional': bool(byte & 0x04),
                }

    def encodeflags(self):
        """
        Encode the flags of this Param. Return a byte suitable for
        writing to a typelib file.

        """
        flags = 0
        if self.in_:
            flags |= 0x80
        if self.out:
            flags |= 0x40
        if self.retval:
            flags |= 0x20
        if self.shared:
            flags |= 0x10
        if self.dipper:
            flags |= 0x08
        if self.optional:
            flags |= 0x04
        return flags

    @staticmethod
    def read(typelib, map, data_pool, offset):
        """
        Read a ParamDescriptor at |offset| from the mmaped file |map| with
        data pool offset |data_pool|. Returns (Param, next offset),
        where |next offset| is an offset suitable for reading the data
        following this ParamDescriptor.
        """
        start = data_pool + offset - 1
        (flags,) = Param._descriptorstart.unpack_from(map, start)
        # only the first five bits are flags
        flags &= 0xFC
        flags = Param.decodeflags(flags)
        offset += Param._descriptorstart.size
        t, offset = Type.read(typelib, map, data_pool, offset)
        p = Param(t, **flags)
        return p, offset

    def write(self, typelib, file):
        """
        Write a ParamDescriptor to |file|, which is assumed to be seeked
        to the correct position.

        """
        file.write(Param._descriptorstart.pack(self.encodeflags()))
        self.type.write(typelib, file)

    def prefix(self):
        """
        Return a human-readable string representing the flags set
        on this Param.

        """
        s = ""
        if self.out:
            if self.in_:
                s = "inout "
            else:
                s = "out "
        else:
            s = "in "
        if self.dipper:
            s += "dipper "
        if self.retval:
            s += "retval "
        if self.shared:
            s += "shared "
        if self.optional:
            s += "optional "
        return s

    def __str__(self):
        return self.prefix() + str(self.type)

class Method(object):
    """
    A method of an interface, defining its associated parameters
    and return value.
    (MethodDescriptor from the typelib specification.)
    
    """
    _descriptorstart = struct.Struct(">BIB")

    def __init__(self, name, result,
                 params=[], getter=False, setter=False, notxpcom=False,
                 constructor=False, hidden=False, optargc=False,
                 implicit_jscontext=False):
        self.name = name
        self._name_offset = 0
        self.getter = getter
        self.setter = setter
        self.notxpcom = notxpcom
        self.constructor = constructor
        self.hidden = hidden
        self.optargc = optargc
        self.implicit_jscontext = implicit_jscontext
        self.params = list(params)
        if result and not isinstance(result, Param):
            raise Exception("result must be a Param!")
        self.result = result

    def read_params(self, typelib, map, data_pool, offset, num_args):
        """
        Read |num_args| ParamDescriptors representing this Method's arguments
        from the mmaped file |map| with data pool at the offset |data_pool|,
        starting at |offset| into self.params. Returns the offset
        suitable for reading the data following the ParamDescriptor array.
        
        """
        for i in range(num_args):
            p, offset = Param.read(typelib, map, data_pool, offset)
            self.params.append(p)
        return offset

    def read_result(self, typelib, map, data_pool, offset):
        """
        Read a ParamDescriptor representing this Method's return type
        from the mmaped file |map| with data pool at the offset |data_pool|,
        starting at |offset| into self.result. Returns the offset
        suitable for reading the data following the ParamDescriptor.
        
        """
        self.result, offset = Param.read(typelib, map, data_pool, offset)
        return offset

    @staticmethod
    def decodeflags(byte):
        """
        Given |byte|, an unsigned uint8 containing flag bits,
        decode the flag bits as described in
        http://www.mozilla.org/scriptable/typelib_file.html#MethodDescriptor
        and return a dict of flagname: (True|False) suitable
        for passing to Method.__init__ as **kwargs
        
        """
        return {'getter': bool(byte & 0x80),
                'setter': bool(byte & 0x40),
                'notxpcom': bool(byte & 0x20),
                'constructor': bool(byte & 0x10),
                'hidden': bool(byte & 0x08),
                # Not in the spec, see
                # http://hg.mozilla.org/mozilla-central/annotate/0e0e2516f04e/xpcom/typelib/xpt/public/xpt_struct.h#l489
                'optargc': bool(byte & 0x04),
                'implicit_jscontext': bool(byte & 0x02),
                }

    def encodeflags(self):
        """
        Encode the flags of this Method object, return a byte suitable
        for writing to a typelib file.

        """
        flags = 0
        if self.getter:
            flags |= 0x80
        if self.setter:
            flags |= 0x40
        if self.notxpcom:
            flags |= 0x20
        if self.constructor:
            flags |= 0x10
        if self.hidden:
            flags |= 0x08
        if self.optargc:
            flags |= 0x04
        if self.implicit_jscontext:
            flags |= 0x02
        return flags

    @staticmethod
    def read(typelib, map, data_pool, offset):
        """
        Read a MethodDescriptor at |offset| from the mmaped file |map| with
        data pool offset |data_pool|. Returns (Method, next offset),
        where |next offset| is an offset suitable for reading the data
        following this MethodDescriptor.
        
        """
        start = data_pool + offset - 1
        flags, name_offset, num_args = Method._descriptorstart.unpack_from(map, start)
        # only the first seven bits are flags
        flags &= 0xFE
        flags = Method.decodeflags(flags)
        name = Typelib.read_string(map, data_pool, name_offset)
        m = Method(name, None, **flags)
        offset += Method._descriptorstart.size
        offset = m.read_params(typelib, map, data_pool, offset, num_args)
        offset = m.read_result(typelib, map, data_pool, offset)
        return m, offset

    def write(self, typelib, file):
        """
        Write a MethodDescriptor to |file|, which is assumed to be
        seeked to the right position.

        """
        file.write(Method._descriptorstart.pack(self.encodeflags(),
                                                self._name_offset,
                                                len(self.params)))
        for p in self.params:
            p.write(typelib, file)
        self.result.write(typelib, file)

    def write_name(self, file, data_pool_offset):
        """
        Write this method's name to |file|.
        Assumes that |file| is currently seeked to an unused portion
        of the data pool.

        """
        if self.name:
            self._name_offset = file.tell() - data_pool_offset + 1
            file.write(self.name + "\x00")
        else:
            self._name_offset = 0

class Constant(object):
    """
    A constant value of a specific type defined on an interface.
    (ConstantDesciptor from the typelib specification.)

    """
    _descriptorstart = struct.Struct(">I")
    # Actual value is restricted to this set of types
    #XXX: the spec lies, the source allows a bunch more
    # http://hg.mozilla.org/mozilla-central/annotate/9c85f9aaec8c/xpcom/typelib/xpt/src/xpt_struct.c#l689
    typemap = {Type.Tags.int16: '>h',
               Type.Tags.uint16: '>H',
               Type.Tags.int32: '>i',
               Type.Tags.uint32: '>I'}

    def __init__(self, name, type, value):
        self.name = name
        self._name_offset = 0
        self.type = type
        self.value = value

    @staticmethod
    def read(typelib, map, data_pool, offset):
        """
        Read a ConstDescriptor at |offset| from the mmaped file |map| with
        data pool offset |data_pool|. Returns (Constant, next offset),
        where |next offset| is an offset suitable for reading the data
        following this ConstDescriptor.
        
        """
        start = data_pool + offset - 1
        (name_offset,) = Constant._descriptorstart.unpack_from(map, start)
        name = Typelib.read_string(map, data_pool, name_offset)
        offset += Constant._descriptorstart.size
        # Read TypeDescriptor
        t, offset = Type.read(typelib, map, data_pool, offset)
        c = None
        if isinstance(t, SimpleType) and t.tag in Constant.typemap:
            tt = Constant.typemap[t.tag]
            start = data_pool + offset - 1
            (val,) = struct.unpack_from(tt, map, start)
            offset += struct.calcsize(tt)
            c = Constant(name, t, val)
        return c, offset

    def write(self, typelib, file):
        """
        Write a ConstDescriptor to |file|, which is assumed
        to be seeked to the proper position.

        """
        file.write(Constant._descriptorstart.pack(self._name_offset))
        self.type.write(typelib, file)
        tt = Constant.typemap[self.type.tag]
        file.write(struct.pack(tt, self.value))

    def write_name(self, file, data_pool_offset):
        """
        Write this constants's name to |file|.
        Assumes that |file| is currently seeked to an unused portion
        of the data pool.

        """
        if self.name:
            self._name_offset = file.tell() - data_pool_offset + 1
            file.write(self.name + "\x00")
        else:
            self._name_offset = 0

    def __repr__(self):
        return "Constant(%s, %s, %d)" % (self.name, str(self.type), self.value)

class Interface(object):
    """
    An Interface represents an object, with its associated methods
    and constant values.
    (InterfaceDescriptor from the typelib specification.)
    
    """
    _direntry = struct.Struct(">16sIII")
    _descriptorstart = struct.Struct(">HH")

    UNRESOLVED_IID = "00000000-0000-0000-0000-000000000000"

    def __init__(self, name, iid=UNRESOLVED_IID, namespace="",
                 resolved=False, parent=None, methods=[], constants=[],
                 scriptable=False, function=False, builtinclass=False):
        self.resolved = resolved
        #TODO: should validate IIDs!
        self.iid = iid
        self.name = name
        self.namespace = namespace
        # if unresolved, all the members following this are unusable
        self.parent = parent
        self.methods = list(methods)
        self.constants = list(constants)
        self.scriptable = scriptable
        self.function = function
        self.builtinclass = builtinclass
        # For sanity, if someone constructs an Interface and passes
        # in methods or constants, then it's resolved.
        if self.methods or self.constants:
            # make sure it has a valid IID
            if self.iid == Interface.UNRESOLVED_IID:
                raise DataError, "Cannot instantiate Interface %s containing methods or constants with an unresolved IID" % self.name
            self.resolved = True
        # These are only used for writing out the interface
        self._descriptor_offset = 0
        self._name_offset = 0
        self._namespace_offset = 0
        self.xpt_filename = None

    def __repr__(self):
        return "Interface('%s', '%s', '%s', methods=%s)" % (self.name, self.iid, self.namespace, self.methods)

    def __str__(self):
        return "Interface(name='%s', iid='%s')" % (self.name, self.iid)

    def __hash__(self):
        return hash((self.name, self.iid))

    def __cmp__(self, other):
        c = cmp(self.iid, other.iid)
        if c != 0:
            return c
        c = cmp(self.name, other.name)
        if c != 0:
            return c
        c = cmp(self.namespace, other.namespace)
        if c != 0:
            return c
        # names and IIDs are the same, check resolved
        if self.resolved != other.resolved:
            if self.resolved:
                return -1
            else:
                return 1
        else:
            # both unresolved, but names and IIDs are the same, so equal
            return 0
        #TODO: actually compare methods etc
        return 0

    def read_descriptor(self, typelib, map, data_pool):
        offset = self._descriptor_offset
        if offset == 0:
            return
        start = data_pool + offset - 1
        parent, num_methods = Interface._descriptorstart.unpack_from(map, start)
        if parent > 0 and parent <= len(typelib.interfaces):
            self.parent = typelib.interfaces[parent - 1]
        # Read methods
        offset += Interface._descriptorstart.size
        for i in range(num_methods):
            m, offset = Method.read(typelib, map, data_pool, offset)
            self.methods.append(m)
        # Read constants
        start = data_pool + offset - 1
        (num_constants, ) = struct.unpack_from(">H", map, start)
        offset = offset + struct.calcsize(">H")
        for i in range(num_constants):
            c, offset = Constant.read(typelib, map, data_pool, offset)
            self.constants.append(c)
        # Read flags
        start = data_pool + offset - 1
        (flags, ) = struct.unpack_from(">B", map, start)
        offset = offset + struct.calcsize(">B")
        # only the first two bits are flags
        flags &= 0xE0
        if flags & 0x80:
            self.scriptable = True
        if flags & 0x40:
            self.function = True
        if flags & 0x20:
            self.builtinclass = True
        self.resolved = True

    def write_directory_entry(self, file):
        """
        Write an InterfaceDirectoryEntry for this interface
        to |file|, which is assumed to be seeked to the correct offset.

        """
        file.write(Interface._direntry.pack(Typelib.string_to_iid(self.iid),
                                            self._name_offset,
                                            self._namespace_offset,
                                            self._descriptor_offset))

    def write(self, typelib, file, data_pool_offset):
        """
        Write an InterfaceDescriptor to |file|, which is assumed
        to be seeked to the proper position. If this interface
        is not resolved, do not write any data.

        """
        if not self.resolved:
            self._descriptor_offset = 0
            return
        self._descriptor_offset = file.tell() - data_pool_offset + 1
        parent_idx = 0
        if self.parent:
            parent_idx = typelib.interfaces.index(self.parent) + 1
        file.write(Interface._descriptorstart.pack(parent_idx, len(self.methods)))
        for m in self.methods:
            m.write(typelib, file)
        file.write(struct.pack(">H", len(self.constants)))
        for c in self.constants:
            c.write(typelib, file)
        flags = 0
        if self.scriptable:
            flags |= 0x80
        if self.function:
            flags |= 0x40
        if self.builtinclass:
            flags |= 0x20
        file.write(struct.pack(">B", flags))

    def write_names(self, file, data_pool_offset):
        """
        Write this interface's name and namespace to |file|,
        as well as the names of all of its methods and constants.
        Assumes that |file| is currently seeked to an unused portion
        of the data pool.

        """
        if self.name:
            self._name_offset = file.tell() - data_pool_offset + 1
            file.write(self.name + "\x00")
        else:
            self._name_offset = 0
        if self.namespace:
            self._namespace_offset = file.tell() - data_pool_offset + 1
            file.write(self.namespace + "\x00")
        else:
            self._namespace_offset = 0
        for m in self.methods:
            m.write_name(file, data_pool_offset)
        for c in self.constants:
            c.write_name(file, data_pool_offset)

class Typelib(object):
    """
    A typelib represents one entire typelib file and all the interfaces
    referenced within, whether defined entirely within the typelib or
    merely referenced by name or IID.

    Typelib objects may be instantiated directly and populated with data,
    or the static Typelib.read method may be called to read one from a file.

    """
    _header = struct.Struct(">16sBBHIII")

    def __init__(self, version=TYPELIB_VERSION, interfaces=[], annotations=[]):
        """
        Instantiate a new Typelib.

        """
        self.version = version
        self.interfaces = list(interfaces)
        self.annotations = list(annotations)
        self.filename = None

    @staticmethod
    def iid_to_string(iid):
        """
        Convert a 16-byte IID into a UUID string.

        """
        def hexify(s):
            return ''.join(["%02x" % ord(x) for x in s])

        return "%s-%s-%s-%s-%s" % (hexify(iid[:4]), hexify(iid[4:6]),
                                   hexify(iid[6:8]), hexify(iid[8:10]),
                                   hexify(iid[10:]))

    @staticmethod
    def string_to_iid(iid_str):
        """
        Convert a UUID string into a 16-byte IID.

        """
        s = iid_str.replace('-','')
        return ''.join([chr(int(s[i:i+2], 16)) for i in range(0, len(s), 2)])

    @staticmethod
    def read_string(map, data_pool, offset):
        if offset == 0:
            return ""
        sz = map.find('\x00', data_pool + offset - 1)
        if sz == -1:
            return ""
        return map[data_pool + offset - 1:sz]

    @staticmethod
    def read(input_file):
        """
        Read a typelib from |input_file| and return
        the constructed Typelib object. |input_file| can be a filename
        or a file-like object.

        """
        filename = ""
        data = None
        expected_size = None
        if isinstance(input_file, basestring):
            filename = input_file
            with open(input_file, "rb") as f:
                st = os.fstat(f.fileno())
                data = f.read(st.st_size)
                expected_size = st.st_size
        else:
            data = input_file.read()

        (magic,
         major_ver,
         minor_ver,
         num_interfaces,
         file_length,
         interface_directory_offset,
         data_pool_offset) = Typelib._header.unpack_from(data)
        if magic != XPT_MAGIC:
            raise FileFormatError, "Bad magic: %s" % magic
        xpt = Typelib((major_ver, minor_ver))
        xpt.filename = filename
        if expected_size and file_length != expected_size:
            raise FileFormatError, "File is of wrong length, got %d bytes, expected %d" % (expected_size, file_length)
        #XXX: by spec this is a zero-based file offset. however,
        # the xpt_xdr code always subtracts 1 from data offsets
        # (because that's what you do in the data pool) so it
        # winds up accidentally treating this as 1-based.
        # Filed as: https://bugzilla.mozilla.org/show_bug.cgi?id=575343
        interface_directory_offset -= 1
        # make a half-hearted attempt to read Annotations,
        # since XPIDL doesn't produce any anyway.
        start = Typelib._header.size
        (anno, ) = struct.unpack_from(">B", data, start)
        islast = anno & 0x80
        tag = anno & 0x7F
        if tag == 0: # EmptyAnnotation
            xpt.annotations.append(None)
        # We don't bother handling PrivateAnnotations or anything

        for i in range(num_interfaces):
            # iid, name, namespace, interface_descriptor
            start = interface_directory_offset + i * Interface._direntry.size
            ide = Interface._direntry.unpack_from(data, start)
            iid = Typelib.iid_to_string(ide[0])
            name = Typelib.read_string(data, data_pool_offset, ide[1])
            namespace = Typelib.read_string(data, data_pool_offset, ide[2])
            iface = Interface(name, iid, namespace)
            iface._descriptor_offset = ide[3]
            iface.xpt_filename = xpt.filename
            xpt.interfaces.append(iface)
        for iface in xpt.interfaces:
            iface.read_descriptor(xpt, data, data_pool_offset)
        return xpt

    def __repr__(self):
        return "<Typelib with %d interfaces>" % len(self.interfaces)

    def _sanityCheck(self):
        """
        Check certain assumptions about data contained in this typelib.
        Sort the interfaces array by IID, check that all interfaces
        referenced by methods exist in the array.

        """
        self.interfaces.sort()
        for i in self.interfaces:
            if i.parent and i.parent not in self.interfaces:
                raise DataError, "Interface %s has parent %s not present in typelib!" % (i.name, i.parent.name)
            for m in i.methods:
                for n, p in enumerate(m.params):
                    if isinstance(p, InterfaceType) and \
                        p.iface not in self.interfaces:
                        raise DataError, "Interface method %s::%s, parameter %d references interface %s not present in typelib!" % (i.name, m.name, n, p.iface.name)
                if isinstance(m.result, InterfaceType) and m.result.iface not in self.interfaces:
                    raise DataError, "Interface method %s::%s, result references interface %s not present in typelib!" % (i.name, m.name, m.result.iface.name)

    def writefd(self, fd):
        # write out space for a header + one empty annotation,
        # padded to 4-byte alignment.
        headersize = (Typelib._header.size + 1)
        if headersize % 4:
            headersize += 4 - headersize % 4
        fd.write("\x00" * headersize)
        # save this offset, it's the interface directory offset.
        interface_directory_offset = fd.tell()
        # write out space for an interface directory
        fd.write("\x00" * Interface._direntry.size * len(self.interfaces))
        # save this offset, it's the data pool offset.
        data_pool_offset = fd.tell()
        # write out all the interface descriptors to the data pool
        for i in self.interfaces:
            i.write_names(fd, data_pool_offset)
            i.write(self, fd, data_pool_offset)
        # now, seek back and write the header
        file_len = fd.tell()
        fd.seek(0)
        fd.write(Typelib._header.pack(XPT_MAGIC,
                                      TYPELIB_VERSION[0],
                                      TYPELIB_VERSION[1],
                                      len(self.interfaces),
                                      file_len,
                                      interface_directory_offset,
                                      data_pool_offset))
        # write an empty annotation
        fd.write(struct.pack(">B", 0x80))
        # now write the interface directory
        #XXX: bug-compatible with existing xpt lib, put it one byte
        # ahead of where it's supposed to be.
        fd.seek(interface_directory_offset - 1)
        for i in self.interfaces:
            i.write_directory_entry(fd)

    def write(self, output_file):
        """
        Write the contents of this typelib to |output_file|,
        which can be either a filename or a file-like object.

        """
        self._sanityCheck()
        if isinstance(output_file, basestring):
            with open(output_file, "wb") as f:
                self.writefd(f)
        else:
            self.writefd(output_file)

    def dump(self, out):
        """
        Print a human-readable listing of the contents of this typelib
        to |out|, in the format of xpt_dump.
        
        """
        out.write("""Header:
   Major version:         %d
   Minor version:         %d
   Number of interfaces:  %d
   Annotations:\n""" % (self.version[0], self.version[1], len(self.interfaces)))
        for i, a in enumerate(self.annotations):
            if a is None:
                out.write("      Annotation #%d is empty.\n" % i)
        out.write("\nInterface Directory:\n")
        for i in self.interfaces:
            out.write("   - %s::%s (%s):\n" % (i.namespace, i.name, i.iid))
            if not i.resolved:
                out.write("      [Unresolved]\n")
            else:
                if i.parent:
                    out.write("      Parent: %s::%s\n" % (i.parent.namespace,
                                                    i.parent.name))
                out.write("""      Flags:
         Scriptable: %s
         BuiltinClass: %s
         Function: %s\n""" % (i.scriptable and "TRUE" or "FALSE",
                              i.builtinclass and "TRUE" or "FALSE",
                              i.function and "TRUE" or "FALSE"))
                out.write("      Methods:\n")
                if len(i.methods) == 0:
                    out.write("         No Methods\n")
                else:
                    for m in i.methods:
                        out.write("   %s%s%s%s%s%s%s %s %s(%s);\n" % (
                            m.getter and "G" or " ",
                            m.setter and "S" or " ",
                            m.hidden and "H" or " ",
                            m.notxpcom and "N" or " ",
                            m.constructor and "C" or " ",
                            m.optargc and "O" or " ",
                            m.implicit_jscontext and "J" or " ",
                            str(m.result.type),
                            m.name,
                            m.params and ", ".join(str(p) for p in m.params) or ""
                            ))
                out.write("      Constants:\n")
                if len(i.constants) == 0:
                    out.write("         No Constants\n")
                else:
                    for c in i.constants:
                        out.write("         %s %s = %d;\n" % (c.type, c.name, c.value))

def xpt_dump(file):
    """
    Dump the contents of |file| to stdout in the format of xpt_dump.

    """
    t = Typelib.read(file)
    t.dump(sys.stdout)

def xpt_link(inputs):
    """
    Link all of the xpt files in |inputs| together and return the result
    as a Typelib object. All entries in inputs may be filenames or
    file-like objects. Non-scriptable interfaces that are unreferenced
    from scriptable interfaces will be removed during linking.

    """
    def read_input(i):
        if isinstance(i, Typelib):
            return i
        return Typelib.read(i)

    if not inputs:
        print >>sys.stderr, "Usage: xpt_link <destination file> <input files>"
        return None
    # This is the aggregate list of interfaces.
    interfaces = []
    # This will be a dict of replaced interface -> replaced with
    # containing interfaces that were replaced with interfaces from
    # another typelib, and the interface that replaced them.
    merged_interfaces = {}
    for f in inputs:
        t = read_input(f)
        interfaces.extend(t.interfaces)
    # Sort interfaces by name so we can merge adjacent duplicates
    interfaces.sort(key=operator.attrgetter('name'))

    Result = enum('Equal',     # Interfaces the same, doesn't matter
                  'NotEqual',  # Interfaces differ, keep both
                  'KeepFirst', # Replace second interface with first
                  'KeepSecond')# Replace first interface with second
        
    def compare(i, j):
        """
        Compare two interfaces, determine if they're equal or
        completely different, or should be merged (and indicate which
        one to keep in that case).

        """
        if i == j:
            # Arbitrary, just pick one
            return Result.Equal
        if i.name != j.name:
            if i.iid == j.iid and i.iid != Interface.UNRESOLVED_IID:
                # Same IID but different names: raise an exception.
                raise DataError, \
                    "Typelibs contain definitions of interface %s" \
                    " with different names (%s (%s) vs %s (%s))!" % \
                    (i.iid, i.name, i.xpt_filename, j.name, j.xpt_filename)
            # Otherwise just different interfaces.
            return Result.NotEqual
        # Interfaces have the same name, so either they need to be merged
        # or there's a data error. Sort out which one to keep
        if i.resolved != j.resolved:
            # prefer resolved interfaces over unresolved
            if j.resolved:
                assert i.iid == j.iid or i.iid == Interface.UNRESOLVED_IID
                # keep j
                return Result.KeepSecond
            else:
                assert i.iid == j.iid or j.iid == Interface.UNRESOLVED_IID
                # replace j with i
                return Result.KeepFirst
        elif i.iid != j.iid:
            # Prefer unresolved interfaces with valid IIDs
            if j.iid == Interface.UNRESOLVED_IID:
                # replace j with i
                assert not j.resolved
                return Result.KeepFirst
            elif i.iid == Interface.UNRESOLVED_IID:
                # keep j
                assert not i.resolved
                return Result.KeepSecond
            else:
                # Same name but different IIDs: raise an exception.
                raise DataError, \
                    "Typelibs contain definitions of interface %s" \
                                " with different IIDs (%s (%s) vs %s (%s))!" % \
                                (i.name, i.iid, i.xpt_filename, \
                                 j.iid, j.xpt_filename)
        raise DataError, "No idea what happened here: %s:%s (%s), %s:%s (%s)" % \
            (i.name, i.iid, i.xpt_filename, j.name, j.iid, j.xpt_filename)
    
    # Compare interfaces pairwise to find duplicates that should be merged.
    i = 1
    while i < len(interfaces):
        res = compare(interfaces[i-1], interfaces[i])
        if res == Result.NotEqual:
            i += 1
        elif res == Result.Equal:
            # Need to drop one but it doesn't matter which
            del interfaces[i]
        elif res == Result.KeepFirst:
            merged_interfaces[interfaces[i]] = interfaces[i-1]
            del interfaces[i]
        elif res == Result.KeepSecond:
            merged_interfaces[interfaces[i-1]] = interfaces[i]
            del interfaces[i-1]
    
    # Now fixup any merged interfaces
    def checkType(t):
        if isinstance(t, InterfaceType) and t.iface in merged_interfaces:
            t.iface = merged_interfaces[t.iface]
        elif isinstance(t, ArrayType) and \
             isinstance(t.element_type, InterfaceType) and \
             t.element_type.iface in merged_interfaces:
            t.element_type.iface = merged_interfaces[t.element_type.iface]

    for i in interfaces:
        # Replace parent references
        if i.parent in merged_interfaces:
            i.parent = merged_interfaces[i.parent]
        for m in i.methods:
            # Replace InterfaceType params and return values
            checkType(m.result.type)
            for p in m.params:
                checkType(p.type)

    # There's no need to have non-scriptable interfaces in a typelib, and
    # removing them saves memory when typelibs are loaded.  But we can't
    # just blindly remove all non-scriptable interfaces, since we still
    # need to know about non-scriptable interfaces referenced from
    # scriptable interfaces.
    worklist = set(i for i in interfaces if i.scriptable)
    required_interfaces = set()
    def maybe_add_to_worklist(iface):
        if iface in required_interfaces or iface in worklist:
            return
        worklist.add(iface)

    while worklist:
        i = worklist.pop()
        required_interfaces.add(i)
        if i.parent:
            maybe_add_to_worklist(i.parent)
        for m in i.methods:
            if isinstance(m.result.type, InterfaceType):
                maybe_add_to_worklist(m.result.type.iface)
            for p in m.params:
                if isinstance(p.type, InterfaceType):
                    maybe_add_to_worklist(p.type.iface)
                elif isinstance(p.type, ArrayType) and isinstance(p.type.element_type, InterfaceType):
                    maybe_add_to_worklist(p.type.element_type.iface)

    interfaces = list(required_interfaces)

    # Re-sort interfaces (by IID)
    interfaces.sort()
    return Typelib(interfaces=interfaces)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print >>sys.stderr, "xpt <dump|link> <files>"
        sys.exit(1)
    if sys.argv[1] == 'dump':
        xpt_dump(sys.argv[2])
    elif sys.argv[1] == 'link':
        xpt_link(sys.argv[3:]).write(sys.argv[2])
        

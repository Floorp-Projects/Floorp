#!/usr/bin/env python
# xpidl.py - A parser for cross-platform IDL (XPIDL) files.
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
#   Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Benjamin Smedberg <benjamin@smedbergs.us>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

"""A parser for cross-platform IDL (XPIDL) files."""

import sys, os.path, re
from ply import lex, yacc

"""A type conforms to the following pattern:

    def isScriptable(self):
        'returns True or False'

    def nativeType(self, calltype):
        'returns a string representation of the native type
        calltype must be 'in', 'out', or 'inout'

Interface members const/method/attribute conform to the following pattern:

    name = 'string'

    def toIDL(self):
        'returns the member signature as IDL'
"""

def attlistToIDL(attlist):
    if len(attlist) == 0:
        return ''

    attlist = list(attlist)
    attlist.sort(cmp=lambda a,b: cmp(a[0], b[0]))

    return '[%s] ' % ','.join(["%s%s" % (name, value is not None and '(%s)' % value or '')
                              for name, value, aloc in attlist])

_paramsHardcode = {
    2: ('array', 'shared', 'iid_is', 'size_is', 'retval'),
    3: ('array', 'size_is', 'const'),
}

def paramAttlistToIDL(attlist):
    if len(attlist) == 0:
        return ''

    # Hack alert: g_hash_table_foreach is pretty much unimitatable... hardcode
    # quirk
    attlist = list(attlist)
    sorted = []
    if len(attlist) in _paramsHardcode:
        for p in _paramsHardcode[len(attlist)]:
            i = 0
            while i < len(attlist):
                if attlist[i][0] == p:
                    sorted.append(attlist[i])
                    del attlist[i]
                    continue

                i += 1

    sorted.extend(attlist)

    return '[%s] ' % ', '.join(["%s%s" % (name, value is not None and ' (%s)' % value or '')
                                for name, value, aloc in sorted])

def unaliasType(t):
    while t.kind == 'typedef':
        t = t.realtype
    assert t is not None
    return t

def getBuiltinOrNativeTypeName(t):
    t = unaliasType(t)
    if t.kind == 'builtin':
        return t.name
    elif t.kind == 'native':
        assert t.specialtype is not None
        return '[%s]' % t.specialtype
    else:
        return None

class BuiltinLocation(object):
    def get(self):
        return "<builtin type>"

    def __str__(self):
        return self.get()

class Builtin(object):
    kind = 'builtin'
    location = BuiltinLocation

    def __init__(self, name, nativename, signed=False, maybeConst=False):
        self.name = name
        self.nativename = nativename
        self.signed = signed
        self.maybeConst = maybeConst

    def isScriptable(self):
        return True

    def nativeType(self, calltype, shared=False, const=False):
        if const:
            print >>sys.stderr, IDLError("[const] doesn't make sense on builtin types.", self.location, warning=True)
            const = 'const '
        elif calltype == 'in' and self.nativename.endswith('*'):
            const = 'const '
        elif shared:
            if not self.nativename.endswith('*'):
                raise IDLError("[shared] not applicable to non-pointer types.", self.location)
            const = 'const '
        else:
            const = ''
        return "%s%s %s" % (const, self.nativename,
                            calltype != 'in' and '*' or '')

builtinNames = [
    Builtin('boolean', 'bool'),
    Builtin('void', 'void'),
    Builtin('octet', 'PRUint8'),
    Builtin('short', 'PRInt16', True, True),
    Builtin('long', 'PRInt32', True, True),
    Builtin('long long', 'PRInt64', True, False),
    Builtin('unsigned short', 'PRUint16', False, True),
    Builtin('unsigned long', 'PRUint32', False, True),
    Builtin('unsigned long long', 'PRUint64', False, False),
    Builtin('float', 'float', True, False),
    Builtin('double', 'double', True, False),
    Builtin('char', 'char', True, False),
    Builtin('string', 'char *', False, False),
    Builtin('wchar', 'PRUnichar', False, False),
    Builtin('wstring', 'PRUnichar *', False, False),
]

builtinMap = {}
for b in builtinNames:
    builtinMap[b.name] = b

class Location(object):
    _line = None

    def __init__(self, lexer, lineno, lexpos):
        self._lineno = lineno
        self._lexpos = lexpos
        self._lexdata = lexer.lexdata
        self._file = getattr(lexer, 'filename', "<unknown>")

    def __eq__(self, other):
        return self._lexpos == other._lexpos and \
               self._file == other._file

    def resolve(self):
        if self._line:
            return

        startofline = self._lexdata.rfind('\n', 0, self._lexpos) + 1
        endofline = self._lexdata.find('\n', self._lexpos, self._lexpos + 80)
        self._line = self._lexdata[startofline:endofline]
        self._colno = self._lexpos - startofline

    def pointerline(self):
        def i():
            for i in xrange(0, self._colno):
                yield " "
            yield "^"

        return "".join(i())

    def get(self):
        self.resolve()
        return "%s line %s:%s" % (self._file, self._lineno, self._colno)

    def __str__(self):
        self.resolve()
        return "%s line %s:%s\n%s\n%s" % (self._file, self._lineno, self._colno,
                                          self._line, self.pointerline())

class NameMap(object):
    """Map of name -> object. Each object must have a .name and .location property.
    Setting the same name twice throws an error."""
    def __init__(self):
        self._d = {}

    def __getitem__(self, key):
        if key in builtinMap:
            return builtinMap[key]
        return self._d[key]

    def __iter__(self):
        return self._d.itervalues()

    def __contains__(self, key):
        return key in builtinMap or key in self._d

    def set(self, object):
        if object.name in builtinMap:
            raise IDLError("name '%s' is a builtin and cannot be redeclared" % (object.name), object.location)
        if object.name in self._d:
            old = self._d[object.name]
            if old == object: return
            if isinstance(old, Forward) and isinstance(object, Interface):
                self._d[object.name] = object
            elif isinstance(old, Interface) and isinstance(object, Forward):
                pass
            else:
                raise IDLError("name '%s' specified twice. Previous location: %s" % (object.name, self._d[object.name].location), object.location)
        else:
            self._d[object.name] = object

    def get(self, id, location):
        try:
            return self[id]
        except KeyError:
            raise IDLError("Name '%s' not found", location)

class IDLError(Exception):
    def __init__(self, message, location, warning=False):
        self.message = message
        self.location = location
        self.warning = warning

    def __str__(self):
        return "%s: %s, %s" % (self.warning and 'warning' or 'error',
                               self.message, self.location)

class Include(object):
    kind = 'include'

    def __init__(self, filename, location):
        self.filename = filename
        self.location = location

    def __str__(self):
        return "".join(["include '%s'\n" % self.filename])

    def resolve(self, parent):
        def incfiles():
            yield self.filename
            for dir in parent.incdirs:
                yield os.path.join(dir, self.filename)

        for file in incfiles():
            if not os.path.exists(file): continue

            self.IDL = parent.parser.parse(open(file).read(), filename=file)
            self.IDL.resolve(parent.incdirs, parent.parser)
            for type in self.IDL.getNames():
                parent.setName(type)
            parent.deps.extend(self.IDL.deps)
            return

        raise IDLError("File '%s' not found" % self.filename, self.location)

class IDL(object):
    def __init__(self, productions):
        self.productions = productions
        self.deps = []

    def setName(self, object):
        self.namemap.set(object)

    def getName(self, id, location):
        try:
            return self.namemap[id]
        except KeyError:
            raise IDLError("type '%s' not found" % id, location)

    def hasName(self, id):
        return id in self.namemap

    def getNames(self):
        return iter(self.namemap)

    def __str__(self):
        return "".join([str(p) for p in self.productions])

    def resolve(self, incdirs, parser):
        self.namemap = NameMap()
        self.incdirs = incdirs
        self.parser = parser
        for p in self.productions:
            p.resolve(self)

    def includes(self):
        for p in self.productions:
            if p.kind == 'include':
                yield p

    def needsJSTypes(self):
        for p in self.productions:
            if p.kind == 'interface' and p.needsJSTypes():
                return True
        return False

class CDATA(object):
    kind = 'cdata'
    _re = re.compile(r'\n+')

    def __init__(self, data, location):
        self.data = self._re.sub('\n', data)
        self.location = location

    def resolve(self, parent):
        pass

    def __str__(self):
        return "cdata: %s\n\t%r\n" % (self.location.get(), self.data)

    def count(self):
        return 0

class Typedef(object):
    kind = 'typedef'

    def __init__(self, type, name, location, doccomments):
        self.type = type
        self.name = name
        self.location = location
        self.doccomments = doccomments

    def __eq__(self, other):
        return self.name == other.name and self.type == other.type

    def resolve(self, parent):
        parent.setName(self)
        self.realtype = parent.getName(self.type, self.location)

    def isScriptable(self):
        return self.realtype.isScriptable()

    def nativeType(self, calltype):
        return "%s %s" % (self.name,
                          calltype != 'in' and '*' or '')

    def __str__(self):
        return "typedef %s %s\n" % (self.type, self.name)

class Forward(object):
    kind = 'forward'

    def __init__(self, name, location, doccomments):
        self.name = name
        self.location = location
        self.doccomments = doccomments

    def __eq__(self, other):
        return other.kind == 'forward' and other.name == self.name

    def resolve(self, parent):
        # Hack alert: if an identifier is already present, move the doccomments
        # forward.
        if parent.hasName(self.name):
            for i in xrange(0, len(parent.productions)):
                if parent.productions[i] is self: break
            for i in xrange(i + 1, len(parent.productions)):
                if hasattr(parent.productions[i], 'doccomments'):
                    parent.productions[i].doccomments[0:0] = self.doccomments
                    break

        parent.setName(self)

    def isScriptable(self):
        return True

    def nativeType(self, calltype):
        return "%s %s" % (self.name,
                          calltype != 'in' and '* *' or '*')

    def __str__(self):
        return "forward-declared %s\n" % self.name

class Native(object):
    kind = 'native'

    modifier = None
    specialtype = None

    specialtypes = {
        'nsid': None,
        'domstring': 'nsAString',
        'utf8string': 'nsACString',
        'cstring': 'nsACString',
        'astring': 'nsAString',
        'jsval': 'JS::Value'
        }

    def __init__(self, name, nativename, attlist, location):
        self.name = name
        self.nativename = nativename
        self.location = location

        for name, value, aloc in attlist:
            if value is not None:
                raise IDLError("Unexpected attribute value", aloc)
            if name in ('ptr', 'ref'):
                if self.modifier is not None:
                    raise IDLError("More than one ptr/ref modifier", aloc)
                self.modifier = name
            elif name in self.specialtypes.keys():
                if self.specialtype is not None:
                    raise IDLError("More than one special type", aloc)
                self.specialtype = name
                if self.specialtypes[name] is not None:
                    self.nativename = self.specialtypes[name]
            else:
                raise IDLError("Unexpected attribute", aloc)

    def __eq__(self, other):
        return self.name == other.name and \
               self.nativename == other.nativename and \
               self.modifier == other.modifier and \
               self.specialtype == other.specialtype

    def resolve(self, parent):
        parent.setName(self)

    def isScriptable(self):
        if self.specialtype is None:
            return False

        if self.specialtype == 'nsid':
            return self.modifier is not None

        return self.modifier == 'ref'

    def isPtr(self, calltype):
        return self.modifier == 'ptr' or (self.modifier == 'ref' and self.specialtype == 'jsval' and calltype == 'out')

    def isRef(self, calltype):
        return self.modifier == 'ref' and not (self.specialtype == 'jsval' and calltype == 'out')

    def nativeType(self, calltype, const=False, shared=False):
        if shared:
            if calltype != 'out':
                raise IDLError("[shared] only applies to out parameters.")
            const = True

        if self.specialtype is not None and calltype == 'in':
            const = True

        if self.isRef(calltype):
            m = '& '
        elif self.isPtr(calltype):
            m = '*' + ((self.modifier == 'ptr' and calltype != 'in') and '*' or '')
        else:
            m = calltype != 'in' and '*' or ''
        return "%s%s %s" % (const and 'const ' or '', self.nativename, m)

    def __str__(self):
        return "native %s(%s)\n" % (self.name, self.nativename)

class BaseInterface(object):
    def __init__(self, name, attlist, base, members, location, doccomments):
        self.name = name
        self.attributes = InterfaceAttributes(attlist, location)
        self.base = base
        self.members = members
        self.location = location
        self.namemap = NameMap()
        self.doccomments = doccomments
        self.nativename = name

        for m in members:
            if not isinstance(m, CDATA):
                self.namemap.set(m)

        self.ops = {
            'index':
                {
                    'getter': None,
                    'setter': None
                },
            'name':
                {
                    'getter': None,
                    'setter': None
                },
            'stringifier': None
            }

    def __eq__(self, other):
        return self.name == other.name and self.location == other.location

    def resolve(self, parent):
        self.idl = parent

        # Hack alert: if an identifier is already present, libIDL assigns
        # doc comments incorrectly. This is quirks-mode extraordinaire!
        if parent.hasName(self.name):
            for member in self.members:
                if hasattr(member, 'doccomments'):
                    member.doccomments[0:0] = self.doccomments
                    break
            self.doccomments = parent.getName(self.name, None).doccomments

        if self.attributes.function:
            has_method = False
            for member in self.members:
                if member.kind is 'method':
                    if has_method:
                        raise IDLError("interface '%s' has multiple methods, but marked 'function'" % self.name, self.location)
                    else:
                        has_method = True

        parent.setName(self)
        if self.base is not None:
            realbase = parent.getName(self.base, self.location)
            if realbase.kind != self.kind:
                raise IDLError("%s '%s' inherits from non-%s type '%s'" % (self.kind, self.name, self.kind, self.base), self.location)

            if self.attributes.scriptable and not realbase.attributes.scriptable:
                print >>sys.stderr, IDLError("interface '%s' is scriptable but derives from non-scriptable '%s'" % (self.name, self.base), self.location, warning=True)

        forwardedMembers = set()
        for member in self.members:
            member.resolve(self)
            if member.kind is 'method' and member.forward:
                forwardedMembers.add(member.forward)
        for member in self.members:
            if member.kind is 'method' and member.name in forwardedMembers:
                forwardedMembers.remove(member.name)
        for member in forwardedMembers:
            raise IDLError("member '%s' on interface '%s' forwards to '%s' which is not on the interface itself" % (member.name, self.name, member.forward), self.location)

        # The number 250 is NOT arbitrary; this number is the maximum number of
        # stub entries defined in xpcom/reflect/xptcall/public/genstubs.pl
        # Do not increase this value without increasing the number in that
        # location, or you WILL cause otherwise unknown problems!
        if self.countEntries() > 250 and not self.attributes.builtinclass:
            raise IDLError("interface '%s' has too many entries" % self.name,
                self.location)

    def isScriptable(self):
        # NOTE: this is not whether *this* interface is scriptable... it's
        # whether, when used as a type, it's scriptable, which is true of all
        # interfaces.
        return True

    def nativeType(self, calltype, const=False):
        return "%s%s %s" % (const and 'const ' or '',
                            self.name,
                            calltype != 'in' and '* *' or '*')

    def __str__(self):
        l = ["interface %s\n" % self.name]
        if self.base is not None:
            l.append("\tbase %s\n" % self.base)
        l.append(str(self.attributes))
        if self.members is None:
            l.append("\tincomplete type\n")
        else:
            for m in self.members:
                l.append(str(m))
        return "".join(l)

    def getConst(self, name, location):
        # The constant may be in a base class
        iface = self
        while name not in iface.namemap and iface is not None:
            iface = self.idl.getName(self.base, self.location)
        if iface is None:
            raise IDLError("cannot find symbol '%s'" % name, c.location)
        c = iface.namemap.get(name, location)
        if c.kind != 'const':
            raise IDLError("symbol '%s' is not a constant", c.location)

        return c.getValue()

    def needsJSTypes(self):
        for m in self.members:
            if m.kind == "attribute" and m.type == "jsval":
                return True
            if m.kind == "method" and m.needsJSTypes():
                return True
        return False

    def countEntries(self):
        ''' Returns the number of entries in the vtable for this interface. '''
        total = sum(member.count() for member in self.members)
        if self.base is not None:
            realbase = self.idl.getName(self.base, self.location)
            total += realbase.countEntries()
        return total

class Interface(BaseInterface):
    kind = 'interface'

    def __init__(self, name, attlist, base, members, location, doccomments):
        BaseInterface.__init__(self, name, attlist, base, members, location, doccomments)

        if self.attributes.uuid is None:
            raise IDLError("interface has no uuid", location)

class Dictionary(BaseInterface):
    kind = 'dictionary'

    def __init__(self, name, attlist, base, members, location, doccomments):
        BaseInterface.__init__(self, name, attlist, base, members, location, doccomments)

class InterfaceAttributes(object):
    uuid = None
    scriptable = False
    builtinclass = False
    function = False
    deprecated = False
    noscript = False

    def setuuid(self, value):
        self.uuid = value.lower()

    def setscriptable(self):
        self.scriptable = True

    def setfunction(self):
        self.function = True

    def setnoscript(self):
        self.noscript = True

    def setbuiltinclass(self):
        self.builtinclass = True

    def setdeprecated(self):
        self.deprecated = True

    actions = {
        'uuid':       (True, setuuid),
        'scriptable': (False, setscriptable),
        'builtinclass': (False, setbuiltinclass),
        'function':   (False, setfunction),
        'noscript':   (False, setnoscript),
        'deprecated': (False, setdeprecated),
        'object':     (False, lambda self: True),
        }

    def __init__(self, attlist, location):
        def badattribute(self):
            raise IDLError("Unexpected interface attribute '%s'" % name, location)

        for name, val, aloc in attlist:
            hasval, action = self.actions.get(name, (False, badattribute))
            if hasval:
                if val is None:
                    raise IDLError("Expected value for attribute '%s'" % name,
                                   aloc)

                action(self, val)
            else:
                if val is not None:
                    raise IDLError("Unexpected value for attribute '%s'" % name,
                                   aloc)

                action(self)

    def __str__(self):
        l = []
        if self.uuid:
            l.append("\tuuid: %s\n" % self.uuid)
        if self.scriptable:
            l.append("\tscriptable\n")
        if self.builtinclass:
            l.append("\tbuiltinclass\n")
        if self.function:
            l.append("\tfunction\n")
        return "".join(l)

class ConstMember(object):
    kind = 'const'
    def __init__(self, type, name, value, location, doccomments):
        self.type = type
        self.name = name
        self.value = value
        self.location = location
        self.doccomments = doccomments

    def resolve(self, parent):
        self.realtype = parent.idl.getName(self.type, self.location)
        self.iface = parent
        basetype = self.realtype
        while isinstance(basetype, Typedef):
            basetype = basetype.realtype
        if not isinstance(basetype, Builtin) or not basetype.maybeConst:
            raise IDLError("const may only be a short or long type, not %s" % self.type, self.location)

        self.basetype = basetype

    def getValue(self):
        return self.value(self.iface)

    def __str__(self):
        return "\tconst %s %s = %s\n" % (self.type, self.name, self.getValue())

    def count(self):
        return 0

class Attribute(object):
    kind = 'attribute'
    noscript = False
    notxpcom = False
    readonly = False
    implicit_jscontext = False
    nostdcall = False
    binaryname = None
    null = None
    undefined = None
    deprecated = False
    nullable = False
    defvalue = None

    def __init__(self, type, name, attlist, readonly, nullable, defvalue, location, doccomments):
        self.type = type
        self.name = name
        self.attlist = attlist
        self.readonly = readonly
        self.nullable = nullable
        self.defvalue = defvalue
        self.location = location
        self.doccomments = doccomments

        for name, value, aloc in attlist:
            if name == 'binaryname':
                if value is None:
                    raise IDLError("binaryname attribute requires a value",
                                   aloc)

                self.binaryname = value
                continue

            if name == 'Null':
                if value is None:
                    raise IDLError("'Null' attribute requires a value", aloc)
                if readonly:
                    raise IDLError("'Null' attribute only makes sense for setters",
                                   aloc);
                if value not in ('Empty', 'Null', 'Stringify'):
                    raise IDLError("'Null' attribute value must be 'Empty', 'Null' or 'Stringify'",
                                   aloc);
                self.null = value
            elif name == 'Undefined':
                if value is None:
                    raise IDLError("'Undefined' attribute requires a value", aloc)
                if readonly:
                    raise IDLError("'Undefined' attribute only makes sense for setters",
                                   aloc);
                if value not in ('Empty', 'Null'):
                    raise IDLError("'Undefined' attribute value must be 'Empty' or 'Null'",
                                   aloc);
                self.undefined = value
            else:
                if value is not None:
                    raise IDLError("Unexpected attribute value", aloc)

                if name == 'noscript':
                    self.noscript = True
                elif name == 'notxpcom':
                    self.notxpcom = True
                elif name == 'implicit_jscontext':
                    self.implicit_jscontext = True
                elif name == 'deprecated':
                    self.deprecated = True
                elif name == 'nostdcall':
                    self.nostdcall = True
                else:
                    raise IDLError("Unexpected attribute '%s'" % name, aloc)

    def resolve(self, iface):
        self.iface = iface
        self.realtype = iface.idl.getName(self.type, self.location)
        if (self.null is not None and
            getBuiltinOrNativeTypeName(self.realtype) != '[domstring]'):
            raise IDLError("'Null' attribute can only be used on DOMString",
                           self.location)
        if (self.undefined is not None and
            getBuiltinOrNativeTypeName(self.realtype) != '[domstring]'):
            raise IDLError("'Undefined' attribute can only be used on DOMString",
                           self.location)
        if (self.nullable and
            getBuiltinOrNativeTypeName(self.realtype) != '[domstring]'):
            raise IDLError("Nullable types (T?) is supported only for DOMString",
                           self.location)

    def toIDL(self):
        attribs = attlistToIDL(self.attlist)
        readonly = self.readonly and 'readonly ' or ''
        return "%s%sattribute %s %s;" % (attribs, readonly, self.type, self.name)
        
    def isScriptable(self):
        if not self.iface.attributes.scriptable: return False
        return not (self.noscript or self.notxpcom)

    def __str__(self):
        return "\t%sattribute %s %s\n" % (self.readonly and 'readonly ' or '',
                                          self.type, self.name)

    def count(self):
        return self.readonly and 1 or 2

class Method(object):
    kind = 'method'
    noscript = False
    notxpcom = False
    binaryname = None
    implicit_jscontext = False
    nostdcall = False
    optional_argc = False
    deprecated = False
    getter = False
    setter = False
    stringifier = False
    forward = None

    def __init__(self, type, name, attlist, paramlist, location, doccomments, raises):
        self.type = type
        self.name = name
        self.attlist = attlist
        self.params = paramlist
        self.location = location
        self.doccomments = doccomments
        self.raises = raises

        for name, value, aloc in attlist:
            if name == 'binaryname':
                if value is None:
                    raise IDLError("binaryname attribute requires a value",
                                   aloc)

                self.binaryname = value
                continue
            if name == 'forward':
                if value is None:
                    raise IDLError("forward attribute requires a value",
                                   aloc)

                self.forward = value
                continue

            if value is not None:
                raise IDLError("Unexpected attribute value", aloc)

            if name == 'noscript':
                self.noscript = True
            elif name == 'notxpcom':
                self.notxpcom = True
            elif name == 'implicit_jscontext':
                self.implicit_jscontext = True
            elif name == 'optional_argc':
                self.optional_argc = True
            elif name == 'deprecated':
                self.deprecated = True
            elif name == 'nostdcall':
                self.nostdcall = True
            elif name == 'getter':
                if (len(self.params) != 1):
                    raise IDLError("Methods marked as getter must take 1 argument", aloc)
                self.getter = True
            elif name == 'setter':
                if (len(self.params) != 2):
                    raise IDLError("Methods marked as setter must take 2 arguments", aloc)
                self.setter = True
            elif name == 'stringifier':
                if (len(self.params) != 0):
                    raise IDLError("Methods marked as stringifier must take 0 arguments", aloc)
                self.stringifier = True
            else:
                raise IDLError("Unexpected attribute '%s'" % name, aloc)

        self.namemap = NameMap()
        for p in paramlist:
            self.namemap.set(p)

    def resolve(self, iface):
        self.iface = iface
        self.realtype = self.iface.idl.getName(self.type, self.location)
        for p in self.params:
            p.resolve(self)
        if self.getter:
            if getBuiltinOrNativeTypeName(self.params[0].realtype) == 'unsigned long':
                ops = 'index'
            else:
                if getBuiltinOrNativeTypeName(self.params[0].realtype) != '[domstring]':
                    raise IDLError("a getter must take a single unsigned long or DOMString argument" % self.iface.name, self.location)
                ops = 'name'
            if self.iface.ops[ops]['getter']:
                raise IDLError("a %s getter was already defined on interface '%s'" % (ops, self.iface.name), self.location)
            self.iface.ops[ops]['getter'] = self
        if self.setter:
            if getBuiltinOrNativeTypeName(self.params[0].realtype) == 'unsigned long':
                ops = 'index'
            else:
                if getBuiltinOrNativeTypeName(self.params[0].realtype) != '[domstring]':
                    print getBuiltinOrNativeTypeName(self.params[0].realtype)
                    raise IDLError("a setter must take a unsigned long or DOMString argument" % self.iface.name, self.location)
                ops = 'name'
            if self.iface.ops[ops]['setter']:
                raise IDLError("a %s setter was already defined on interface '%s'" % (ops, self.iface.name), self.location)
            self.iface.ops[ops]['setter'] = self
        if self.stringifier:
            if self.iface.ops['stringifier']:
                raise IDLError("a stringifier was already defined on interface '%s'" % self.iface.name, self.location)
            if getBuiltinOrNativeTypeName(self.realtype) != '[domstring]':
                raise IDLError("'stringifier' attribute can only be used on methods returning DOMString",
                               self.location)
            self.iface.ops['stringifier'] = self
        for p in self.params:
            if p.size_is:
                found_size_param = False
                for size_param in self.params:
                    if p.size_is == size_param.name:
                        found_size_param = True
                        if getBuiltinOrNativeTypeName(size_param.realtype) != 'unsigned long':
                            raise IDLError("is_size parameter must have type 'unsigned long'", self.location)
                if not found_size_param:
                    raise IDLError("could not find is_size parameter '%s'" % p.size_is, self.location)

    def isScriptable(self):
        if not self.iface.attributes.scriptable: return False
        return not (self.noscript or self.notxpcom)

    def __str__(self):
        return "\t%s %s(%s)\n" % (self.type, self.name, ", ".join([p.name for p in self.params]))

    def toIDL(self):
        if len(self.raises):
            raises = ' raises (%s)' % ','.join(self.raises)
        else:
            raises = ''

        return "%s%s %s (%s)%s;" % (attlistToIDL(self.attlist),
                                    self.type,
                                    self.name,
                                    ", ".join([p.toIDL()
                                               for p in self.params]),
                                    raises)

    def needsJSTypes(self):
        if self.implicit_jscontext:
            return True
        if self.type == "jsval":
            return True
        for p in self.params:
            t = p.realtype
            if isinstance(t, Native) and t.specialtype == "jsval":
                return True
        return False

    def count(self):
        return 1

class Param(object):
    size_is = None
    iid_is = None
    const = False
    array = False
    retval = False
    shared = False
    optional = False
    null = None
    undefined = None

    def __init__(self, paramtype, type, name, attlist, location, realtype=None):
        self.paramtype = paramtype
        self.type = type
        self.name = name
        self.attlist = attlist
        self.location = location
        self.realtype = realtype

        for name, value, aloc in attlist:
            # Put the value-taking attributes first!
            if name == 'size_is':
                if value is None:
                    raise IDLError("'size_is' must specify a parameter", aloc)
                self.size_is = value
            elif name == 'iid_is':
                if value is None:
                    raise IDLError("'iid_is' must specify a parameter", aloc)
                self.iid_is = value
            elif name == 'Null':
                if value is None:
                    raise IDLError("'Null' must specify a parameter", aloc)
                if value not in ('Empty', 'Null', 'Stringify'):
                    raise IDLError("'Null' parameter value must be 'Empty', 'Null', or 'Stringify'",
                                   aloc);
                self.null = value
            elif name == 'Undefined':
                if value is None:
                    raise IDLError("'Undefined' must specify a parameter", aloc)
                if value not in ('Empty', 'Null'):
                    raise IDLError("'Undefined' parameter value must be 'Empty' or 'Null'",
                                   aloc);
                self.undefined = value
            else:
                if value is not None:
                    raise IDLError("Unexpected value for attribute '%s'" % name,
                                   aloc)

                if name == 'const':
                    self.const = True
                elif name == 'array':
                    self.array = True
                elif name == 'retval':
                    self.retval = True
                elif name == 'shared':
                    self.shared = True
                elif name == 'optional':
                    self.optional = True
                else:
                    raise IDLError("Unexpected attribute '%s'" % name, aloc)

    def resolve(self, method):
        self.realtype = method.iface.idl.getName(self.type, self.location)
        if self.array:
            self.realtype = Array(self.realtype)
        if (self.null is not None and
            getBuiltinOrNativeTypeName(self.realtype) != '[domstring]'):
            raise IDLError("'Null' attribute can only be used on DOMString",
                           self.location)
        if (self.undefined is not None and
            getBuiltinOrNativeTypeName(self.realtype) != '[domstring]'):
            raise IDLError("'Undefined' attribute can only be used on DOMString",
                           self.location)

    def nativeType(self):
        kwargs = {}
        if self.shared: kwargs['shared'] = True
        if self.const: kwargs['const'] = True

        try:
            return self.realtype.nativeType(self.paramtype, **kwargs)
        except IDLError, e:
            raise IDLError(e.message, self.location)
        except TypeError, e:
            raise IDLError("Unexpected parameter attribute", self.location)

    def toIDL(self):
        return "%s%s %s %s" % (paramAttlistToIDL(self.attlist),
                               self.paramtype,
                               self.type,
                               self.name)

class Array(object):
    def __init__(self, basetype):
        self.type = basetype

    def isScriptable(self):
        return self.type.isScriptable()

    def nativeType(self, calltype, const=False):
        return "%s%s*" % (const and 'const ' or '',
                          self.type.nativeType(calltype))

class IDLParser(object):
    keywords = {
        'const': 'CONST',
        'interface': 'INTERFACE',
        'dictionary': 'DICTIONARY',
        'in': 'IN',
        'inout': 'INOUT',
        'out': 'OUT',
        'attribute': 'ATTRIBUTE',
        'raises': 'RAISES',
        'readonly': 'READONLY',
        'native': 'NATIVE',
        'typedef': 'TYPEDEF'
        }

    tokens = [
        'IDENTIFIER',
        'CDATA',
        'INCLUDE',
        'IID',
        'NUMBER',
        'HEXNUM',
        'LSHIFT',
        'RSHIFT',
        'NATIVEID',
        'STRING',
        ]

    tokens.extend(keywords.values())

    states = (
        ('nativeid', 'exclusive'),
    )

    hexchar = r'[a-fA-F0-9]'

    t_NUMBER = r'-?\d+'
    t_HEXNUM = r'0x%s+' % hexchar
    t_LSHIFT = r'<<'
    t_RSHIFT=  r'>>'

    literals = '"(){}[],;:=|+-*?'

    t_ignore = ' \t'

    def t_multilinecomment(self, t):
        r'/\*(?s).*?\*/'
        t.lexer.lineno += t.value.count('\n')
        if t.value.startswith("/**"):
            self._doccomments.append(t.value)

    def t_singlelinecomment(self, t):
        r'(?m)//.*?$'

    def t_IID(self, t):
        return t
    t_IID.__doc__ = r'%(c)s{8}-%(c)s{4}-%(c)s{4}-%(c)s{4}-%(c)s{12}' % {'c': hexchar}

    def t_IDENTIFIER(self, t):
        r'unsigned\ long\ long|unsigned\ short|unsigned\ long|long\ long|[A-Za-z][A-Za-z_0-9]*'
        t.type = self.keywords.get(t.value, 'IDENTIFIER')
        return t

    def t_LCDATA(self, t):
        r'(?s)%\{[ ]*C\+\+[ ]*\n(?P<cdata>.*?\n?)%\}[ ]*(C\+\+)?'
        t.type = 'CDATA'
        t.value = t.lexer.lexmatch.group('cdata')
        t.lexer.lineno += t.value.count('\n')
        return t

    def t_INCLUDE(self, t):
        r'\#include[ \t]+"[^"\n]+"'
        inc, value, end = t.value.split('"')
        t.value = value
        return t

    def t_STRING(self, t):
        r'"[^"\n]+"'
        begin, value, end = t.value.split('"')
        t.value = value
        return t

    def t_directive(self, t):
        r'\#(?P<directive>[a-zA-Z]+)[^\n]+'
        raise IDLError("Unrecognized directive %s" % t.lexer.lexmatch.group('directive'),
                       Location(lexer=self.lexer, lineno=self.lexer.lineno,
                                lexpos=self.lexer.lexpos))

    def t_newline(self, t):
        r'\n+'
        t.lexer.lineno += len(t.value)

    def t_nativeid_NATIVEID(self, t):
        r'[^()\n]+(?=\))'
        t.lexer.begin('INITIAL')
        return t

    t_nativeid_ignore = ''

    def t_ANY_error(self, t):
        raise IDLError("unrecognized input",
                       Location(lexer=self.lexer,
                                lineno=self.lexer.lineno,
                                lexpos=self.lexer.lexpos))

    precedence = (
        ('left', '|'),
        ('left', 'LSHIFT', 'RSHIFT'),
        ('left', '+', '-'),
        ('left', '*'),
        ('left', 'UMINUS'),
    )

    def p_idlfile(self, p):
        """idlfile : productions"""
        p[0] = IDL(p[1])

    def p_productions_start(self, p):
        """productions : """
        p[0] = []

    def p_productions_cdata(self, p):
        """productions : CDATA productions"""
        p[0] = list(p[2])
        p[0].insert(0, CDATA(p[1], self.getLocation(p, 1)))

    def p_productions_include(self, p):
        """productions : INCLUDE productions"""
        p[0] = list(p[2])
        p[0].insert(0, Include(p[1], self.getLocation(p, 1)))

    def p_productions_interface(self, p):
        """productions : interface productions
                       | dictionary productions
                       | typedef productions
                       | native productions"""
        p[0] = list(p[2])
        p[0].insert(0, p[1])

    def p_typedef(self, p):
        """typedef : TYPEDEF IDENTIFIER IDENTIFIER ';'"""
        p[0] = Typedef(type=p[2],
                       name=p[3],
                       location=self.getLocation(p, 1),
                       doccomments=p.slice[1].doccomments)

    def p_native(self, p):
        """native : attributes NATIVE IDENTIFIER afternativeid '(' NATIVEID ')' ';'"""
        p[0] = Native(name=p[3],
                      nativename=p[6],
                      attlist=p[1]['attlist'],
                      location=self.getLocation(p, 2))

    def p_afternativeid(self, p):
        """afternativeid : """
        # this is a place marker: we switch the lexer into literal identifier
        # mode here, to slurp up everything until the closeparen
        self.lexer.begin('nativeid')

    def p_anyident(self, p):
        """anyident : IDENTIFIER
                    | CONST"""
        p[0] = {'value': p[1],
                'location': self.getLocation(p, 1)}

    def p_attributes(self, p):
        """attributes : '[' attlist ']'
                      | """
        if len(p) == 1:
            p[0] = {'attlist': []}
        else:
            p[0] = {'attlist': p[2],
                    'doccomments': p.slice[1].doccomments}

    def p_attlist_start(self, p):
        """attlist : attribute"""
        p[0] = [p[1]]

    def p_attlist_continue(self, p):
        """attlist : attribute ',' attlist"""
        p[0] = list(p[3])
        p[0].insert(0, p[1])

    def p_attribute(self, p):
        """attribute : anyident attributeval"""
        p[0] = (p[1]['value'], p[2], p[1]['location'])

    def p_attributeval(self, p):
        """attributeval : '(' IDENTIFIER ')'
                        | '(' IID ')'
                        | """
        if len(p) > 1:
            p[0] = p[2]

    def p_interface(self, p):
        """interface : attributes INTERFACE IDENTIFIER ifacebase ifacebody ';'"""
        atts, INTERFACE, name, base, body, SEMI = p[1:]
        attlist = atts['attlist']
        doccomments = []
        if 'doccomments' in atts:
            doccomments.extend(atts['doccomments'])
        doccomments.extend(p.slice[2].doccomments)

        l = lambda: self.getLocation(p, 2)

        if body is None:
            # forward-declared interface... must not have attributes!
            if len(attlist) != 0:
                raise IDLError("Forward-declared interface must not have attributes",
                               list[0][3])

            if base is not None:
                raise IDLError("Forward-declared interface must not have a base",
                               l())
            p[0] = Forward(name=name, location=l(), doccomments=doccomments)
        else:
            p[0] = Interface(name=name,
                             attlist=attlist,
                             base=base,
                             members=body,
                             location=l(),
                             doccomments=doccomments)

    def p_ifacebody(self, p):
        """ifacebody : '{' members '}'
                     | """
        if len(p) > 1:
            p[0] = p[2]

    def p_ifacebase(self, p):
        """ifacebase : ':' IDENTIFIER
                     | """
        if len(p) == 3:
            p[0] = p[2]

    def p_members_start(self, p):
        """members : """
        p[0] = []

    def p_members_continue(self, p):
        """members : member members"""
        p[0] = list(p[2])
        p[0].insert(0, p[1])

    def p_member_cdata(self, p):
        """member : CDATA"""
        p[0] = CDATA(p[1], self.getLocation(p, 1))

    def p_member_const(self, p):
        """member : CONST IDENTIFIER IDENTIFIER '=' number ';' """
        p[0] = ConstMember(type=p[2], name=p[3],
                           value=p[5], location=self.getLocation(p, 1),
                           doccomments=p.slice[1].doccomments)

# All "number" products return a function(interface)

    def p_number_decimal(self, p):
        """number : NUMBER"""
        n = int(p[1])
        p[0] = lambda i: n

    def p_number_hex(self, p):
        """number : HEXNUM"""
        n = int(p[1], 16)
        p[0] = lambda i: n

    def p_number_identifier(self, p):
        """number : IDENTIFIER"""
        id = p[1]
        loc = self.getLocation(p, 1)
        p[0] = lambda i: i.getConst(id, loc)

    def p_number_paren(self, p):
        """number : '(' number ')'"""
        p[0] = p[2]

    def p_number_neg(self, p):
        """number : '-' number %prec UMINUS"""
        n = p[2]
        p[0] = lambda i: - n(i)

    def p_number_add(self, p):
        """number : number '+' number
                  | number '-' number
                  | number '*' number"""
        n1 = p[1]
        n2 = p[3]
        if p[2] == '+':
            p[0] = lambda i: n1(i) + n2(i)
        elif p[2] == '-':
            p[0] = lambda i: n1(i) - n2(i)
        else:
            p[0] = lambda i: n1(i) * n2(i)

    def p_number_shift(self, p):
        """number : number LSHIFT number
                  | number RSHIFT number"""
        n1 = p[1]
        n2 = p[3]
        if p[2] == '<<':
            p[0] = lambda i: n1(i) << n2(i)
        else:
            p[0] = lambda i: n1(i) >> n2(i)

    def p_number_bitor(self, p):
        """number : number '|' number"""
        n1 = p[1]
        n2 = p[3]
        p[0] = lambda i: n1(i) | n2(i)

    def p_member_att(self, p):
        """member : attributes optreadonly ATTRIBUTE IDENTIFIER identifier ';'"""
        if 'doccomments' in p[1]:
            doccomments = p[1]['doccomments']
        elif p[2] is not None:
            doccomments = p[2]
        else:
            doccomments = p.slice[3].doccomments

        p[0] = Attribute(type=p[4],
                         name=p[5],
                         attlist=p[1]['attlist'],
                         readonly=p[2] is not None,
                         nullable=False,
                         defvalue=None,
                         location=self.getLocation(p, 3),
                         doccomments=doccomments)

    def p_member_method(self, p):
        """member : attributes IDENTIFIER IDENTIFIER '(' paramlist ')' raises ';'"""
        if 'doccomments' in p[1]:
            doccomments = p[1]['doccomments']
        else:
            doccomments = p.slice[2].doccomments

        p[0] = Method(type=p[2],
                      name=p[3],
                      attlist=p[1]['attlist'],
                      paramlist=p[5],
                      location=self.getLocation(p, 3),
                      doccomments=doccomments,
                      raises=p[7])

    def p_paramlist(self, p):
        """paramlist : param moreparams
                     | """
        if len(p) == 1:
            p[0] = []
        else:
            p[0] = list(p[2])
            p[0].insert(0, p[1])

    def p_moreparams_start(self, p):
        """moreparams :"""
        p[0] = []

    def p_moreparams_continue(self, p):
        """moreparams : ',' param moreparams"""
        p[0] = list(p[3])
        p[0].insert(0, p[2])

    def p_param(self, p):
        """param : attributes paramtype IDENTIFIER identifier"""
        p[0] = Param(paramtype=p[2],
                     type=p[3],
                     name=p[4],
                     attlist=p[1]['attlist'],
                     location=self.getLocation(p, 3))

    def p_paramtype(self, p):
        """paramtype : IN
                     | INOUT
                     | OUT"""
        p[0] = p[1]

    def p_optreadonly(self, p):
        """optreadonly : READONLY
                       | """
        if len(p) > 1:
            p[0] = p.slice[1].doccomments
        else:
            p[0] = None

    def p_dictionary(self, p):
        """dictionary : attributes DICTIONARY IDENTIFIER ifacebase dictbody ';'"""
        atts, DICTIONARY, name, base, body, SEMI = p[1:]
        attlist = atts['attlist']
        doccomments = []
        if 'doccomments' in atts:
            doccomments.extend(atts['doccomments'])
        doccomments.extend(p.slice[2].doccomments)

        l = lambda: self.getLocation(p, 2)

        p[0] = Dictionary(name=name,
                          attlist=attlist,
                          base=base,
                          members=body,
                          location=l(),
                          doccomments=doccomments)

    def p_dictbody(self, p):
        """dictbody : '{' dictmembers '}'
                     | """
        if len(p) > 1:
            p[0] = p[2]

    def p_dictmembers_start(self, p):
        """dictmembers : """
        p[0] = []

    def p_dictmembers_continue(self, p):
        """dictmembers : dictmember dictmembers"""
        p[0] = list(p[2])
        p[0].insert(0, p[1])

    def p_dictmember(self, p):
        """dictmember : attributes IDENTIFIER optnullable IDENTIFIER optdefvalue ';'"""
        if 'doccomments' in p[1]:
            doccomments = p[1]['doccomments']
        else:
            doccomments = p.slice[2].doccomments

        p[0] = Attribute(type=p[2],
                         name=p[4],
                         attlist=p[1]['attlist'],
                         readonly=False,
                         nullable=p[3] is not None,
                         defvalue=p[5],
                         location=self.getLocation(p, 1),
                         doccomments=doccomments)

    def p_optnullable(self, p):
        """optnullable : '?'
                       | """
        if len(p) > 1:
            p[0] = p[1]
        else:
            p[0] = None

    def p_optdefvalue(self, p):
        """optdefvalue : '=' STRING
                       | """
        if len(p) > 1:
            p[0] = p[2]
        else:
            p[0] = None

    def p_identifier(self, p):
        """identifier : DICTIONARY
                      | IDENTIFIER"""
        p[0] = p[1]

    def p_raises(self, p):
        """raises : RAISES '(' idlist ')'
                  | """
        if len(p) == 1:
            p[0] = []
        else:
            p[0] = p[3]

    def p_idlist(self, p):
        """idlist : IDENTIFIER"""
        p[0] = [p[1]]

    def p_idlist_continue(self, p):
        """idlist : IDENTIFIER ',' idlist"""
        p[0] = list(p[3])
        p[0].insert(0, p[1])

    def p_error(self, t):
        if not t:
            raise IDLError("Syntax Error at end of file. Possibly due to missing semicolon(;), braces(}) or both", None)
        else:
            location = Location(self.lexer, t.lineno, t.lexpos)
            raise IDLError("invalid syntax", location)

    def __init__(self, outputdir=''):
        self._doccomments = []
        self.lexer = lex.lex(object=self,
                             outputdir=outputdir,
                             lextab='xpidllex',
                             optimize=1)
        self.parser = yacc.yacc(module=self,
                                outputdir=outputdir,
                                debugfile='xpidl_debug',
                                tabmodule='xpidlyacc',
                                optimize=1)

    def clearComments(self):
        self._doccomments = []

    def token(self):
        t = self.lexer.token()
        if t is not None and t.type != 'CDATA':
            t.doccomments = self._doccomments
            self._doccomments = []
        return t

    def parse(self, data, filename=None):
        if filename is not None:
            self.lexer.filename = filename
        self.lexer.lineno = 1
        self.lexer.input(data)
        idl = self.parser.parse(lexer=self)
        if filename is not None:
            idl.deps.append(filename)
        return idl

    def getLocation(self, p, i):
        return Location(self.lexer, p.lineno(i), p.lexpos(i))

if __name__ == '__main__':
    p = IDLParser()
    for f in sys.argv[1:]:
        print "Parsing %s" % f
        p.parse(open(f).read(), filename=f)

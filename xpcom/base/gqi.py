# -*- Mode: Python: tab-width: 8; indent-tabs-mode: nil; python-indent: 4 -*-

"""Implement QueryInterface using optimized generated code."""

import os, errno, re, sys, sets

# Global controlling debug output to sys.stderr
debug = False

def findfile(f, curfile, includedirs):
    """Find a file 'f' by looking in the directory of the current file
    and then in all directories specified by -I until it is found. Returns
    the path of the file that was found.

    @raises IOError is the file is not found."""

    sdirs = [os.path.dirname(curfile)]
    sdirs.extend(includedirs)

    for dir in sdirs:
        t = os.path.join(dir, f)
        if os.path.exists(t):
            return t

    raise IOError(errno.ENOENT, "File not found", f)

# Giant parsing loop: parse input files into the internal representation,
# keeping track of file/line numbering information. This iterator automatically
# skips blank lines and lines beginning with //

class IterFile(object):
    def __init__(self, file):
        self._file = file
        self._iter = open(file)
        self._lineno = 0

    def __iter__(self):
        return self

    def next(self):
        self._lineno += 1
        line = self._iter.next().strip()
        if line == '' or line.startswith('//'):
            return self.next()
        return line

    def loc(self):
        return "%s:%i" % (self._file, self._lineno)

class uniqdict(dict):
    """A subclass of dict that will throw an error if you attempt to set the same key to different values."""
    def __setitem__(self, key, value):
        if key in self and not value == self[key]:
            raise IndexError('Key "%s" already present in uniqdict' % key)

        dict.__setitem__(self, key, value)

    def update(self, d):
        for key, value in d.iteritems():
            if key in self and not value == self[key]:
                raise IndexError('Key "%s" already present in uniqdict' % key)

        dict.update(self, d)

def dump_hex_tuple(t):
    return "(%s)" % ", ".join(["%#x" % i for i in t])

class UUID(object):
    type = 'UUID'

    def __init__(self, name, iidstr, base=None):
        """This method *assumes* that the UUID is validly formed."""
        self.name = name
        self.base = base

        # iid is in 32-16-16-8*8 format, as hex *strings*
        iid = (iidstr[0:8], iidstr[9:13], iidstr[14:18],
               iidstr[19:21], iidstr[21:23], iidstr[24:26], iidstr[26:28],
               iidstr[28:30], iidstr[30:32], iidstr[32:34], iidstr[34:36])

        self.iid = iid

        # (big_endian_words, little_endian_words)
        self.words = (
            (int(iid[0], 16),
             int(iid[1] + iid[2], 16),
             int("".join([iid[i] for i in xrange(3, 7)]), 16),
             int("".join([iid[i] for i in xrange(7, 11)]), 16)),
            (int(iid[0], 16),
             int(iid[2] + iid[1], 16),
             int("".join([iid[i] for i in xrange(6, 2, -1)]), 16),
             int("".join([iid[i] for i in xrange(10, 6, -1)]), 16)))

        if debug:
            print >>sys.stderr, "%r" % self
            print >>sys.stderr, "  bigendian: %s" % dump_hex_tuple(self.words[False])
            print >>sys.stderr, "  littleendian: %r" % dump_hex_tuple(self.words[True])

    def __eq__(self, uuid):
        return self.iid == uuid.iid and self.name == uuid.name

    def __repr__(self):
        return """%s(%r, "%s-%s-%s-%s-%s", base=%r)""" % (
            self.type, self.name,
            self.iid[0], self.iid[1], self.iid[2], self.iid[3] + self.iid[4],
            "".join([self.iid[i] for i in xrange(5, 11)]), self.base)

    def asstruct(self):
        return "{ 0x%s, 0x%s, 0x%s, { 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s } }" % self.iid

    def decl(self):
        return ""

    def ref(self):
        return "NS_GET_IID(%s)" % self.name

class PseudoUUID(UUID):
    type = 'PseudoUUID'

    # No special init method needed

    def decl(self):
        return """
        #ifdef DEBUG
        static const nsID kGQI_%(iname)s = %(struct)s;
        NS_ASSERTION(NS_GET_IID(%(iname)s).Equals(kGQI_%(iname)s),
                     "GQI pseudo-IID doesn't match reality.");
        #endif
        """ % {
            'iname': self.name,
            'struct': self.asstruct()
            }

class CID(UUID):
    type = 'CID'

    def __init__(self, name, iid, decl):
        UUID.__init__(self, name, iid)
        self.pdecl = decl

    def decl(self):
        return """
        static const nsID kGQI_%(pdecl)s = %(struct)s;
        #ifdef DEBUG
        static const nsID kGQI_TEST_%(pdecl)s = %(pdecl)s;
        #endif
        NS_ASSERTION(kGQI_%(pdecl)s.Equals(kGQI_TEST_%(pdecl)s),
                     "GQI pseudo-IID doesn't match reality.");
        """ % {
            'pdecl': self.pdecl,
            'struct': self.asstruct()
            }

    def ref(self):
        return "kGQI_%s" % self.pdecl

_uuid_pattern_string = r'[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}'

_includefinder = re.compile(r'#include\s+(?:"|\<)(?P<filename>[a-z\.\-\_0-9]+)(?:"|\>)\s*$', re.I)
_uuidfinder = re.compile(r'uuid\((?P<iid>' + _uuid_pattern_string + r')\)', re.I)
_namefinder = re.compile(r'interface\s+(?P<interface>[A-Za-z_0-9]+)\s+:\s+(?P<base>[A-Za-z]+)')

def importidl(file, includedirs):
    """Parse an IDL file. Returns (mapofiids, setofdeps)"""

    iids = {}
    deps = sets.Set()
    deps.add(file)

    f = IterFile(file)
    for line in f:
        if line.startswith('%{'):
            # Skip literal IDL blocks such as %{C++ %}
            for line in f:
                if line.startswith('%}'):
                    break
            continue

        m = _includefinder.match(line)
        if m is not None:
            importediids, importeddeps = importidl(findfile(m.group('filename'), file, includedirs), includedirs)
            iids.update(importediids)
            deps |= importeddeps
            continue

        m = _uuidfinder.search(line)
        if m is not None:
            iid = m.group('iid')
            line = f.next()
            if line == 'interface nsISupports {':
                iids['nsISupports'] = UUID('nsISupports', iid)
            else:
                m = _namefinder.match(line)
                if m is None:
                    raise Exception("%s: expected interface" % f.loc())

                iids[m.group('interface')] = \
                    UUID(m.group('interface'), iid, base=m.group('base'))

    return iids, deps

def gqi(ifaces, endian):
    """Find an algorithm that uniquely identifies interfaces by a single
    word. Returns (indexfunction, table)"""
    for bits in xrange(3, 11):
        bitmask = (1 << bits) - 1
        for word in xrange(0, 4):
            for sh in xrange(0, 33 - bits):
                shmask = bitmask << sh

                if debug:
                    print "Trying word: %i, bits: %i, shift: %i" % (word, bits, sh)

                l = list([None for i in xrange(0, 1 << bits)])
                try:
                    for i in xrange(0, len(ifaces)):
                        n = (ifaces[i].uuid.words[endian][word] & shmask) >> sh
                        if l[n] is not None:
                            if debug:
                                print "found conflict, index %i" % n
                                print "old iface: %s" % ifaces[l[n]].uuid
                                print "new iface: %s" % ifaces[i].uuid
                            raise IndexError()

                        l[n] = i

                except IndexError:
                    continue
    
                # If we got here, we're ok... create the table we want
                indexfunc = "(reinterpret_cast<const PRUint32*>(&aIID)[%i] & 0x%x) >> %i" % (word, shmask, sh)
                return indexfunc, l

    raise Exception("No run of 10 bits within a word was unique!: interfaces: %s" % [i.uuid for i in ifaces])

def basecast(item, baselist):
    """Returns a string where an item is static_cast through a list of bases."""
    for base in baselist:
        item = "static_cast<%s*>(%s)" % (base, item)

    return item

class QIImpl(object):
    _default_unfound = """
      *aResult = nsnull;
      return NS_NOINTERFACE;
    """

    _qiimpl = """
    enum %(cname)s_QIActionType {
      %(actionenums)s
    };

    struct %(cname)s_QIAction {
      const nsID *iid;
      %(actiontype)s
      union {
        void *ptr;
        %(actiondata)s
      } data;
    };

    NS_IMETHODIMP %(cname)s::QueryInterface(REFNSIID aIID, void **aResult)
    {
      %(uuiddecls)s

      static const PRUint8 kLookupTable[] = {
#ifdef IS_BIG_ENDIAN
        %(biglookup)s
#else
        %(littlelookup)s
#endif
      };

      static const %(cname)s_QIAction kActionTable[] = {
        %(actiontable)s
      };

      nsISupports* found = nsnull;
      const %(cname)s_QIAction *entry;
      PRUint32 index =
#ifdef IS_BIG_ENDIAN
        %(bigindexfunc)s;
#else
        %(littleindexfunc)s;
#endif

      PRUint8 action = kLookupTable[index];
      if (action == 0xFF)
        goto unfound;

      entry = kActionTable + action;
      if (!entry->iid->Equals(aIID))
        goto unfound;

      %(actioncode)s

      NS_NOTREACHED("Unhandled case?");

      %(nullcheck)s

    exit_addref:
      NS_ASSERTION(found, "Interface should have been found by now.");
      found->AddRef();
      *aResult = found;
      return NS_OK;

    unfound:
      %(unfound)s
    }"""

    _datatypes = """
        %(type)s %(name)s;
    """

    _actiontable_entry = """
        {
          &(%(iref)s),
          %(enumtype)s
          %(emitTable)s
        },
    """

    _actioncode_multi = """
      switch (entry->action) {
      %s
      }
    """

    _actioncode_multi_entry = """
      case %(cname)s_%(enumtype)s:
        %(setResult)s
    """

    _nullcheck = """
    exit_nullcheck_addref:
      if (!found)
        return NS_ERROR_OUT_OF_MEMORY;
    """

    _refcounting = """
    NS_IMPL_%(threadsafe)sADDREF(%(cname)s)
    NS_IMPL_%(threadsafe)sRELEASE(%(cname)s)
    """

    def __init__(self, cname, ifaces, emitrefcount=False,
                 threadsafe="", unfound=None, base=None):
        self.cname = cname
        self.emitrefcount = emitrefcount
        self.threadsafe = threadsafe
        self.base = base
        self.ifaces = ifaces
        self.unfound = unfound

    def iter_self_and_bases(self):
        """yields (impl, baselist) for all bases"""
        impl = self
        baselist = []
        
        while impl is not None:
            baselist = list(baselist)
            baselist.append(impl.cname)
            yield impl, baselist
            impl = impl.base

    def iter_all_ifaces(self):
        """yields (iface, baselist) for all interfaces"""
        for impl, baselist in self.iter_self_and_bases():
            for i in impl.ifaces:
                if len(baselist) == 1 or i.inherit:
                    yield i, baselist

    def output(self, fd):
        unfound = None
        for impl, baselist in self.iter_self_and_bases():
            if impl.unfound is not None:
                unfound = impl.unfound
                break

        if unfound is None:
            unfound = self._default_unfound

        needsnullcheck = False
        actions = sets.Set()
        types = uniqdict()
        for i, baselist in self.iter_all_ifaces():
            action = i.action
            actions.add(action)
            if action.needsnullcheck:
                needsnullcheck = True

            if action.datatype:
                types[action.varname] = action.datatype

        actionenums = ", ".join(["%s_%s" % (self.cname, action.enumtype)
                                 for action in actions])

        # types.ptr is explicitly first (for initialization), so delete it
        types.pop('ptr', None)

        actiondata = "".join([self._datatypes % {'type': type,
                                                 'name': name}
                              for name, type in types.iteritems()])

        # ensure that interfaces are not duplicated
        # maps name -> (uuid, baselist)
        ifdict = uniqdict()
        for i, baselist in self.iter_all_ifaces():
            if i.uuid.name in ifdict:
                print >>sys.stderr, "warning: interface '%s' from derived class '%s' overrides base class '%s'" % (i.uuid.name, baselist[0], baselist[len(baselist) - 1])
            else:
                ifdict[i.uuid.name] = (i, baselist)

        # list of (i, baselist)
        ilist = [i for i in ifdict.itervalues()]

        uuiddecls = "".join([i.uuid.decl()
                             for i, baselist in ilist])

        bigindexfunc, bigitable = gqi([iface for iface, baselist in ilist], False)
        littleindexfunc, littleitable = gqi([iface for iface, baselist in ilist], True)

        # (falseval, trueval)[conditional expression] is the python
        # ternary operator
        biglookup = ",\n".join([(str(i), "0xFF")[i is None]
                                for i in bigitable])
        littlelookup = ",\n".join([(str(i), "0xFF")[i is None]
                                   for i in littleitable])

        if len(actions) > 1:
            actiontype = "%s_QIActionType action;" % self.cname
            enumtype = "%(cname)s_%(enumtype)s,"
        else:
            actiontype = ""
            enumtype = ""

        actiontable = "".join([self._actiontable_entry %
               {'enumtype': enumtype % {'cname': self.cname, 
                                        'enumtype': iface.action.enumtype},
                'iname': iface.uuid.name,
                'iref': iface.uuid.ref(),
                'emitTable': iface.emitTable(self.cname, baselist)}
                               for iface, baselist in ilist])

        if len(actions) == 1:
            actioncode = iter(actions).next().setResult % {
                'classname': self.cname
                }
        else:
            actioncode = self._actioncode_multi % (
                "".join([self._actioncode_multi_entry % {
                            'cname': self.cname,
                            'enumtype': action.enumtype,
                            'setResult': action.setResult % {'classname': self.cname}}
                         for action in actions])
                )
                
        if needsnullcheck:
            nullcheck = self._nullcheck
        else:
            nullcheck = ""

        print >>fd, self._qiimpl % {
            'cname': self.cname,
            'actionenums': actionenums,
            'actiontype': actiontype,
            'actiondata': actiondata,
            'uuiddecls': uuiddecls,
            'biglookup': biglookup,
            'littlelookup': littlelookup,
            'actiontable': actiontable,
            'bigindexfunc': bigindexfunc,
            'littleindexfunc': littleindexfunc,
            'actioncode': actioncode,
            'nullcheck': nullcheck,
            'unfound': unfound
            }

        if self.emitrefcount:
            print >>fd, self._refcounting % {
                'threadsafe': self.threadsafe,
                'cname': self.cname
                }

class PointerAction(object):
    enumtype = "STATIC_POINTER"
    datatype = "void*"
    varname  = "ptr"
    setResult = """*aResult = entry->data.ptr;
                   return NS_OK;"""
    needsnullcheck = False

nsCycleCollectionParticipant = PseudoUUID("nsCycleCollectionParticipant", "9674489b-1f6f-4550-a730-ccaedd104cf9")

class CCParticipantResponse(object):
    action = PointerAction
    inherit = True

    def __init__(self, classname):
        self.classname = classname
        self.uuid = nsCycleCollectionParticipant

    def emitTable(self, classname, baselist):
        return "{ &NS_CYCLE_COLLECTION_NAME(%s) }" % classname

class TearoffResponse(object):
    """Because each tearoff is different, this response is its own action."""
    datatype = None
    needsnullcheck = True
    inherit = True

    def __init__(self, uuid, allocator):
        self.setResult = """
            found = static_cast<%s*>(%s);
            goto exit_nullcheck_addref;
    """ % (uuid.name, allocator)
        self.uuid = uuid
        self.action = self
        self.enumtype = "NS_TEAROFF_%s" % uuid.name

    def emitTable(self, classname, baselist):
        return "{0}"

class ConditionalResponse(object):
    """Because each condition is different, this response is its own action."""
    datatype = None
    needsnullcheck = False
    inherit = True

    def __init__(self, uuid, condition):
        self.setResult = """
        if (!(%s)) {
            goto unfound;
        }
        found = static_cast<%s*>(this);
        goto exit_addref;
        """ % (condition, uuid.name);

        self.uuid = uuid
        self.action = self
        self.enumtype = "NS_CONDITIONAL_%s" % uuid.name

    def emitTable(self, classname, baselist):
        return "{0}"

nsCycleCollectionISupports = PseudoUUID("nsCycleCollectionISupports", "c61eac14-5f7a-4481-965e-7eaa6effa85f")

class CCSupportsAction(object):
    enumtype = "CC_ISUPPORTS"
    datatype = None
    setResult = """
        found = NS_CYCLE_COLLECTION_CLASSNAME(%(classname)s)::Upcast(this);
        goto exit_nullcheck_addref;
    """
    needsnullcheck = True

class CCISupportsResponse(object):
    action = CCSupportsAction
    inherit = True

    def __init__(self, classname):
        self.classname = classname
        self.uuid = nsCycleCollectionISupports

    def emitTable(self, classname, baselist):
        return "{ 0 }"

nsIClassInfo = PseudoUUID("nsIClassInfo", "986c11d0-f340-11d4-9075-0010a4e73d9a")

class DOMCIResult(object):
    datatype = None
    enumtype = "DOMCI"
    needsnullcheck = True
    inherit = False

    def __init__(self, domciname):
        self.action = self
        self.uuid = nsIClassInfo
        self.setResult = """
      found = NS_GetDOMClassInfoInstance(eDOMClassInfo_%s_id);
      goto exit_nullcheck_addref;
    """ % domciname

    def emitTable(self, classname, baselist):
        return "{ 0 }"

class OffsetAction(object):
    enumtype = "OFFSET_THIS_ADDREF"
    datatype = "PRUint32"
    varname  = "offset"
    setResult = """
        found = reinterpret_cast<nsISupports*>(reinterpret_cast<char*>(this) + entry->data.offset);
        goto exit_addref;
    """
    needsnullcheck = False

class OffsetThisQIResponse(object):
    action = OffsetAction
    inherit = True

    def __init__(self, uuid):
        self.uuid = uuid

    def emitTable(self, classname, baselist):
        casted = basecast("((%s*) 0x1000)" % classname, baselist)

        return """
   { reinterpret_cast<void*>(
       reinterpret_cast<char*>(
         static_cast<%(iname)s*>(%(casted)s)) -
       reinterpret_cast<char*>(%(casted)s))
   }""" % { 'casted': casted,
            'iname': self.uuid.name }

class OffsetFutureThisQIResponse(OffsetThisQIResponse):
    action = OffsetAction
    inherit = True

    def __init__(self, uuid):
        self.uuid = uuid

    def emitTable(self, classname, baselist):
        casted = "((%s*) 0x1000)" % classname

        return """
   { reinterpret_cast<void*>(
       reinterpret_cast<char*>(
         static_cast<%(iname)s*>(%(casted)s)) -
       reinterpret_cast<char*>(%(casted)s))
   }""" % { 'casted': casted,
            'iname': self.uuid.name }

class OffsetThisQIResponseAmbiguous(OffsetThisQIResponse):
    def __init__(self, uuid, intermediate):
        self.intermediate = intermediate
        OffsetThisQIResponse.__init__(self, uuid)

    def emitTable(self, classname, baselist):
        casted = basecast("((%s*) 0x1000)" % classname, baselist)

        return """
   { reinterpret_cast<void*>(
       reinterpret_cast<char*>(
         static_cast<%(iname)s*>(
           static_cast<%(intermediate)s*>(%(casted)s))) -
       reinterpret_cast<char*>(%(casted)s))
   }""" % \
            { 'casted': casted,
              'iname': self.uuid.name,
              'intermediate': self.intermediate.name }

class LiteralAction(object):
    datatype = None
    def __init__(self, uname, code):
        self.enumtype = "LITERAL_CODE_%s" % uname
        self.setResult = code
        self.needsnullcheck = code.find('exit_nullcheck_addref') != -1

class LiteralResponse(object):
    inherit = True

    def __init__(self, action, uuid):
        self.action = action
        self.uuid = uuid

    def emitTable(self, cname, baselist):
        return "{0}"

_map_entry = re.compile(r'(?P<action>[A-Z_]+)\((?P<list>.*)\)$')
_split_commas = re.compile(r'\s*,\s*')

def build_map(f, cname, iids):
    """Parse the body of an NS_INTERFACE_MAP and return (members, unfound)"""
    unfound = None
    members = []
    for line in f:
        if line == 'NS_INTERFACE_MAP_END':
            return (members, unfound)

        if line == 'NS_INTERFACE_MAP_UNFOUND':
            unfound = ''
            for line in f:
                if line == 'END':
                    break
                unfound += line
            continue

        if line.find(')') == -1:
            for line2 in f:
                line += line2
                if line.find(')') != -1:
                    break

        m = _map_entry.match(line)
        if m is None:
            raise Exception("%s: Unparseable interface map entry" % f.loc())

        items = _split_commas.split(m.group('list'))

        action = m.group('action')
        if action == 'NS_INTERFACE_MAP_ENTRY':
            iname, = items
            members.append(OffsetThisQIResponse(iids[iname]))
        elif action == 'NS_FUTURE_INTERFACE_MAP_ENTRY':
            iname, = items
            members.append(OffsetFutureThisQIResponse(iids[iname]))
        elif action == 'NS_INTERFACE_MAP_ENTRY_AMBIGUOUS':
            iname, intermediate = items
            members.append(OffsetThisQIResponseAmbiguous(iids[iname], iids[intermediate]))
        elif action == 'NS_INTERFACE_MAP_ENTRY_TEAROFF':
            iname, allocator = items
            members.append(TearoffResponse(iids[iname], allocator))
        elif action == 'NS_INTERFACE_MAP_ENTRY_CONDITIONAL':
            iname, condition = items
            members.append(ConditionalResponse(iids[iname], condition))
        elif action == 'NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO':
            domciname, = items
            members.append(DOMCIResult(domciname))
        elif action == 'NS_INTERFACE_MAP_ENTRY_LITERAL':
            code = ''
            for line in f:
                if line == 'END':
                    break
                code += line
            
            a = LiteralAction(items[0], code)
            for iname in items:
                iface = iids[iname]
                members.append(LiteralResponse(a, iface))
        else:
            raise Exception("%s: Unexpected interface map entry" % f.loc())

    raise Exception("%s: Unexpected EOF" % f.loc())

_import = re.compile(r'%(?P<type>import|import-idl)\s+(?:"|\<)(?P<filename>[a-z\.\-\_0-9]+)(?:"|\>)\s*$', re.I)
_impl_qi = re.compile(r'NS_IMPL_(?P<threadsafe>THREADSAFE_)?(?P<type>QUERY_INTERFACE|ISUPPORTS)(?P<inherited>_INHERITED)?(?:\d+)\((?P<bases>.*)\)\s*$')
_pseudoiid = re.compile(r'%pseudo-iid\s+(?P<name>[a-z_0-9]+)\s+(?P<iid>' + _uuid_pattern_string + r')\s*$', re.I)
_pseudocid = re.compile(r'%pseudo-cid\s+(?P<name>[a-z_0-9]+)\s+(?P<iid>' + _uuid_pattern_string + r')\s+(?P<decl>[a-z_0-9]+)$', re.I)
_map_begin = re.compile(r'NS_INTERFACE_MAP_BEGIN(?P<cc>_CYCLE_COLLECTION)?\((?P<classname>[A-Za-z0-9+]+)(?:\s*,\s*(?P<base>[A-Za-z0-9+]+))?\)$')

def parsefile(file, fd, includedirs):
    """Parse a file, returning a (map of name->QIImpls, set of deps)
    %{C++ blocks are printed immediately to fd."""

    iids = uniqdict()
    impls = uniqdict()
    imported = uniqdict()
    deps = sets.Set()
    deps.add(file)

    f = IterFile(file)
    for line in f:
        if len(line) == 0:
            continue

        if line == "%{C++":
            for line in f:
                if line == "%}":
                    break
                print >>fd, line
            continue

        m = _pseudoiid.match(line)
        if m is not None:
            uuid = PseudoUUID(m.group('name'), m.group('iid'))
            iids[m.group('name')] = uuid
            continue

        m = _pseudocid.match(line)
        if m is not None:
            uuid = CID(m.group('name'), m.group('iid'), m.group('decl'))
            iids[m.group('decl')] = uuid
            continue

        m = _import.match(line)
        if m is not None:
            if m.group('type') == 'import':
                newimpls, newdeps = parsefile(findfile(m.group('filename'), file, includedirs), fd, includedirs)
                imported.update(newimpls)
                deps |= newdeps
            else:
                newiids, newdeps = importidl(findfile(m.group('filename'), file, includedirs), includedirs)
                iids.update(newiids)
                deps |= newdeps
                print >>fd, '#include "%s"' % m.group('filename').replace('.idl', '.h')
            continue

        if line.find('NS_IMPL_') != -1:
            if line.find(')') == -1:
                for follow in f:
                    line += follow
                    if follow.find(')') != -1:
                        break

                if line.find(')') == -1:
                    raise Exception("%s: Unterminated NS_IMPL_ call" % f.loc())

            m = _impl_qi.match(line)
            if m is None:
                raise Exception("%s: Unparseable NS_IMPL_ call" % f.loc())

            bases = _split_commas.split(m.group('bases'))
            cname = bases.pop(0)
            if m.group('inherited'):
                inherited = bases.pop(0)
                if inherited in impls:
                    base = impls[inherited]
                else:
                    base = imported[inherited]
            else:
                base = None

            baseuuids = [iids[name] for name in bases]
            ifaces = [OffsetThisQIResponse(uuid) for uuid in baseuuids]
            ifaces.append(OffsetThisQIResponseAmbiguous(iids['nsISupports'], ifaces[0].uuid))
            q = QIImpl(cname, ifaces,
                       emitrefcount=(m.group('type') == 'ISUPPORTS'),
                       threadsafe=m.group('threadsafe') or '',
                       base=base)
            impls[cname] = q
            continue

        m = _map_begin.match(line)
        if m is not None:
            members, unfound = build_map(f, m.group('classname'), iids)
            if m.group('cc') is not None:
                members.append(CCParticipantResponse(m.group('classname')))
                members.append(CCISupportsResponse(m.group('classname')))
            base = None
            if m.group('base') is not None:
                if m.group('base') in impls:
                    base = impls[m.group('base')]
                else:
                    base = imported[m.group('base')]
            q = QIImpl(m.group('classname'), members, unfound=unfound, base=base)
            impls[m.group('classname')] = q
            continue

        raise Exception("%s: unexpected input line" % f.loc())

    return impls, deps

def main():
    from optparse import OptionParser

    o = OptionParser()
    o.add_option("-I", action="append", dest="include_dirs", default=[],
                 help="Directory to search for included files.")
    o.add_option("-D", dest="deps_file",
                 help="Write dependencies to a file")
    o.add_option("-o", dest="out_file",
                 help="Write output to file. Required")
    o.add_option("-v", dest="verbose", action="store_true",
                 help="Print verbose debugging output.")

    (options, files) = o.parse_args()

    if options.verbose:
        global debug
        debug = True

    if options.out_file is None:
        o.print_help()
        sys.exit(1)

    outfd = open(options.out_file, 'w')

    deps = sets.Set()

    for file in files:
        impls, newdeps = parsefile(file, outfd, options.include_dirs)
        for q in impls.itervalues():
            q.output(outfd)
        deps |= newdeps

        if options.deps_file is not None:
            depsfd = open(options.deps_file, 'w')
            print >>depsfd, "%s: %s" % (options.out_file,
                                        " \\\n   ".join(deps))

if __name__ == '__main__':
    main()


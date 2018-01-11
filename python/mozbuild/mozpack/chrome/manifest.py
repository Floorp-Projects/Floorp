# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import re
import os
from urlparse import urlparse
import mozpack.path as mozpath
from mozpack.chrome.flags import Flags
from mozpack.errors import errors


class ManifestEntry(object):
    '''
    Base class for all manifest entry types.
    Subclasses may define the following class or member variables:
        - localized: indicates whether the manifest entry is used for localized
          data.
        - type: the manifest entry type (e.g. 'content' in
          'content global content/global/')
        - allowed_flags: a set of flags allowed to be defined for the given
          manifest entry type.

    A manifest entry is attached to a base path, defining where the manifest
    entry is bound to, and that is used to find relative paths defined in
    entries.
    '''
    localized = False
    type = None
    allowed_flags = [
        'application',
        'platformversion',
        'os',
        'osversion',
        'abi',
        'xpcnativewrappers',
        'tablet',
        'process',
        'contentaccessible',
    ]

    def __init__(self, base, *flags):
        '''
        Initialize a manifest entry with the given base path and flags.
        '''
        self.base = base
        self.flags = Flags(*flags)
        if not all(f in self.allowed_flags for f in self.flags):
            errors.fatal('%s unsupported for %s manifest entries' %
                         (','.join(f for f in self.flags
                          if not f in self.allowed_flags), self.type))

    def serialize(self, *args):
        '''
        Serialize the manifest entry.
        '''
        entry = [self.type] + list(args)
        flags = str(self.flags)
        if flags:
            entry.append(flags)
        return ' '.join(entry)

    def __eq__(self, other):
        return self.base == other.base and str(self) == str(other)

    def __ne__(self, other):
        return not self.__eq__(other)

    def __repr__(self):
        return '<%s@%s>' % (str(self), self.base)

    def move(self, base):
        '''
        Return a new manifest entry with a different base path.
        '''
        return parse_manifest_line(base, str(self))

    def rebase(self, base):
        '''
        Return a new manifest entry with all relative paths defined in the
        entry relative to a new base directory.
        The base class doesn't define relative paths, so it is equivalent to
        move().
        '''
        return self.move(base)


class ManifestEntryWithRelPath(ManifestEntry):
    '''
    Abstract manifest entry type with a relative path definition.
    '''
    def __init__(self, base, relpath, *flags):
        ManifestEntry.__init__(self, base, *flags)
        self.relpath = relpath

    def __str__(self):
        return self.serialize(self.relpath)

    def rebase(self, base):
        '''
        Return a new manifest entry with all relative paths defined in the
        entry relative to a new base directory.
        '''
        clone = ManifestEntry.rebase(self, base)
        clone.relpath = mozpath.rebase(self.base, base, self.relpath)
        return clone

    @property
    def path(self):
        return mozpath.normpath(mozpath.join(self.base,
                                                       self.relpath))


class Manifest(ManifestEntryWithRelPath):
    '''
    Class for 'manifest' entries.
        manifest some/path/to/another.manifest
    '''
    type = 'manifest'


class ManifestChrome(ManifestEntryWithRelPath):
    '''
    Abstract class for chrome entries.
    '''
    def __init__(self, base, name, relpath, *flags):
        ManifestEntryWithRelPath.__init__(self, base, relpath, *flags)
        self.name = name

    @property
    def location(self):
        return mozpath.join(self.base, self.relpath)


class ManifestContent(ManifestChrome):
    '''
    Class for 'content' entries.
        content global content/global/
    '''
    type = 'content'
    allowed_flags = ManifestChrome.allowed_flags + [
        'contentaccessible',
        'platform',
    ]

    def __str__(self):
        return self.serialize(self.name, self.relpath)


class ManifestMultiContent(ManifestChrome):
    '''
    Abstract class for chrome entries with multiple definitions.
    Used for locale and skin entries.
    '''
    type = None

    def __init__(self, base, name, id, relpath, *flags):
        ManifestChrome.__init__(self, base, name, relpath, *flags)
        self.id = id

    def __str__(self):
        return self.serialize(self.name, self.id, self.relpath)


class ManifestLocale(ManifestMultiContent):
    '''
    Class for 'locale' entries.
        locale global en-US content/en-US/
        locale global fr content/fr/
    '''
    localized = True
    type = 'locale'


class ManifestSkin(ManifestMultiContent):
    '''
    Class for 'skin' entries.
        skin global classic/1.0 content/skin/classic/
    '''
    type = 'skin'


class ManifestOverload(ManifestEntry):
    '''
    Abstract class for chrome entries defining some kind of overloading.
    Used for overlay, override or style entries.
    '''
    type = None

    def __init__(self, base, overloaded, overload, *flags):
        ManifestEntry.__init__(self, base, *flags)
        self.overloaded = overloaded
        self.overload = overload

    def __str__(self):
        return self.serialize(self.overloaded, self.overload)


class ManifestOverlay(ManifestOverload):
    '''
    Class for 'overlay' entries.
        overlay chrome://global/content/viewSource.xul \
            chrome://browser/content/viewSourceOverlay.xul
    '''
    type = 'overlay'


class ManifestStyle(ManifestOverload):
    '''
    Class for 'style' entries.
        style chrome://global/content/viewSource.xul \
            chrome://browser/skin/
    '''
    type = 'style'


class ManifestOverride(ManifestOverload):
    '''
    Class for 'override' entries.
        override chrome://global/locale/netError.dtd \
            chrome://browser/locale/netError.dtd
    '''
    type = 'override'


class ManifestResource(ManifestEntry):
    '''
    Class for 'resource' entries.
        resource gre-resources toolkit/res/
        resource services-sync resource://gre/modules/services-sync/

    The target may be a relative path or a resource or chrome url.
    '''
    type = 'resource'

    def __init__(self, base, name, target, *flags):
        ManifestEntry.__init__(self, base, *flags)
        self.name = name
        self.target = target

    def __str__(self):
        return self.serialize(self.name, self.target)

    def rebase(self, base):
        u = urlparse(self.target)
        if u.scheme and u.scheme != 'jar':
            return ManifestEntry.rebase(self, base)
        clone = ManifestEntry.rebase(self, base)
        clone.target = mozpath.rebase(self.base, base, self.target)
        return clone


class ManifestBinaryComponent(ManifestEntryWithRelPath):
    '''
    Class for 'binary-component' entries.
        binary-component some/path/to/a/component.dll
    '''
    type = 'binary-component'


class ManifestComponent(ManifestEntryWithRelPath):
    '''
    Class for 'component' entries.
        component {b2bba4df-057d-41ea-b6b1-94a10a8ede68} foo.js
    '''
    type = 'component'

    def __init__(self, base, cid, file, *flags):
        ManifestEntryWithRelPath.__init__(self, base, file, *flags)
        self.cid = cid

    def __str__(self):
        return self.serialize(self.cid, self.relpath)


class ManifestInterfaces(ManifestEntryWithRelPath):
    '''
    Class for 'interfaces' entries.
        interfaces foo.xpt
    '''
    type = 'interfaces'


class ManifestCategory(ManifestEntry):
    '''
    Class for 'category' entries.
        category command-line-handler m-browser @mozilla.org/browser/clh;
    '''
    type = 'category'

    def __init__(self, base, category, name, value, *flags):
        ManifestEntry.__init__(self, base, *flags)
        self.category = category
        self.name = name
        self.value = value

    def __str__(self):
        return self.serialize(self.category, self.name, self.value)


class ManifestContract(ManifestEntry):
    '''
    Class for 'contract' entries.
        contract @mozilla.org/foo;1 {b2bba4df-057d-41ea-b6b1-94a10a8ede68}
    '''
    type = 'contract'

    def __init__(self, base, contractID, cid, *flags):
        ManifestEntry.__init__(self, base, *flags)
        self.contractID = contractID
        self.cid = cid

    def __str__(self):
        return self.serialize(self.contractID, self.cid)

# All manifest classes by their type name.
MANIFESTS_TYPES = dict([(c.type, c) for c in globals().values()
                       if type(c) == type and issubclass(c, ManifestEntry)
                       and hasattr(c, 'type') and c.type])

MANIFEST_RE = re.compile(r'^#.*$')


def parse_manifest_line(base, line):
    '''
    Parse a line from a manifest file with the given base directory and
    return the corresponding ManifestEntry instance.
    '''
    # Remove comments
    cmd = MANIFEST_RE.sub('', line).strip().split()
    if not cmd:
        return None
    if not cmd[0] in MANIFESTS_TYPES:
        return errors.fatal('Unknown manifest directive: %s' % cmd[0])
    return MANIFESTS_TYPES[cmd[0]](base, *cmd[1:])


def parse_manifest(root, path, fileobj=None):
    '''
    Parse a manifest file.
    '''
    base = mozpath.dirname(path)
    if root:
        path = os.path.normpath(os.path.abspath(os.path.join(root, path)))
    if not fileobj:
        fileobj = open(path)
    linenum = 0
    for line in fileobj:
        linenum += 1
        with errors.context(path, linenum):
            e = parse_manifest_line(base, line)
            if e:
                yield e


def is_manifest(path):
    '''
    Return whether the given path is that of a manifest file.
    '''
    return path.endswith('.manifest') and not path.endswith('.CRT.manifest') \
        and not path.endswith('.exe.manifest')

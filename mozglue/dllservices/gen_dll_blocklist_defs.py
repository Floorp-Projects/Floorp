# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

from copy import deepcopy
from six import (iteritems, PY2)
from struct import unpack
import os
from uuid import UUID

H_HEADER = """/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This file was auto-generated from {0} by gen_dll_blocklist_data.py.  */

#ifndef mozilla_{1}_h
#define mozilla_{1}_h

"""

H_FOOTER = """#endif  // mozilla_{1}_h

"""

H_DEFS_BEGIN_DEFAULT = """#include "mozilla/WindowsDllBlocklistCommon.h"

DLL_BLOCKLIST_DEFINITIONS_BEGIN

"""

H_DEFS_END_DEFAULT = """
DLL_BLOCKLIST_DEFINITIONS_END

"""

H_BEGIN_LSP = """#include <guiddef.h>

static const GUID gLayerGuids[] = {

"""

H_END_LSP = """
};

"""

H_BEGIN_A11Y = """#include "mozilla/WindowsDllBlocklistCommon.h"

DLL_BLOCKLIST_DEFINITIONS_BEGIN_NAMED(gBlockedInprocDlls)

"""

# These flag names should match the ones defined in WindowsDllBlocklistCommon.h
FLAGS_DEFAULT = 'FLAGS_DEFAULT'
BLOCK_WIN8PLUS_ONLY = 'BLOCK_WIN8PLUS_ONLY'
BLOCK_WIN8_ONLY = 'BLOCK_WIN8_ONLY'
USE_TIMESTAMP = 'USE_TIMESTAMP'
CHILD_PROCESSES_ONLY = 'CHILD_PROCESSES_ONLY'
BROWSER_PROCESS_ONLY = 'BROWSER_PROCESS_ONLY'
SUBSTITUTE_LSP_PASSTHROUGH = 'SUBSTITUTE_LSP_PASSTHROUGH'

# Only these flags are available in the input script
INPUT_ONLY_FLAGS = {
    BLOCK_WIN8PLUS_ONLY,
    BLOCK_WIN8_ONLY,
}


def FILTER_ALLOW_ALL(entry):
    # A11y entries are special, so we always exclude those by default
    # (so it's not really allowing 'all', but it is simpler to reason about by
    #  pretending that it is allowing all.)
    return not isinstance(entry, A11yBlocklistEntry)


def FILTER_DENY_ALL(entry):
    return False


def FILTER_ALLOW_ONLY_A11Y(entry):
    return isinstance(entry, A11yBlocklistEntry)


def FILTER_ALLOW_ONLY_LSP(entry):
    return isinstance(entry, LspBlocklistEntry)


def FILTER_TESTS_ONLY(entry):
    return not isinstance(entry, A11yBlocklistEntry) and entry.is_test()


def derive_test_key(key):
    return key + '_TESTS'


ALL_DEFINITION_LISTS = ('ALL_PROCESSES', 'BROWSER_PROCESS', 'CHILD_PROCESSES')


class BlocklistDescriptor(object):
    """This class encapsulates every file that is output from this script.
    Each instance has a name, an "input specification", and optional "flag
    specification" and "output specification" entries.
    """

    DEFAULT_OUTSPEC = {
        'mode': '',
        'filter': FILTER_ALLOW_ALL,
        'begin': H_DEFS_BEGIN_DEFAULT,
        'end': H_DEFS_END_DEFAULT,
    }

    FILE_NAME_TPL = 'WindowsDllBlocklist{0}Defs'

    OutputDir = None
    ExistingFd = None
    ExistingFdLeafName = None

    def __init__(self, name, inspec, **kwargs):
        """Positional arguments:

        name -- String containing the name of the output list.

        inspec -- One or more members of ALL_DEFINITION_LISTS. The input used
        for this blocklist file is the union of all lists specified by this
        variable.

        Keyword arguments:

        outspec -- an optional list of dicts that specify how the lists output
        will be written out to a file. Each dict may contain the following keys:

            'mode' -- a string that specifies a mode that is used when writing
            out list entries to this particular output. This is passed in as the
            mode argument to DllBlocklistEntry's write method.

            'filter' -- a function that, given a blocklist entry, decides
            whether or not that entry shall be included in this output file.

            'begin' -- a string that is written to the output file after writing
            the file's header, but prior to writing out any blocklist entries.

            'end' -- a string that is written to the output file after writing
            out any blocklist entries but before the file's footer.

        Any unspecified keys will be assigned default values.

        flagspec -- an optional dict whose keys consist of one or more of the
        list names from inspec. Each corresponding value is a set of flags that
        should be applied to each entry from that list. For example, the
        flagspec:

            {'CHILD_PROCESSES': {CHILD_PROCESSES_ONLY}}

        causes any blocklist entries from the CHILD_PROCESSES list to be output
        with the CHILD_PROCESSES_ONLY flag set.

        """

        self._name = name

        # inspec's elements must all come from ALL_DEFINITION_LISTS
        assert not (set(inspec).difference(set(ALL_DEFINITION_LISTS)))

        # Internally to the descriptor, we store input specifications as a dict
        # that maps each input blocklist name to the set of flags to be applied
        # to each entry in that blocklist.
        self._inspec = {blocklist: set() for blocklist in inspec}

        self._outspecs = kwargs.get('outspec',
                                    BlocklistDescriptor.DEFAULT_OUTSPEC)
        if isinstance(self._outspecs, dict):
            # _outspecs should always be stored as a list of dicts
            self._outspecs = [self._outspecs]

        flagspecs = kwargs.get('flagspec', dict())
        # flagspec's keys must all come from ALL_DEFINITION_LISTS
        assert not (set(flagspecs.keys()).difference(set(self._inspec.keys())))

        # Merge the flags from flagspec into _inspec's sets
        for blocklist, flagspec in iteritems(flagspecs):
            spec = self._inspec[blocklist]
            if not isinstance(spec, set):
                raise TypeError('Flag spec for list %s must be a set!' % blocklist)
            spec.update(flagspec)

    @staticmethod
    def set_output_fd(fd):
        """The build system has already provided an open file descriptor for
        one of our outputs. We save that here so that we may use that fd once
        we're ready to process that fd's file. We also obtain the output dir for
        use as the base directory for the other output files that we open.
        """
        BlocklistDescriptor.ExistingFd = fd
        (BlocklistDescriptor.OutputDir,
         BlocklistDescriptor.ExistingFdLeafName) = os.path.split(fd.name)

    @staticmethod
    def ensure_no_dupes(defs):
        """Ensure that defs does not contain any dupes. We raise a ValueError
        because this is a bug in the input and requires developer intervention.
        """
        seen = set()
        for e in defs:
            name = e.get_name()
            if name not in seen:
                seen.add(name)
            else:
                raise ValueError('Duplicate entry found: %s' % name)

    @staticmethod
    def get_test_entries(exec_env, blocklist):
        """Obtains all test entries for the corresponding blocklist, and also
        ensures that each entry has its test flag set.

        Positional arguments:

        exec_env -- dict containing the globals that were passed to exec to
        when the input script was run.

        blocklist -- The name of the blocklist to obtain tests from. This
        should be one of the members of ALL_DEFINITION_LISTS
        """
        test_key = derive_test_key(blocklist)

        def set_is_test(elem):
            elem.set_is_test()
            return elem

        return map(set_is_test, exec_env[test_key])

    def gen_list(self, exec_env, filter_func):
        """Generates a sorted list of blocklist entries from the input script,
        filtered via filter_func.

        Positional arguments:

        exec_env -- dict containing the globals that were passed to exec to
        when the input script was run. This function expects exec_env to
        contain lists of blocklist entries, keyed using one of the members of
        ALL_DEFINITION_LISTS.

        filter_func -- a filter function that evaluates each blocklist entry
        to determine whether or not it should be included in the results.
        """

        # This list contains all the entries across all blocklists that we
        # potentially want to process
        unified_list = []

        # For each blocklist specified in the _inspec, we query the globals
        # for their entries, add any flags, and then add them to the
        # unified_list.
        for blocklist, listflags in iteritems(self._inspec):
            def add_list_flags(elem):
                # We deep copy so that flags set for an entry in one blocklist
                # do not interfere with flags set for an entry in a different
                # list.
                result = deepcopy(elem)
                result.add_flags(listflags)
                return result
            # We add list flags *before* filtering because the filters might
            # want to access flags as part of their operation.
            unified_list.extend(map(add_list_flags, exec_env[blocklist]))

            # We also add any test entries for the lists specified by _inspec
            unified_list.extend(
                map(add_list_flags,
                    self.get_test_entries(exec_env, blocklist)))

        # There should be no dupes in the input. If there are, raise an error.
        self.ensure_no_dupes(unified_list)

        # Now we filter out any unwanted list entries
        filtered_list = filter(filter_func, unified_list)

        # Sort the list on entry name so that the blocklist code may use
        # binary search if it so chooses.
        return sorted(filtered_list, key=lambda e: e.get_name())

    @staticmethod
    def get_fd(outspec_leaf_name):
        """If BlocklistDescriptor.ExistingFd corresponds to outspec_leaf_name,
        then we return that. Otherwise, we construct a new absolute path to
        outspec_leaf_name and open a new file descriptor for writing.
        """
        if (not BlocklistDescriptor.ExistingFd or
            BlocklistDescriptor.ExistingFdLeafName != outspec_leaf_name):
            new_name = os.path.join(BlocklistDescriptor.OutputDir,
                                    outspec_leaf_name)
            return open(new_name, 'w')

        fd = BlocklistDescriptor.ExistingFd
        BlocklistDescriptor.ExistingFd = None
        return fd

    def write(self, src_file, exec_env):
        """Write out all output files that are specified by this descriptor.

        Positional arguments:

        src_file -- name of the input file from which the lists were generated.

        exec_env -- dictionary containing the lists that were parsed from the
        input file when it was executed.
        """

        for outspec in self._outspecs:
            # Use DEFAULT_OUTSPEC to supply defaults for any unused outspec keys
            effective_outspec = BlocklistDescriptor.DEFAULT_OUTSPEC.copy()
            effective_outspec.update(outspec)

            entries = self.gen_list(exec_env, effective_outspec['filter'])
            if not entries:
                continue

            mode = effective_outspec['mode']

            # Since each output descriptor may generate output across multiple
            # modes, each list is uniquified via the concatenation of our name
            # and the mode.
            listname = self._name + mode
            leafname_no_ext = BlocklistDescriptor.FILE_NAME_TPL.format(listname)
            leafname = leafname_no_ext + '.h'

            with self.get_fd(leafname) as output:
                print(H_HEADER.format(src_file, leafname_no_ext), file=output,
                      end='')
                print(effective_outspec['begin'], file=output, end='')

                for e in entries:
                    e.write(output, mode)

                print(effective_outspec['end'], file=output, end='')
                print(H_FOOTER.format(src_file, leafname_no_ext), file=output,
                      end='')


A11Y_OUTPUT_SPEC = {
    'filter': FILTER_ALLOW_ONLY_A11Y,
    'begin': H_BEGIN_A11Y,
}

LSP_MODE_GUID = 'Guid'

LSP_OUTPUT_SPEC = [
    {'mode': LSP_MODE_GUID,
     'filter': FILTER_ALLOW_ONLY_LSP,
     'begin': H_BEGIN_LSP,
     'end': H_END_LSP},
]

GENERATED_BLOCKLIST_FILES = [
    BlocklistDescriptor('A11y', ['BROWSER_PROCESS'], outspec=A11Y_OUTPUT_SPEC),
    BlocklistDescriptor('Launcher', ALL_DEFINITION_LISTS, flagspec={
                        'BROWSER_PROCESS': {BROWSER_PROCESS_ONLY},
                        'CHILD_PROCESSES': {CHILD_PROCESSES_ONLY}}),
    BlocklistDescriptor('Legacy', ALL_DEFINITION_LISTS, flagspec={
                        'BROWSER_PROCESS': {BROWSER_PROCESS_ONLY},
                        'CHILD_PROCESSES': {CHILD_PROCESSES_ONLY}}),
    # Roughed-in for the moment; we'll enable this in bug 1238735
    # BlocklistDescriptor('LSP', ALL_DEFINITION_LISTS, outspec=LSP_OUTPUT_SPEC),
    BlocklistDescriptor('Test', ALL_DEFINITION_LISTS,
                        outspec={'filter': FILTER_TESTS_ONLY}),
]


class PETimeStamp(object):
    def __init__(self, ts):
        max_timestamp = (2 ** 32) - 1
        if ts < 0 or ts > max_timestamp:
            raise ValueError('Invalid timestamp value')
        self._value = ts

    def __str__(self):
        return '0x%08XU' % self._value


class Version(object):
    """Encapsulates a DLL version.
    """

    ALL_VERSIONS = 0xFFFFFFFFFFFFFFFF
    UNVERSIONED = 0

    def __init__(self, *args):
        """There are multiple ways to construct a Version:

        As a tuple containing four elements (recommended);
        As four integral arguments;
        As a PETimeStamp;
        As a long integer.

        The tuple and list formats require the value of each element to be
        between 0 and 0xFFFF, inclusive.
        """

        self._ver = Version.UNVERSIONED

        if len(args) == 1:
            if isinstance(args[0], tuple):
                self.validate_iterable(args[0])

                self._ver = 'MAKE_VERSION%r' % (args[0],)
            elif isinstance(args[0], PETimeStamp):
                self._ver = args[0]
            else:
                self._ver = int(args[0])
        elif len(args) == 4:
            self.validate_iterable(args)

            self._ver = 'MAKE_VERSION%r' % (tuple(args),)
        else:
            raise ValueError('Bad arguments to Version constructor')

    def validate_iterable(self, arg):
        if len(arg) != 4:
            raise ValueError('Versions must be a 4-tuple')

        for component in arg:
            if (not isinstance(component, int) or component < 0
                or component > 0xFFFF):
                raise ValueError('Each version component must be a 16-bit '
                                 'unsigned integer')

    def build_long(self, args):
        self.validate_iterable(args)
        return (int(args[0]) << 48) | (int(args[1]) << 32) | \
            (int(args[2]) << 16) | int(args[3])

    def is_timestamp(self):
        return isinstance(self._ver, PETimeStamp)

    def __str__(self):
        if isinstance(self._ver, int):
            if self._ver == Version.ALL_VERSIONS:
                return 'DllBlockInfo::ALL_VERSIONS'

            if self._ver == Version.UNVERSIONED:
                return 'DllBlockInfo::UNVERSIONED'

            return '0x%016XULL' % self._ver

        return str(self._ver)


class DllBlocklistEntry(object):
    TEST_CONDITION = 'defined(ENABLE_TESTS)'

    def __init__(self, name, ver, flags=(), **kwargs):
        """Positional arguments:

        name -- The leaf name of the DLL.

        ver -- The maximum version to be blocked. NB: The comparison used by the
        blocklist is <=, so you should specify the last bad version, as opposed
        to the first good version.

        flags -- iterable containing the flags that should be applicable to
        this blocklist entry.

        Keyword arguments:

        condition -- a string containing a C++ preprocessor expression. This
        condition is written as a condition for an #if/#endif block that is
        generated around the entry during output.
        """

        self.check_ascii(name)
        self._name = name.lower()
        self._ver = Version(ver)

        self._flags = set()
        self.add_flags(flags)
        if self._ver.is_timestamp():
            self._flags.add(USE_TIMESTAMP)

        self._cond = kwargs.get('condition', set())
        if isinstance(self._cond, str):
            self._cond = {self._cond}

    @staticmethod
    def check_ascii(name):
        if PY2:
            if not all(ord(c) < 128 for c in name):
                raise ValueError('DLL name "%s" must be ASCII!' % name)
            return

        try:
            # Supported in Python 3.7
            if not name.isascii():
                raise ValueError('DLL name "%s" must be ASCII!' % name)
            return
        except AttributeError:
            pass

        try:
            name.encode('ascii')
        except UnicodeEncodeError:
            raise ValueError('DLL name "%s" must be ASCII!' % name)

    def get_name(self):
        return self._name

    def set_condition(self, cond):
        self._cond.add(cond)

    def get_condition(self):
        if len(self._cond) == 1:
            fmt = '{0}'
        else:
            fmt = '({0})'

        return ' && '.join([fmt.format(c) for c in self._cond])

    def set_is_test(self):
        self.set_condition(DllBlocklistEntry.TEST_CONDITION)

    def is_test(self):
        return self._cond and DllBlocklistEntry.TEST_CONDITION in self._cond

    def add_flags(self, new_flags):
        if isinstance(new_flags, str):
            self._flags.add(new_flags)
        else:
            self._flags.update(new_flags)

    @staticmethod
    def get_flag_string(flag):
        return 'DllBlockInfo::' + flag

    def get_flags_list(self):
        return self._flags

    def write(self, output, mode):
        if self._cond:
            print('#if %s' % self.get_condition(), file=output)

        flags_str = ''

        flags = self.get_flags_list()
        if flags:
            flags_str = ', ' + ' | '.join(map(self.get_flag_string, flags))

        entry_str = "  DLL_BLOCKLIST_ENTRY(\"%s\", %s%s)" % \
                    (self._name, str(self._ver), flags_str)
        print(entry_str, file=output)

        if self._cond:
            print('#endif  // %s' % self.get_condition(), file=output)


class A11yBlocklistEntry(DllBlocklistEntry):
    """ Represents a blocklist entry for injected a11y DLLs. This class does
    not need to do anything special compared to a DllBlocklistEntry; we just
    use this type to distinguish a11y entries from "regular" blocklist entries.
    """

    def __init__(self, name, ver, flags=(), **kwargs):
        """These arguments are identical to DllBlocklistEntry.__init__
        """

        super(A11yBlocklistEntry, self).__init__(name, ver, flags, **kwargs)


class LspBlocklistEntry(DllBlocklistEntry):
    """ Represents a blocklist entry for a WinSock Layered Service Provider (LSP).
    """

    GUID_UNPACK_FMT_LE = '<IHHBBBBBBBB'
    Guids = dict()

    def __init__(self, name, ver, guids, flags=(), **kwargs):
        """Positional arguments:

        name -- The leaf name of the DLL.

        ver -- The maximum version to be blocked. NB: The comparison used by the
        blocklist is <=, so you should specify the last bad version, as opposed
        to the first good version.

        guids -- Either a string or list of strings containing the GUIDs that
        uniquely identify the LSP. These GUIDs are generated by the developer of
        the LSP and are registered with WinSock alongside the LSP. We record
        this GUID as part of the "Winsock_LSP" annotation in our crash reports.

        flags -- iterable containing the flags that should be applicable to
        this blocklist entry.

        Keyword arguments:

        condition -- a string containing a C++ preprocessor expression. This
        condition is written as a condition for an #if/#endif block that is
        generated around the entry during output.
        """

        super(LspBlocklistEntry, self).__init__(name, ver, flags, **kwargs)
        if not guids:
            raise ValueError('Missing GUID(s)!')

        if isinstance(guids, str):
            self.insert(UUID(guids), name)
        else:
            for guid in guids:
                self.insert(UUID(guid), name)

    def insert(self, guid, name):
        # Some explanation here: Multiple DLLs (and thus multiple instances of
        # LspBlocklistEntry) may share the same GUIDs. To ensure that we do not
        # have any duplicates, we store each GUID in a class variable, Guids.
        # We include the original DLL name from the blocklist definitions so
        # that we may output a comment above each GUID indicating which entries
        # correspond to which GUID.
        LspBlocklistEntry.Guids.setdefault(guid, []).append(name)

    def get_flags_list(self):
        flags = super(LspBlocklistEntry, self).get_flags_list()
        # LSP entries always include the following flag
        flags.add(SUBSTITUTE_LSP_PASSTHROUGH)
        return flags

    @staticmethod
    def as_c_struct(guid, names):
        parts = unpack(LspBlocklistEntry.GUID_UNPACK_FMT_LE, guid.bytes_le)
        str_guid = "  // %r\n  // {%s}\n  { 0x%08x, 0x%04x, 0x%04x, " \
            "{ 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x }" \
            " }" % (names, str(guid), parts[0], parts[1], parts[2], parts[3],
                    parts[4], parts[5], parts[6], parts[7], parts[8], parts[9],
                    parts[10])
        return str_guid

    def write(self, output, mode):
        if mode != LSP_MODE_GUID:
            super(LspBlocklistEntry, self).write(output, mode)
            return

        # We dump the entire contents of Guids on the first call, and then
        # clear it. Remaining invocations of this method are no-ops.
        if LspBlocklistEntry.Guids:
            result = ',\n'.join([self.as_c_struct(guid, names)
                                 for guid, names in iteritems(LspBlocklistEntry.Guids)])
            print(result, file=output)
            LspBlocklistEntry.Guids.clear()


def exec_script_file(script_name, globals):
    with open(script_name) as script:
        exec(compile(script.read(), script_name, 'exec'), globals)


def gen_blocklists(first_fd, defs_filename):

    BlocklistDescriptor.set_output_fd(first_fd)

    # exec_env defines the global variables that will be present in the
    # execution environment when defs_filename is run by exec.
    exec_env = {
        # Add the blocklist entry types
        'A11yBlocklistEntry': A11yBlocklistEntry,
        'DllBlocklistEntry': DllBlocklistEntry,
        'LspBlocklistEntry': LspBlocklistEntry,
        # Add the special version types
        'ALL_VERSIONS': Version.ALL_VERSIONS,
        'UNVERSIONED': Version.UNVERSIONED,
        'PETimeStamp': PETimeStamp,
    }

    # Import definition lists into exec_env
    for defname in ALL_DEFINITION_LISTS:
        exec_env[defname] = []
        # For each defname, add a special list for test-only entries
        exec_env[derive_test_key(defname)] = []

    # Import flags into exec_env
    exec_env.update({flag: flag for flag in INPUT_ONLY_FLAGS})

    # Now execute the input script with exec_env providing the globals
    exec_script_file(defs_filename, exec_env)

    # Tell the output descriptors to write out the output files.
    for desc in GENERATED_BLOCKLIST_FILES:
        desc.write(defs_filename, exec_env)

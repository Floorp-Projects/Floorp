# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains utility functions shared by the scalars and the histogram generation
# scripts.

from __future__ import print_function

import re
import yaml
import sys

# This is a list of flags that determine which process a measurement is allowed
# to record from.
KNOWN_PROCESS_FLAGS = {
    'all': 'All',
    'all_children': 'AllChildren',
    'main': 'Main',
    'content': 'Content',
    'gpu': 'Gpu',
    'socket': 'Socket',
    # Historical Values
    'all_childs': 'AllChildren',  # Supporting files from before bug 1363725
}

SUPPORTED_PRODUCTS = {
    'firefox': 'Firefox',
    'fennec': 'Fennec',
    'geckoview': 'Geckoview',
    'geckoview_streaming': 'GeckoviewStreaming',
}

SUPPORTED_OPERATING_SYSTEMS = [
    'mac',
    'linux',
    'windows',
    'android',
    'unix',
    'all',
]

# mozinfo identifies linux, BSD variants, Solaris and SunOS as unix
# Solaris and SunOS are identified as "unix" OS.
UNIX_LIKE_OS = [
    "unix",
    "linux",
    "bsd",
]

CANONICAL_OPERATING_SYSTEMS = {
    'darwin': 'mac',
    'linux': 'linux',
    'winnt': 'windows',
    'android': 'android',
    # for simplicity we treat all BSD and Solaris systems as unix
    'gnu/kfreebsd': 'unix',
    'sunos': 'unix',
    'dragonfly': 'unix',
    'freeunix': 'unix',
    'netunix': 'unix',
    'openunix': 'unix'
}

PROCESS_ENUM_PREFIX = "mozilla::Telemetry::Common::RecordedProcessType::"
PRODUCT_ENUM_PREFIX = "mozilla::Telemetry::Common::SupportedProduct::"


class ParserError(Exception):
    """Thrown by different probe parsers. Errors are partitioned into
    'immediately fatal' and 'eventually fatal' so that the parser can print
    multiple error messages at a time. See bug 1401612 ."""

    eventual_errors = []

    def __init__(self, *args):
        Exception.__init__(self, *args)

    def handle_later(self):
        ParserError.eventual_errors.append(self)

    def handle_now(self):
        ParserError.print_eventuals()
        print(self.message, file=sys.stderr)
        sys.exit(1)

    @classmethod
    def print_eventuals(cls):
        while cls.eventual_errors:
            print(cls.eventual_errors.pop(0).message, file=sys.stderr)

    @classmethod
    def exit_func(cls):
        if cls.eventual_errors:
            cls("Some errors occurred").handle_now()


def is_valid_process_name(name):
    return (name in KNOWN_PROCESS_FLAGS)


def process_name_to_enum(name):
    return PROCESS_ENUM_PREFIX + KNOWN_PROCESS_FLAGS.get(name)


def is_valid_product(name):
    return (name in SUPPORTED_PRODUCTS)


def is_valid_os(name):
    return (name in SUPPORTED_OPERATING_SYSTEMS)


def canonical_os(os):
    """Translate possible OS_TARGET names to their canonical value."""

    return CANONICAL_OPERATING_SYSTEMS.get(os.lower()) or "unknown"


def product_name_to_enum(product):
    if not is_valid_product(product):
        raise ParserError("Invalid product {}".format(product))
    return PRODUCT_ENUM_PREFIX + SUPPORTED_PRODUCTS.get(product)


class StringTable:
    """Manages a string table and allows C style serialization to a file."""

    def __init__(self):
        self.current_index = 0
        self.table = {}

    def c_strlen(self, string):
        """The length of a string including the null terminating character.
        :param string: the input string.
        """
        return len(string) + 1

    def stringIndex(self, string):
        """Returns the index in the table of the provided string. Adds the string to
        the table if it's not there.
        :param string: the input string.
        """
        if string in self.table:
            return self.table[string]
        else:
            result = self.current_index
            self.table[string] = result
            self.current_index += self.c_strlen(string)
            return result

    def stringIndexes(self, strings):
        """ Returns a list of indexes for the provided list of strings.
        Adds the strings to the table if they are not in it yet.
        :param strings: list of strings to put into the table.
        """
        return [self.stringIndex(s) for s in strings]

    def writeDefinition(self, f, name):
        """Writes the string table to a file as a C const char array.

        This writes out the string table as one single C char array for memory
        size reasons, separating the individual strings with '\0' characters.
        This way we can index directly into the string array and avoid the additional
        storage costs for the pointers to them (and potential extra relocations for those).

        :param f: the output stream.
        :param name: the name of the output array.
        """
        entries = self.table.items()
        entries.sort(key=lambda x: x[1])

        # Avoid null-in-string warnings with GCC and potentially
        # overlong string constants; write everything out the long way.
        def explodeToCharArray(string):
            def toCChar(s):
                if s == "'":
                    return "'\\''"
                else:
                    return "'%s'" % s
            return ", ".join(map(toCChar, string))

        f.write("#if defined(_MSC_VER) && !defined(__clang__)\n")
        f.write("const char %s[] = {\n" % name)
        f.write("#else\n")
        f.write("constexpr char %s[] = {\n" % name)
        f.write("#endif\n")
        for (string, offset) in entries:
            if "*/" in string:
                raise ValueError("String in string table contains unexpected sequence '*/': %s" %
                                 string)

            e = explodeToCharArray(string)
            if e:
                f.write("  /* %5d - \"%s\" */ %s, '\\0',\n"
                        % (offset, string, explodeToCharArray(string)))
            else:
                f.write("  /* %5d - \"%s\" */ '\\0',\n" % (offset, string))
        f.write("};\n\n")


def static_assert(output, expression, message):
    """Writes a C++ compile-time assertion expression to a file.
    :param output: the output stream.
    :param expression: the expression to check.
    :param message: the string literal that will appear if the expression evaluates to
        false.
    """
    print("static_assert(%s, \"%s\");" % (expression, message), file=output)


def validate_expiration_version(expiration):
    """ Makes sure the expiration version has the expected format.

    Allowed examples: "10", "20", "60", "never"
    Disallowed examples: "Never", "asd", "4000000", "60a1", "30.5a1"

    :param expiration: the expiration version string.
    :return: True if the expiration validates correctly, False otherwise.
    """
    if expiration != 'never' and not re.match(r'^\d{1,3}$', expiration):
        return False

    return True


def add_expiration_postfix(expiration):
    """ Formats the expiration version and adds a version postfix if needed.

    :param expiration: the expiration version string.
    :return: the modified expiration string.
    """
    if re.match(r'^[1-9][0-9]*$', expiration):
        return expiration + ".0a1"

    if re.match(r'^[1-9][0-9]*\.0$', expiration):
        return expiration + "a1"

    return expiration


def load_yaml_file(filename):
    """ Load a YAML file from disk, throw a ParserError on failure."""
    try:
        with open(filename, 'r') as f:
            return yaml.safe_load(f)
    except IOError, e:
        raise ParserError('Error opening ' + filename + ': ' + e.message)
    except ValueError, e:
        raise ParserError('Error parsing processes in {}: {}'
                          .format(filename, e.message))

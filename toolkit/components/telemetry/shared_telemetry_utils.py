# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains utility functions shared by the scalars and the histogram generation
# scripts.

from __future__ import print_function

import re

# This is a list of flags that determine which process a measurement is allowed
# to record from.
KNOWN_PROCESS_FLAGS = {
    'all': 'All',
    'all_childs': 'AllChilds',
    'main': 'Main',
    'content': 'Content',
    'gpu': 'Gpu',
}

PROCESS_ENUM_PREFIX = "mozilla::Telemetry::Common::RecordedProcessType::"


# This is thrown by the different probe parsers.
class ParserError(Exception):
    pass


def is_valid_process_name(name):
    return (name in KNOWN_PROCESS_FLAGS)


def process_name_to_enum(name):
    return PROCESS_ENUM_PREFIX + KNOWN_PROCESS_FLAGS.get(name)


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

        f.write("const char %s[] = {\n" % name)
        for (string, offset) in entries:
            if "*/" in string:
                raise ValueError("String in string table contains unexpected sequence '*/': %s" % string)

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

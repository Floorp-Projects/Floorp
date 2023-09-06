# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from io import StringIO


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
        result = self.current_index
        self.table[string] = result
        self.current_index += self.c_strlen(string)
        return result

    def stringIndexes(self, strings):
        """Returns a list of indexes for the provided list of strings.
        Adds the strings to the table if they are not in it yet.
        :param strings: list of strings to put into the table.
        """
        return [self.stringIndex(s) for s in strings]

    def writeToString(self, name):
        """Writes the string table to a string as a C const char array.

        See `writeDefinition` for details

        :param name: the name of the output array.
        """

        output = StringIO()
        self.writeDefinition(output, name)
        return output.getvalue()

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

        # Avoid null-in-string warnings with GCC and potentially
        # overlong string constants; write everything out the long way.
        def explodeToCharArray(string):
            def toCChar(s):
                if s == "'":
                    return "'\\''"
                return "'%s'" % s

            return ", ".join(map(toCChar, string))

        f.write("#if defined(_MSC_VER) && !defined(__clang__)\n")
        f.write("const char %s[] = {\n" % name)
        f.write("#else\n")
        f.write("constexpr char %s[] = {\n" % name)
        f.write("#endif\n")
        for string, offset in sorted(entries, key=lambda x: x[1]):
            if "*/" in string:
                raise ValueError(
                    "String in string table contains unexpected sequence '*/': %s"
                    % string
                )

            e = explodeToCharArray(string)
            if e:
                f.write(
                    "  /* %5d - \"%s\" */ %s, '\\0',\n"
                    % (offset, string, explodeToCharArray(string))
                )
            else:
                f.write("  /* %5d - \"%s\" */ '\\0',\n" % (offset, string))
        f.write("};\n\n")

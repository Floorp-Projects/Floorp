# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import codecs
import encodings.idna
import re
import sys

from make_dafsa import words_to_bin, words_to_cxx

"""
Processes a file containing effective TLD data.  See the following URL for a
description of effective TLDs and of the file format that this script
processes (although for the latter you're better off just reading this file's
short source code).

http://wiki.mozilla.org/Gecko:Effective_TLD_Service
"""


def getEffectiveTLDs(path):
    file = codecs.open(path, "r", "UTF-8")
    domains = set()
    for line in file:
        # line always contains a line terminator unless the file is empty
        if len(line) == 0:
            raise StopIteration
        line = line.rstrip()
        # comment, empty, or superfluous line for explicitness purposes
        if line.startswith("//") or not line.strip():
            continue
        line = re.split(r"[ \t\n]", line, 1)[0]
        entry = EffectiveTLDEntry(line)
        domain = entry.domain()
        assert domain not in domains, "repeating domain %s makes no sense" % domain
        domains.add(domain)
        yield entry


def _normalizeHostname(domain):
    """
    Normalizes the given domain, component by component.  ASCII components are
    lowercased, while non-ASCII components are processed using the ToASCII
    algorithm.
    """

    def convertLabel(label):
        if _isASCII(label):
            return label.lower()
        return encodings.idna.ToASCII(label).decode("utf-8")

    return ".".join(map(convertLabel, domain.split(".")))


def _isASCII(s):
    "True if s consists entirely of ASCII characters, false otherwise."
    for c in s:
        if ord(c) > 127:
            return False
    return True


class EffectiveTLDEntry:
    """
    Stores an entry in an effective-TLD name file.
    """

    _exception = False
    _wild = False

    def __init__(self, line):
        """
        Creates a TLD entry from a line of data, which must have been stripped of
        the line ending.
        """
        if line.startswith("!"):
            self._exception = True
            domain = line[1:]
        elif line.startswith("*."):
            self._wild = True
            domain = line[2:]
        else:
            domain = line
        self._domain = _normalizeHostname(domain)

    def domain(self):
        "The domain this represents."
        return self._domain

    def exception(self):
        "True if this entry's domain denotes does not denote an effective TLD."
        return self._exception

    def wild(self):
        "True if this entry represents a class of effective TLDs."
        return self._wild


#################
# DO EVERYTHING #
#################


def main(output, effective_tld_filename, output_format="cxx"):
    """
    effective_tld_filename is the effective TLD file to parse.
    based on the output format, either a C++ array of a binary representation
    of a DAFSA representing the eTLD file is then printed to standard output
    or a binary file is written to disk.
    """

    def typeEnum(etld):
        """
        Maps the flags to the DAFSA's enum types.
        """
        if etld.exception():
            return 1
        elif etld.wild():
            return 2
        else:
            return 0

    def dafsa_words():
        """
        make_dafsa expects lines of the form "<domain_name><enum_value>"
        """
        for etld in getEffectiveTLDs(effective_tld_filename):
            yield "%s%d" % (etld.domain(), typeEnum(etld))

    """ words_to_bin() returns a bytes while words_to_cxx() returns string """
    if output_format == "bin":
        output.write(words_to_bin(dafsa_words()))
    else:
        output.write(words_to_cxx(dafsa_words()))


if __name__ == "__main__":
    """
    This program can output the DAFSA in two formats:
    as C++ code that will be included and compiled at build time
    or as a binary file that will be published in Remote Settings.

    Flags for format options:
    "cxx" -> C++ array [default]
    "bin" -> Binary file
    """

    output_format = "bin" if "--bin" in sys.argv else "cxx"
    main(sys.stdout, sys.argv[1], output_format=output_format)

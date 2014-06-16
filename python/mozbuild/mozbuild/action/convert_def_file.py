# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Convert Windows-style export files into a single Unix-style linker
# script, applying any necessary preprocessing.

import itertools
import re
import sys
from StringIO import StringIO

from mozbuild.preprocessor import Preprocessor
from mozbuild.util import FileAvoidWrite

def preprocess_file(pp, deffile):
    output = StringIO()
    with open(deffile, 'r') as input:
        pp.processFile(input=input, output=output)
    return output.getvalue().splitlines()

# NSS .def files serve multiple masters, as this copied comment indicates:
#
# OK, this file is meant to support SUN, LINUX, AIX and WINDOWS
#   1. For all unix platforms, the string ";-"  means "remove this line"
#   2. For all unix platforms, the string " DATA " will be removed from any 
#	line on which it occurs.
#   3. Lines containing ";+" will have ";+" removed on SUN and LINUX.
#      On AIX, lines containing ";+" will be removed.  
#   4. For all unix platforms, the string ";;" will have the ";;" removed.
#   5. For all unix platforms, after the above processing has taken place,
#    all characters after the first ";" on the line will be removed.  
#    And for AIX, the first ";" will also be removed.
#  This file is passed directly to windows. Since ';' is a comment, all UNIX
#   directives are hidden behind ";", ";+", and ";-"
#
# We don't care about rule 1, as that mainly serves to eliminate LIBRARY
# and EXPORTS lines.  Our symbol extraction routines handle DATA, so we
# don't need to bother with rule 2.  We don't want to enforce rule 3, as
# we know how to eliminate comments. ';+' also tends to hide Unix
# linker-script specific things, which we don't want to deal with here.
# Rule 5 is also unnecessary; later comment-aware processing will deal
# with that.
#
# We need to handle rule 4, since ';;' often hides things marked with DATA.
def nss_preprocess_file(deffile):
    with open(deffile, 'r') as input:
        for line in input:
            yield line.replace(';;', '')

COMMENT = re.compile(';.*')

def extract_symbols(lines):
    # Filter comments.
    nocomments = iter(COMMENT.sub('', s).strip() for s in lines)
    lines = iter(s for s in nocomments if len(s))

    exports = itertools.dropwhile(lambda s: 'EXPORTS' not in s, lines)
    symbols = set()
    for line in exports:
        if 'EXPORTS' in line:
            # Handle the case where symbols are specified along with EXPORT.
            fields = line.split()[1:]
            if len(fields) == 0:
                continue
        else:
            fields = line.split()

        # We don't support aliases, and we only support the DATA keyword on
        # symbols. But since aliases can also be specified as 'SYM=ALIAS'
        # with no whitespace, we need extra checks on the original symbol.
        if '=' in fields[0]:
            raise BaseException, 'aliases are not supported (%s)' % line
        if len(fields) == 1:
            pass
        elif len(fields) != 2 or fields[1] != 'DATA':
            raise BaseException, 'aliases and keywords other than DATA are not supported (%s)' % line

        symbols.add(fields[0])

    return symbols

def main(args):
    pp = Preprocessor()
    optparser = pp.getCommandLineParser()
    optparser.add_option('--nss-file', action='append',
                         type='string', dest='nss_files', default=[],
                         help='Specify a .def file that should have NSS\'s processing rules applied to it')
    options, deffiles = optparser.parse_args(args)

    symbols = set()
    for f in options.nss_files:
        symbols |= extract_symbols(nss_preprocess_file(f))
    for f in deffiles:
        # Start each deffile off with a clean slate.
        defpp = pp.clone()
        symbols |= extract_symbols(preprocess_file(defpp, f))

    script = """{
global:
  %s
local:
  *;
};
"""
    with FileAvoidWrite(options.output) as f:
        f.write(script % '\n  '.join("%s;" % s for s in sorted(symbols)))

if __name__ == '__main__':
    main(sys.argv[1:])

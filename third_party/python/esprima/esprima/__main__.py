# -*- coding: utf-8 -*-
# Copyright JS Foundation and other contributors, https://js.foundation/
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import absolute_import, unicode_literals, print_function, division

import sys

from .esprima import parse, tokenize, Error, toDict
from . import version


def main():
    import json
    import time
    import optparse

    usage = "usage: %prog [options] [file.js]"
    parser = optparse.OptionParser(usage=usage, version=version)
    parser.add_option("--comment", dest="comment",
                      action="store_true", default=False,
                      help="Gather all line and block comments in an array")
    parser.add_option("--attachComment", dest="attachComment",
                      action="store_true", default=False,
                      help="Attach comments to nodes")
    parser.add_option("--loc", dest="loc", default=False,
                      action="store_true",
                      help="Include line-column location info for each syntax node")
    parser.add_option("--range", dest="range", default=False,
                      action="store_true",
                      help="Include index-based range for each syntax node")
    parser.add_option("--raw", dest="raw", default=False,
                      action="store_true",
                      help="Display the raw value of literals")
    parser.add_option("--tokens", dest="tokens", default=False,
                      action="store_true",
                      help="List all tokens in an array")
    parser.add_option("--tolerant", dest="tolerant", default=False,
                      action="store_true",
                      help="Tolerate errors on a best-effort basis (experimental)")
    parser.add_option("--tokenize", dest="tokenize", default=False,
                      action="store_true",
                      help="Only tokenize, do not parse.")
    parser.add_option("--module", dest="sourceType", default='string',
                      action="store_const", const='module',
                      help="Tolerate errors on a best-effort basis (experimental)")
    parser.set_defaults(jsx=True, classProperties=True)
    opts, args = parser.parse_args()

    if len(args) == 1:
        with open(args[0], 'rb') as f:
            code = f.read().decode('utf-8')
    elif sys.stdin.isatty():
        parser.print_help()
        return 64
    else:
        code = sys.stdin.read().decode('utf-8')

    options = opts.__dict__
    do_tokenize = options.pop('tokenize')

    t = time.time()
    try:
        if do_tokenize:
            del options['sourceType']
            del options['tokens']
            del options['raw']
            del options['jsx']
            res = toDict(tokenize(code, options=options))
        else:
            res = toDict(parse(code, options=options))
    except Error as e:
        res = e.toDict()
    dt = time.time() - t + 0.000000001

    print(json.dumps(res, indent=4))
    print()
    print('Parsed everyting in', round(dt, 5), 'seconds.')
    print('Thats %d characters per second' % (len(code) // dt))

    return 0


if __name__ == '__main__':
    retval = main()
    sys.exit(retval)

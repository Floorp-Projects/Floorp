# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distibuted with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from mozbuild.preprocessor import Preprocessor
from io import StringIO
import buildconfig
import os


def main(output, input_file):
    with open(input_file) as fh:
        if buildconfig.substs['EXPAND_LIBS_LIST_STYLE'] == 'linkerscript':
            def cleanup(line):
                assert line.startswith('INPUT("')
                assert line.endswith('")')
                return line[len('INPUT("'):-len('")')]

            objs = [cleanup(l.strip()) for l in fh.readlines()]
        else:
            objs = [l.strip() for l in fh.readlines()]

    pp = Preprocessor()
    pp.out = StringIO()
    pp.do_include(os.path.join(buildconfig.topobjdir, 'buildid.h'))
    buildid = pp.context['MOZ_BUILDID']
    output.write(
        'extern const char gToolkitBuildID[] = "%s";' % buildid
    )
    return set(os.path.join('build', o) for o in objs
               if os.path.splitext(os.path.basename(o))[0] != 'buildid')

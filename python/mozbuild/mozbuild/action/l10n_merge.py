# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import shutil
import sys
import os

from mozbuild.util import (
    ensureParentDir,
)


def main(argv):
    parser = argparse.ArgumentParser(description='Merge l10n files.')
    parser.add_argument('--output', help='Path to write merged output')
    parser.add_argument('--ref-file', help='Path to reference file (en-US)')
    parser.add_argument('--l10n-file', help='Path to locale file')

    args = parser.parse_args(argv)

    from compare_locales.compare import (
        ContentComparer,
        Observer,
    )
    from compare_locales.paths import (
        File,
    )
    cc = ContentComparer([Observer()])
    cc.compare(File(args.ref_file, args.ref_file, ''),
               File(args.l10n_file, args.l10n_file, ''),
               args.output)

    ensureParentDir(args.output)
    if not os.path.exists(args.output):
        src = args.l10n_file
        if not os.path.exists(args.l10n_file):
            src = args.ref_file
        shutil.copy(src, args.output)

    return 0


if __name__ == '__main__':
    main(sys.argv[1:])

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

r'''This module contains code for managing clobbering of the tree.'''

import os
import sys

from mozfile.mozfile import rmtree
from textwrap import TextWrapper


CLOBBER_MESSAGE = ''.join([TextWrapper().fill(line) + '\n' for line in
'''
The CLOBBER file has been updated, indicating that an incremental build since \
your last build will probably not work. A full/clobber build is required.

The reason for the clobber is:

{clobber_reason}

Clobbering can be performed automatically. However, we didn't automatically \
clobber this time because:

{no_reason}

The easiest and fastest way to clobber is to run:

 $ mach clobber

If you know this clobber doesn't apply to you or you're feeling lucky -- \
Well, are ya? -- you can ignore this clobber requirement by running:

 $ touch {clobber_file}
'''.splitlines()])

class Clobberer(object):
    def __init__(self, topsrcdir, topobjdir):
        """Create a new object to manage clobbering the tree.

        It is bound to a top source directory and to a specific object
        directory.
        """
        assert os.path.isabs(topsrcdir)
        assert os.path.isabs(topobjdir)

        self.topsrcdir = os.path.normpath(topsrcdir)
        self.topobjdir = os.path.normpath(topobjdir)
        self.src_clobber = os.path.join(topsrcdir, 'CLOBBER')
        self.obj_clobber = os.path.join(topobjdir, 'CLOBBER')

        assert os.path.isfile(self.src_clobber)

    def clobber_needed(self):
        """Returns a bool indicating whether a tree clobber is required."""

        # No object directory clobber file means we're good.
        if not os.path.exists(self.obj_clobber):
            return False

        # Object directory clobber older than current is fine.
        if os.path.getmtime(self.src_clobber) <= \
            os.path.getmtime(self.obj_clobber):

            return False

        return True

    def clobber_cause(self):
        """Obtain the cause why a clobber is required.

        This reads the cause from the CLOBBER file.

        This returns a list of lines describing why the clobber was required.
        Each line is stripped of leading and trailing whitespace.
        """
        with open(self.src_clobber, 'rt') as fh:
            lines = [l.strip() for l in fh.readlines()]
            return [l for l in lines if l and not l.startswith('#')]

    def ensure_objdir_state(self):
        """Ensure the CLOBBER file in the objdir exists.

        This is called as part of the build to ensure the clobber information
        is configured properly for the objdir.
        """
        if not os.path.exists(self.topobjdir):
            os.makedirs(self.topobjdir)

        if not os.path.exists(self.obj_clobber):
            # Simply touch the file.
            with open(self.obj_clobber, 'a'):
                pass

    def maybe_do_clobber(self, cwd, allow_auto=False, fh=sys.stderr):
        """Perform a clobber if it is required. Maybe.

        This is the API the build system invokes to determine if a clobber
        is needed and to automatically perform that clobber if we can.

        This returns a tuple of (bool, bool, str). The elements are:

          - Whether a clobber was/is required.
          - Whether a clobber was performed.
          - The reason why the clobber failed or could not be performed. This
            will be None if no clobber is required or if we clobbered without
            error.
        """
        assert cwd
        cwd = os.path.normpath(cwd)

        if not self.clobber_needed():
            print('Clobber not needed.', file=fh)
            self.ensure_objdir_state()
            return False, False, None

        # So a clobber is needed. We only perform a clobber if we are
        # allowed to perform an automatic clobber (off by default) and if the
        # current directory is not under the object directory. The latter is
        # because operating systems, filesystems, and shell can throw fits
        # if the current working directory is deleted from under you. While it
        # can work in some scenarios, we take the conservative approach and
        # never try.
        if not allow_auto:
            return True, False, \
               self._message('Automatic clobbering is not enabled\n'
                              '  (add "mk_add_options AUTOCLOBBER=1" to your '
                              'mozconfig).')

        if cwd.startswith(self.topobjdir) and cwd != self.topobjdir:
            return True, False, self._message(
                'Cannot clobber while the shell is inside the object directory.')

        print('Automatically clobbering %s' % self.topobjdir, file=fh)
        try:
            if cwd == self.topobjdir:
                for entry in os.listdir(self.topobjdir):
                    full = os.path.join(self.topobjdir, entry)

                    if os.path.isdir(full):
                        rmtree(full)
                    else:
                        os.unlink(full)

            else:
                rmtree(self.topobjdir)

            self.ensure_objdir_state()
            print('Successfully completed auto clobber.', file=fh)
            return True, True, None
        except (IOError) as error:
            return True, False, self._message(
                'Error when automatically clobbering: ' + str(error))

    def _message(self, reason):
        lines = [' ' + line for line in self.clobber_cause()]

        return CLOBBER_MESSAGE.format(clobber_reason='\n'.join(lines),
            no_reason='  ' + reason, clobber_file=self.obj_clobber)


def main(args, env, cwd, fh=sys.stderr):
    if len(args) != 2:
        print('Usage: clobber.py topsrcdir topobjdir', file=fh)
        return 1

    topsrcdir, topobjdir = args

    if not os.path.isabs(topsrcdir):
        topsrcdir = os.path.abspath(topsrcdir)

    if not os.path.isabs(topobjdir):
        topobjdir = os.path.abspath(topobjdir)

    auto = True if env.get('AUTOCLOBBER', False) else False
    clobber = Clobberer(topsrcdir, topobjdir)
    required, performed, message = clobber.maybe_do_clobber(cwd, auto, fh)

    if not required or performed:
        if performed and env.get('TINDERBOX_OUTPUT'):
            print('TinderboxPrint: auto clobber')
        return 0

    print(message, file=fh)
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:], os.environ, os.getcwd(), sys.stdout))


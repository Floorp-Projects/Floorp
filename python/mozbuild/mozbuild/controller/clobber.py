# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

r"""This module contains code for managing clobbering of the tree."""

import errno
import os
import subprocess
import sys
from textwrap import TextWrapper

from mozfile.mozfile import remove as mozfileremove
from mozpack import path as mozpath

CLOBBER_MESSAGE = "".join(
    [
        TextWrapper().fill(line) + "\n"
        for line in """
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
""".splitlines()
    ]
)


class Clobberer(object):
    def __init__(self, topsrcdir, topobjdir, substs=None):
        """Create a new object to manage clobbering the tree.

        It is bound to a top source directory and to a specific object
        directory.
        """
        assert os.path.isabs(topsrcdir)
        assert os.path.isabs(topobjdir)

        self.topsrcdir = mozpath.normpath(topsrcdir)
        self.topobjdir = mozpath.normpath(topobjdir)
        self.src_clobber = mozpath.join(topsrcdir, "CLOBBER")
        self.obj_clobber = mozpath.join(topobjdir, "CLOBBER")
        if substs:
            self.substs = substs
        else:
            self.substs = dict()

    def clobber_needed(self):
        """Returns a bool indicating whether a tree clobber is required."""

        # No object directory clobber file means we're good.
        if not os.path.exists(self.obj_clobber):
            return False

        # No source directory clobber means we're running from a source package
        # that doesn't use clobbering.
        if not os.path.exists(self.src_clobber):
            return False

        # Object directory clobber older than current is fine.
        if os.path.getmtime(self.src_clobber) <= os.path.getmtime(self.obj_clobber):
            return False

        return True

    def clobber_cause(self):
        """Obtain the cause why a clobber is required.

        This reads the cause from the CLOBBER file.

        This returns a list of lines describing why the clobber was required.
        Each line is stripped of leading and trailing whitespace.
        """
        with open(self.src_clobber, "rt") as fh:
            lines = [l.strip() for l in fh.readlines()]
            return [l for l in lines if l and not l.startswith("#")]

    def have_winrm(self):
        # `winrm -h` should print 'winrm version ...' and exit 1
        try:
            p = subprocess.Popen(
                ["winrm.exe", "-h"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT
            )
            return p.wait() == 1 and p.stdout.read().startswith("winrm")
        except Exception:
            return False

    def collect_subdirs(self, root, exclude):
        """Gathers a list of subdirectories excluding specified items."""
        paths = []
        try:
            for p in os.listdir(root):
                if p not in exclude:
                    paths.append(mozpath.join(root, p))
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise

        return paths

    def delete_dirs(self, root, paths_to_delete):
        """Deletes the given subdirectories in an optimal way."""
        procs = []
        for p in sorted(paths_to_delete):
            path = mozpath.join(root, p)
            if (
                sys.platform.startswith("win")
                and self.have_winrm()
                and os.path.isdir(path)
            ):
                procs.append(subprocess.Popen(["winrm", "-rf", path]))
            else:
                # We use mozfile because it is faster than shutil.rmtree().
                mozfileremove(path)

        for p in procs:
            p.wait()

    def remove_objdir(self, full=True):
        """Remove the object directory.

        ``full`` controls whether to fully delete the objdir. If False,
        some directories (e.g. Visual Studio Project Files) will not be
        deleted.
        """
        # Determine where cargo build artifacts are stored
        RUST_TARGET_VARS = ("RUST_HOST_TARGET", "RUST_TARGET")
        rust_targets = set(
            [self.substs[x] for x in RUST_TARGET_VARS if x in self.substs]
        )
        rust_build_kind = "release"
        if self.substs.get("MOZ_DEBUG_RUST"):
            rust_build_kind = "debug"

        # Top-level files and directories to not clobber by default.
        no_clobber = {".mozbuild", "msvc", "_virtualenvs"}

        # Hold off on clobbering cargo build artifacts
        no_clobber |= rust_targets

        if full:
            paths = [self.topobjdir]
        else:
            paths = self.collect_subdirs(self.topobjdir, no_clobber)

        self.delete_dirs(self.topobjdir, paths)

        # Now handle cargo's build artifacts and skip removing the incremental
        # compilation cache.
        for target in rust_targets:
            cargo_path = mozpath.join(self.topobjdir, target, rust_build_kind)
            paths = self.collect_subdirs(
                cargo_path,
                {
                    "incremental",
                },
            )
            self.delete_dirs(cargo_path, paths)

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
        cwd = mozpath.normpath(cwd)

        if not self.clobber_needed():
            print("Clobber not needed.", file=fh)
            return False, False, None

        # So a clobber is needed. We only perform a clobber if we are
        # allowed to perform an automatic clobber (off by default) and if the
        # current directory is not under the object directory. The latter is
        # because operating systems, filesystems, and shell can throw fits
        # if the current working directory is deleted from under you. While it
        # can work in some scenarios, we take the conservative approach and
        # never try.
        if not allow_auto:
            return (
                True,
                False,
                self._message(
                    "Automatic clobbering is not enabled\n"
                    '  (add "mk_add_options AUTOCLOBBER=1" to your '
                    "mozconfig)."
                ),
            )

        if cwd.startswith(self.topobjdir) and cwd != self.topobjdir:
            return (
                True,
                False,
                self._message(
                    "Cannot clobber while the shell is inside the object directory."
                ),
            )

        print("Automatically clobbering %s" % self.topobjdir, file=fh)
        try:
            self.remove_objdir(False)
            print("Successfully completed auto clobber.", file=fh)
            return True, True, None
        except IOError as error:
            return (
                True,
                False,
                self._message("Error when automatically clobbering: " + str(error)),
            )

    def _message(self, reason):
        lines = [" " + line for line in self.clobber_cause()]

        return CLOBBER_MESSAGE.format(
            clobber_reason="\n".join(lines),
            no_reason="  " + reason,
            clobber_file=self.obj_clobber,
        )

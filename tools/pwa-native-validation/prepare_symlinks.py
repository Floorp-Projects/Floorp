# SPDX-License-Identifier: MPL-2.0
"""Create exactly one symlink fixture inside a new private test root; never remove."""
import os
import pathlib
import stat
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: prepare_symlinks.py ABSOLUTE_PRIVATE_TEST_ROOT")
root = pathlib.Path(sys.argv[1])
if not root.is_absolute() or not root.is_dir() or root.is_symlink():
    raise SystemExit("root must be an existing absolute non-symlink directory")
if os.path.normcase(os.path.normpath(str(root))) != os.path.normcase(os.path.realpath(root)):
    raise SystemExit("root must be canonical without symlink ancestors")
if os.name != "nt" and stat.S_IMODE(root.stat().st_mode) & 0o077:
    raise SystemExit("root must be private")
case = root / "symlink-case"
target = root / "symlink-target"
# No exist_ok: reject reused, attacker-controlled or partially prepared trees.
case.mkdir(mode=0o700)
target.mkdir(mode=0o700)
link = case / "fixture.app"
os.symlink(target, link, target_is_directory=True)
if not link.is_symlink() or os.path.normcase(os.path.realpath(link)) != os.path.normcase(str(target)):
    raise SystemExit("preflight symlink validation failed")
print("prepared symlink fixture under", root)

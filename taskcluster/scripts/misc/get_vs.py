#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import shutil
import ssl
from pathlib import Path
from tempfile import TemporaryDirectory
from urllib import request

import certifi
import yaml
from buildconfig import topsrcdir
from vsdownload import downloadPackages, extractPackages

# Hack to hook certifi
_urlopen = request.urlopen


def urlopen(url, data=None):
    return _urlopen(
        url, data, context=ssl.create_default_context(cafile=certifi.where())
    )


request.urlopen = urlopen


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Download and build a Visual Studio artifact"
    )
    parser.add_argument("manifest", help="YAML manifest of the contents to download")
    parser.add_argument("outdir", help="Output directory")
    args = parser.parse_args()

    out_dir = Path(args.outdir)
    with open(Path(topsrcdir) / args.manifest) as f:
        selected = yaml.safe_load(f.read())
    with TemporaryDirectory(prefix="get_vs", dir=".") as tmpdir:
        tmpdir = Path(tmpdir)
        dl_cache = tmpdir / "cache"
        downloadPackages(selected, dl_cache)
        unpacked = tmpdir / "unpack"
        extractPackages(selected, dl_cache, unpacked)
        vfs = {}
        # Fill the output directory with all the paths in lowercase form for
        # cross-compiles.
        for subpath in ("VC", "Windows Kits/10", "DIA SDK"):
            dest = subpath
            # When running on Windows, SDK files are extracted under Windows Kits,
            # but on other platforms, they end up in Program Files/Windows Kits.
            program_files_subpath = unpacked / "Program Files" / subpath
            if program_files_subpath.exists():
                subpath = program_files_subpath
            else:
                subpath = unpacked / subpath
            dest = Path(dest)
            for root, dirs, files in os.walk(subpath):
                relpath = Path(root).relative_to(subpath)
                for f in files:
                    path = Path(root) / f
                    mode = os.stat(path).st_mode
                    with open(path, "rb") as fh:
                        lower_f = f.lower()
                        # Ideally, we'd use the overlay for .libs too but as of
                        # writing it's still impractical to use, so lowercase
                        # them for now, that'll be enough.
                        if lower_f.endswith(".lib"):
                            f = lower_f
                        name = str(dest / relpath / f)
                        # Set executable flag on .exe files, the Firefox build
                        # system wants it.
                        if lower_f.endswith(".exe"):
                            mode |= (mode & 0o444) >> 2
                        print("Adding", name)
                        out_file = out_dir / name
                        out_file.parent.mkdir(parents=True, exist_ok=True)
                        with out_file.open("wb") as out_fh:
                            shutil.copyfileobj(fh, out_fh)
                        os.chmod(out_file, mode)
                        if lower_f.endswith((".h", ".idl")):
                            vfs.setdefault(str(dest / relpath), []).append(f)
        # Create an overlay file for use with clang's -ivfsoverlay flag.
        overlay = {
            "version": 0,
            "case-sensitive": False,
            "root-relative": "overlay-dir",
            "overlay-relative": True,
            "roots": [
                {
                    "name": p,
                    "type": "directory",
                    "contents": [
                        {
                            "name": f,
                            "type": "file",
                            "external-contents": f"{p}/{f}",
                        }
                        for f in files
                    ],
                }
                for p, files in vfs.items()
            ],
        }
        overlay_yaml = out_dir / "overlay.yaml"
        with overlay_yaml.open("w") as fh:
            fh.write(yaml.dump(overlay))

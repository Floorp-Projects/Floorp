#!/usr/bin/env python
"""Generate FreeType reference documentation."""

from __future__ import print_function

import argparse
import glob
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "--input-dir",
        required=True,
        help="Top-level FreeType source directory.",
    )

    parser.add_argument(
        "--version", required=True, help='FreeType version (e.g. "2.x.y").'
    )

    parser.add_argument(
        "--output-dir", required=True, help="Output directory."
    )

    args = parser.parse_args()

    # Get the list of input files of interest.
    include_dir = os.path.join(args.input_dir, "include")
    include_config_dir = os.path.join(include_dir, "config")
    include_cache_dir = os.path.join(include_dir, "cache")

    all_headers = (
        glob.glob(os.path.join(args.input_dir, "include", "freetype", "*.h"))
        + glob.glob(
            os.path.join(
                args.input_dir, "include", "freetype", "config", "*.h"
            )
        )
        + glob.glob(
            os.path.join(
                args.input_dir, "include", "freetype", "cache", "*.h"
            )
        )
    )

    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)
    else:
        assert os.path.isdir(args.output_dir), (
            "Not a directory: " + args.output_dir
        )

    cmds = [
        sys.executable,
        "-m",
        "docwriter",
        "--prefix=ft2",
        "--title=FreeType-" + args.version,
        "--site=reference",
        "--output=" + args.output_dir,
    ] + all_headers

    print("Running docwriter...")
    subprocess.check_call(cmds)

    print("Building static site...")
    subprocess.check_call(
        [sys.executable, "-m", "mkdocs", "build"], cwd=args.output_dir
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

# SPDX-License-Identifier: MPL-2.0
"""Explicit fixed-path filter, retaining original manifest predicates/assertions."""
import os
import sys
from pathlib import Path
from cold_browser_entry import run
from focused_qualification import CASES

if __name__ == "__main__":
    case=os.environ["FLOORP_QUALIFICATION_CASE"]
    if CASES[case] is None:raise ValueError("BLOCKED: reviewed selection unavailable")
    manifest,name=CASES[case]
    root=Path(os.environ["COLD_MOCHITEST_ROOT"]).resolve(strict=True)
    if not (root/"runtests.py").is_file():raise ValueError("missing native harness")
    sys.path.insert(0,str(root))
    import runtests
    sys.exit(run(runtests, [str(Path(manifest).parent/name)]))

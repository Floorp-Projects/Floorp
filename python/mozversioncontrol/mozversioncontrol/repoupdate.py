# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from pathlib import Path
from typing import Union

import subprocess

# The logic here is far from robust. Improvements are welcome.


def update_mercurial_repo(
    hg: str,
    repo,
    path: Union[str, Path],
    revision="default",
    hostfingerprints=None,
    global_args=None,
):
    """Ensure a HG repository exists at a path and is up to date."""
    hostfingerprints = hostfingerprints or {}

    path = Path(path)

    args = [hg]
    if global_args:
        args.extend(global_args)

    for host, fingerprint in sorted(hostfingerprints.items()):
        args.extend(["--config", "hostfingerprints.%s=%s" % (host, fingerprint)])

    if path.exists():
        subprocess.check_call(args + ["pull", repo], cwd=str(path))
    else:
        subprocess.check_call(args + ["clone", repo, str(path)])

    subprocess.check_call([hg, "update", "-r", revision], cwd=str(path))

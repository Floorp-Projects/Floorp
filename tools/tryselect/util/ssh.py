# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import subprocess


def get_ssh_user(host="hg.mozilla.org"):
    ssh_config = subprocess.run(
        ["ssh", "-G", host],
        text=True,
        check=True,
        capture_output=True,
    ).stdout

    lines = [l.strip() for l in ssh_config.splitlines()]
    for line in lines:
        if not line:
            continue
        key, value = line.split(" ", 1)
        if key.lower() == "user":
            return value

    raise Exception(f"Could not detect ssh user for '{host}'!")

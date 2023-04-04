# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import sys
import time


def log_build_task(f, *args, **kwargs):
    """Run the given function, representing an entire build task, and log the
    BUILDTASK metadata row to stdout.
    """
    start = time.monotonic()
    try:
        return f(*args, **kwargs)
    finally:
        end = time.monotonic()
        print(
            "BUILDTASK %s"
            % json.dumps(
                {"argv": sys.argv, "start": start, "end": end, "context": None}
            )
        )

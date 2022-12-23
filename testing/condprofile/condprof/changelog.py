# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This module needs to stay Python 2 and 3 compatible
#
"""
Maintains a unique file that lists all artifacts operations.
"""
import json
import os
import sys
from datetime import datetime


# XXX we should do one per platform and use platform-changelog.json as a name
class Changelog:
    def __init__(self, archives_dir):
        self.archives_dir = archives_dir
        self.location = os.path.join(archives_dir, "changelog.json")
        if os.path.exists(self.location):
            with open(self.location) as f:
                self._data = json.loads(f.read())
                if "changes" not in self._data:
                    self._data["changes"] = []
        else:
            self._data = {"changes": []}

    def append(self, action, platform=sys.platform, **metadata):
        now = datetime.timestamp(datetime.now())
        log = {"action": action, "platform": platform, "when": now}
        log.update(metadata)
        # adding taskcluster specific info if we see it in the env
        for key in (
            "TC_SCHEDULER_ID",
            "TASK_ID",
            "TC_OWNER",
            "TC_SOURCE",
            "TC_PROJECT",
        ):
            if key in os.environ:
                log[key] = os.environ[key]
        self._data["changes"].append(log)

    def save(self, archives_dir=None):
        if archives_dir is not None and archives_dir != self.archives_dir:
            self.location = os.path.join(archives_dir, "changelog.json")
        # we need to resolve potential r/w conflicts on TC here
        with open(self.location, "w") as f:
            f.write(json.dumps(self._data))

    def history(self):
        """From older to newer"""
        return sorted(
            self._data["changes"], key=lambda entry: entry["when"], reverse=True
        )

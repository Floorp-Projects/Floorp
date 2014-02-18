# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json


class BaseFormatter(object):
    def __call__(self, data):
        if hasattr(self, data["action"]):
            handler = getattr(self, data["action"])
            return handler(data)


def format_file(input_file, handler):
    while True:
        line = input_file.readline()
        if not line:
            break
        try:
            data = json.loads(line.strip())
            formatter(data)
        except:
            pass

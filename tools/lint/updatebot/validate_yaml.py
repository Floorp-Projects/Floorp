# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozlint.types import FileType
from mozbuild.vendor.moz_yaml import load_moz_yaml


class UpdatebotValidator(FileType):
    def __init__(self, *args, **kwargs):
        super(UpdatebotValidator, self).__init__(*args, **kwargs)

    def lint_single_file(self, payload, path, config):
        if not path.endswith("moz.yaml") and "test/files/updatebot" not in path:
            return None

        try:
            load_moz_yaml(path)
            return None
        except Exception as e:
            return f"Could not load {path} according to schema in moz_yaml.py: {e}"


def lint(paths, config, **lintargs):
    results = []

    m = UpdatebotValidator()
    for path in paths:
        results.extend(m._lint(path, config, **lintargs))
    return results

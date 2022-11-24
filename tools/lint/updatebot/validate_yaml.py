# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozbuild.vendor.moz_yaml import load_moz_yaml
from mozlint import result
from mozlint.pathutils import expand_exclusions


class UpdatebotValidator:
    def lint_file(self, path, **kwargs):
        if not kwargs.get("testing", False) and not path.endswith("moz.yaml"):
            # When testing, process all files provided
            return None
        if not kwargs.get("testing", False) and "test/files/updatebot" in path:
            # When not testing, ignore the test files
            return None

        try:
            yaml = load_moz_yaml(path)

            if "vendoring" in yaml and yaml["vendoring"].get("flavor", None) == "rust":
                yaml_revision = yaml["origin"]["revision"]

                with open("Cargo.lock", "r") as f:
                    for line in f:
                        if yaml_revision in line:
                            return None

                return f"Revision {yaml_revision} specified in {path} wasn't found in Cargo.lock"

            return None
        except Exception as e:
            return f"Could not load {path} according to schema in moz_yaml.py: {e}"


def lint(paths, config, **lintargs):
    # expand_exclusions expects a list, and will convert a string
    # into it if it doesn't receive one
    if not isinstance(paths, list):
        paths = [paths]

    errors = []
    files = list(expand_exclusions(paths, config, lintargs["root"]))

    m = UpdatebotValidator()
    for f in files:
        message = m.lint_file(f, **lintargs)
        if message:
            errors.append(result.from_config(config, path=f, message=message))

    return errors

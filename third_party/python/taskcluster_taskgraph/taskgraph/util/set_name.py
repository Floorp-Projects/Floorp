# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Define a collection of set_name functions
# Note: this is stored here instead of where it is used in the `from_deps`
# transform to give consumers a chance to register their own `set_name`
# handlers before the `from_deps` schema is created.
SET_NAME_MAP = {}


def set_name(name, schema=None):
    def wrapper(func):
        assert (
            name not in SET_NAME_MAP
        ), f"duplicate set_name function name {name} ({func} and {SET_NAME_MAP[name]})"
        SET_NAME_MAP[name] = func
        func.schema = schema
        return func

    return wrapper


@set_name("strip-kind")
def set_name_strip_kind(config, tasks, primary_dep, primary_kind):
    if primary_dep.label.startswith(primary_kind):
        return primary_dep.label[len(primary_kind) + 1 :]
    else:
        return primary_dep.label


@set_name("retain-kind")
def set_name_retain_kind(config, tasks, primary_dep, primary_kind):
    return primary_dep.label

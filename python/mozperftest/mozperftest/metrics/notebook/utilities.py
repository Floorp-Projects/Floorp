# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from collections.abc import Iterable


def flat(data, parent_dir):
    """
    Converts a dictionary with nested entries like this
        {
            "dict1": {
                "dict2": {
                    "key1": value1,
                    "key2": value2,
                    ...
                },
                ...
            },
            ...
            "dict3": {
                "key3": value3,
                "key4": value4,
                ...
            }
            ...
        }

    to a "flattened" dictionary like this that has no nested entries:
        {
            "dict1.dict2.key1": value1,
            "dict1.dict2.key2": value2,
            ...
            "dict3.key3": value3,
            "dict3.key4": value4,
            ...
        }

    :param Iterable data : json data.
    :param tuple parent_dir: json fields.

    :return dict: {subtest: value}
    """
    ret = {}

    def _helper(data, parent_dir):
        if isinstance(data, list):
            for item in data:
                _helper(item, parent_dir)
        elif isinstance(data, dict):
            for k, v in data.items():
                current_dir = parent_dir + (k,)
                subtest = ".".join(current_dir)
                if isinstance(v, Iterable) and not isinstance(v, str):
                    _helper(v, current_dir)
                elif v or v == 0:
                    ret.setdefault(subtest, []).append(v)

    _helper(data, parent_dir)
    return ret

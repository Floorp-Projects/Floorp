# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.util.copy import deepcopy


def merge_to(source, dest):
    """
    Merge dict and arrays (override scalar values)

    Keys from source override keys from dest, and elements from lists in source
    are appended to lists in dest.

    :param dict source: to copy from
    :param dict dest: to copy to (modified in place)
    """

    for key, value in source.items():
        if (
            isinstance(value, dict)
            and len(value) == 1
            and list(value)[0].startswith("by-")
        ):
            # Do not merge by-* values as this is likely to confuse someone
            dest[key] = value
            continue

        # Override mismatching or empty types
        if type(value) != type(dest.get(key)):  # noqa
            dest[key] = value
            continue

        # Merge dict
        if isinstance(value, dict):
            merge_to(value, dest[key])
            continue

        if isinstance(value, list):
            dest[key] = dest[key] + value
            continue

        dest[key] = value

    return dest


def merge(*objects):
    """
    Merge the given objects, using the semantics described for merge_to, with
    objects later in the list taking precedence.  From an inheritance
    perspective, "parents" should be listed before "children".

    Returns the result without modifying any arguments.
    """
    if len(objects) == 1:
        return deepcopy(objects[0])
    return merge_to(objects[-1], merge(*objects[:-1]))

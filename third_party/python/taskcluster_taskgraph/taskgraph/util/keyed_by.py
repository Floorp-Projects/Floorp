# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from .attributes import keymatch


def evaluate_keyed_by(
    value, item_name, attributes, defer=None, enforce_single_match=True
):
    """
    For values which can either accept a literal value, or be keyed by some
    attributes, perform that lookup and return the result.

    For example, given item::

        by-test-platform:
            macosx-10.11/debug: 13
            win.*: 6
            default: 12

    a call to `evaluate_keyed_by(item, 'thing-name', {'test-platform': 'linux96')`
    would return `12`.

    Items can be nested as deeply as desired::

        by-test-platform:
            win.*:
                by-project:
                    ash: ..
                    cedar: ..
            linux: 13
            default: 12

    Args:
        value (str): Name of the value to perform evaluation on.
        item_name (str): Used to generate useful error messages.
        attributes (dict): Dictionary of attributes used to lookup 'by-<key>' with.
        defer (list):
            Allows evaluating a by-* entry at a later time. In the example
            above it's possible that the project attribute hasn't been set yet,
            in which case we'd want to stop before resolving that subkey and
            then call this function again later. This can be accomplished by
            setting `defer=["project"]` in this example.
        enforce_single_match (bool):
            If True (default), each task may only match a single arm of the
            evaluation.
    """
    while True:
        if not isinstance(value, dict) or len(value) != 1:
            return value
        value_key = next(iter(value))
        if not value_key.startswith("by-"):
            return value

        keyed_by = value_key[3:]  # strip off 'by-' prefix

        if defer and keyed_by in defer:
            return value

        key = attributes.get(keyed_by)
        alternatives = next(iter(value.values()))

        if len(alternatives) == 1 and "default" in alternatives:
            # Error out when only 'default' is specified as only alternatives,
            # because we don't need to by-{keyed_by} there.
            raise Exception(
                "Keyed-by '{}' unnecessary with only value 'default' "
                "found, when determining item {}".format(keyed_by, item_name)
            )

        if key is None:
            if "default" in alternatives:
                value = alternatives["default"]
                continue
            else:
                raise Exception(
                    "No attribute {} and no value for 'default' found "
                    "while determining item {}".format(keyed_by, item_name)
                )

        matches = keymatch(alternatives, key)
        if enforce_single_match and len(matches) > 1:
            raise Exception(
                "Multiple matching values for {} {!r} found while "
                "determining item {}".format(keyed_by, key, item_name)
            )
        elif matches:
            value = matches[0]
            continue

        raise Exception(
            "No {} matching {!r} nor 'default' found while determining item {}".format(
                keyed_by, key, item_name
            )
        )

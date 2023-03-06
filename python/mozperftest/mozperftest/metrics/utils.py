# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import ast
import json
import os
import pathlib
import re

from jsonschema import validate
from jsonschema.exceptions import ValidationError

# Get the jsonschema for intermediate results
PARENT = pathlib.Path(__file__).parent.parent
with pathlib.Path(PARENT, "schemas", "intermediate-results-schema.json").open() as f:
    IR_SCHEMA = json.load(f)


# These are the properties we know about in the schema.
# If anything other than these is present, then we will
# fail validation.
KNOWN_PERFHERDER_PROPS = set(
    ["name", "value", "unit", "lowerIsBetter", "shouldAlert", "alertThreshold"]
)
KNOWN_SUITE_PROPS = set(
    set(["results", "transformer", "transformer-options", "extraOptions", "framework"])
    | KNOWN_PERFHERDER_PROPS
)
KNOWN_SINGLE_MEASURE_PROPS = set(set(["values"]) | KNOWN_PERFHERDER_PROPS)


# Regex splitter for the metric fields - used to handle
# the case when `,` is found within the options values.
METRIC_SPLITTER = re.compile(r",\s*(?![^\[\]]*\])")


def is_number(value):
    """Determines if the value is an int/float."""
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def has_callable_method(obj, method_name):
    """Determines if an object/class has a callable method."""
    if obj and hasattr(obj, method_name) and callable(getattr(obj, method_name)):
        return True
    return False


def open_file(path):
    """Opens a file and returns its contents.

    :param path str: Path to the file, if it's a
        JSON, then a dict will be returned, otherwise,
        the raw contents (not split by line) will be
        returned.
    :return dict/str: Returns a dict for JSON data, and
        a str for any other type.
    """
    print("Reading %s" % path)
    with open(path) as f:
        if os.path.splitext(path)[-1] == ".json":
            return json.load(f)
        return f.read()


def write_json(data, path, file):
    """Writes data to a JSON file.

    :param data dict: Data to write.
    :param path str: Directory of where the data will be stored.
    :param file str: Name of the JSON file.
    :return str: Path to the output.
    """
    path = os.path.join(path, file)
    with open(path, "w+") as f:
        json.dump(data, f)
    return path


def validate_intermediate_results(results):
    """Validates intermediate results coming from the browser layer.

    This method exists because there is no reasonable method to implement
    inheritance with `jsonschema` until the `unevaluatedProperties` field
    is implemented in the validation module. Until then, this method
    checks to make sure that only known properties are available in the
    results. If any property found is unknown, then we raise a
    jsonschema.ValidationError.

    :param results dict: The intermediate results to validate.
    :raises ValidationError: Raised when validation fails.
    """
    # Start with the standard validation
    validate(results, IR_SCHEMA)

    # Now ensure that we have no extra keys
    suite_keys = set(list(results.keys()))
    unknown_keys = suite_keys - KNOWN_SUITE_PROPS
    if unknown_keys:
        raise ValidationError(f"Found unknown suite-level keys: {list(unknown_keys)}")
    if isinstance(results["results"], str):
        # Nothing left to verify
        return

    # The results are split by measurement so we need to
    # check that each of those entries have no extra keys
    for entry in results["results"]:
        measurement_keys = set(list(entry.keys()))
        unknown_keys = measurement_keys - KNOWN_SINGLE_MEASURE_PROPS
        if unknown_keys:
            raise ValidationError(
                "Found unknown single-measure-level keys for "
                f"{entry['name']}: {list(unknown_keys)}"
            )


def metric_fields(value):
    # old form: just the name
    if "," not in value and ":" not in value:
        return {"name": value}

    def _check(field):
        sfield = field.strip().partition(":")
        if len(sfield) != 3 or not (sfield[1] and sfield[2]):
            raise ValueError(f"Unexpected metrics definition {field}")
        if sfield[0] not in KNOWN_SUITE_PROPS:
            raise ValueError(
                f"Unknown field '{sfield[0]}', should be in " f"{KNOWN_SUITE_PROPS}"
            )

        sfield = [sfield[0], sfield[2]]

        try:
            # This handles dealing with parsing lists
            # from a string
            sfield[1] = ast.literal_eval(sfield[1])
        except (ValueError, SyntaxError):
            # Ignore failures, those are from instances
            # which don't need to be converted from a python
            # representation
            pass

        return sfield

    fields = [field.strip() for field in METRIC_SPLITTER.split(value)]
    res = dict([_check(field) for field in fields])
    if "name" not in res:
        raise ValueError(f"{value} misses the 'name' field")
    return res

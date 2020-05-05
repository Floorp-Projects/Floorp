# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import json
import pathlib

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
    set(["results", "transformer", "extraOptions"]) | KNOWN_PERFHERDER_PROPS
)
KNOWN_SINGLE_MEASURE_PROPS = set(set(["values"]) | KNOWN_PERFHERDER_PROPS)


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

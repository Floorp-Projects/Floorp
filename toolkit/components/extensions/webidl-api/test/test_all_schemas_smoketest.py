# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import helpers  # Import test helpers module.
import mozunit

helpers.setup()

from GenerateWebIDLBindings import load_and_parse_JSONSchema


def test_all_jsonschema_load_and_parse_smoketest():
    """Make sure it can load and parse all JSONSchema files successfully"""
    schemas = load_and_parse_JSONSchema()
    assert schemas
    assert len(schemas.json_schemas) > 0
    assert len(schemas.api_namespaces) > 0


if __name__ == "__main__":
    mozunit.main()

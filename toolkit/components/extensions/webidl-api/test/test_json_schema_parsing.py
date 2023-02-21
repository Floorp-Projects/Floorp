# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
from textwrap import dedent

import helpers  # Import test helpers module.
import mozunit
import pytest

helpers.setup()

from GenerateWebIDLBindings import APIEvent, APIFunction, APINamespace, APIType, Schemas


def test_parse_simple_single_api_namespace(write_jsonschema_fixtures):
    """
    Test Basic loading and parsing a single API JSONSchema:
    - single line comments outside of the json structure are ignored
    - parse a simple namespace that includes one permission, type,
      function and event
    """
    schema_dir = write_jsonschema_fixtures(
        {
            "test_api.json": dedent(
                """
          // Single line comments added before the JSON data are tolerated
          // and ignored.
          [
            {
              "namespace": "fantasyApi",
              "permissions": ["fantasyPermission"],
              "types": [
                  {
                    "id": "MyType",
                    "type": "string",
                    "choices": ["value1", "value2"]
                  }
              ],
              "functions": [
                {
                  "name": "myMethod",
                  "type": "function",
                  "parameters": [
                    { "name": "fnParam", "type": "string" },
                    { "name": "fnRefParam", "$ref": "MyType" }
                  ]
                }
              ],
              "events": [
                {
                  "name": "onSomeEvent",
                  "type": "function",
                  "parameters": [
                     { "name": "evParam", "type": "string" },
                     { "name": "evRefParam", "$ref": "MyType" }
                  ]
                }
              ]
            }
          ]
        """
            ),
        }
    )

    schemas = Schemas()
    schemas.load_schemas(schema_dir, "toolkit")

    assert schemas.get_all_namespace_names() == ["fantasyApi"]

    apiNs = schemas.api_namespaces["fantasyApi"]
    assert isinstance(apiNs, APINamespace)

    # Properties related to where the JSON schema is coming from
    # (toolkit, browser or mobile schema directories).
    assert apiNs.in_toolkit
    assert not apiNs.in_browser
    assert not apiNs.in_mobile

    # api_path_string is expected to be exactly the namespace name for
    # non-nested API namespaces.
    assert apiNs.api_path_string == "fantasyApi"

    # parse the schema and verify it includes the expected types events and function.
    schemas.parse_schemas()

    assert set(["fantasyPermission"]) == apiNs.permissions
    assert ["MyType"] == list(apiNs.types.keys())
    assert ["myMethod"] == list(apiNs.functions.keys())
    assert ["onSomeEvent"] == list(apiNs.events.keys())

    type_entry = apiNs.types.get("MyType")
    fn_entry = apiNs.functions.get("myMethod")
    ev_entry = apiNs.events.get("onSomeEvent")

    assert isinstance(type_entry, APIType)
    assert isinstance(fn_entry, APIFunction)
    assert isinstance(ev_entry, APIEvent)


def test_parse_error_on_types_without_id_or_extend(
    base_schema, write_jsonschema_fixtures
):
    """
    Test parsing types without id or $extend raise an error while parsing.
    """
    schema_dir = write_jsonschema_fixtures(
        {
            "test_broken_types.json": json.dumps(
                [
                    {
                        **base_schema(),
                        "namespace": "testBrokenTypeAPI",
                        "types": [
                            {
                                # type with no "id2 or "$ref" properties
                            }
                        ],
                    }
                ]
            )
        }
    )

    schemas = Schemas()
    schemas.load_schemas(schema_dir, "toolkit")

    with pytest.raises(
        Exception,
        match=r"Error loading schema type data defined in 'toolkit testBrokenTypeAPI'",
    ):
        schemas.parse_schemas()


def test_parse_ignores_unsupported_types(base_schema, write_jsonschema_fixtures):
    """
    Test parsing types without id or $extend raise an error while parsing.
    """
    schema_dir = write_jsonschema_fixtures(
        {
            "test_broken_types.json": json.dumps(
                [
                    {
                        **base_schema(),
                        "namespace": "testUnsupportedTypesAPI",
                        "types": [
                            {
                                "id": "AnUnsupportedType",
                                "type": "string",
                                "unsupported": True,
                            },
                            {
                                # missing id or $ref shouldn't matter
                                # no parsing error expected.
                                "unsupported": True,
                            },
                            {"id": "ASupportedType", "type": "string"},
                        ],
                    }
                ]
            )
        }
    )

    schemas = Schemas()
    schemas.load_schemas(schema_dir, "toolkit")
    schemas.parse_schemas()
    apiNs = schemas.api_namespaces["testUnsupportedTypesAPI"]
    assert set(apiNs.types.keys()) == set(["ASupportedType"])


def test_parse_error_on_namespace_with_inconsistent_max_manifest_version(
    base_schema, write_jsonschema_fixtures, tmpdir
):
    """
    Test parsing types without id or $extend raise an error while parsing.
    """
    browser_schema_dir = os.path.join(tmpdir, "browser")
    mobile_schema_dir = os.path.join(tmpdir, "mobile")
    os.mkdir(browser_schema_dir)
    os.mkdir(mobile_schema_dir)

    base_namespace_schema = {
        **base_schema(),
        "namespace": "testInconsistentMaxManifestVersion",
    }

    browser_schema = {**base_namespace_schema, "max_manifest_version": 2}
    mobile_schema = {**base_namespace_schema, "max_manifest_version": 3}

    write_jsonschema_fixtures(
        {"test_inconsistent_maxmv.json": json.dumps([browser_schema])},
        browser_schema_dir,
    )

    write_jsonschema_fixtures(
        {"test_inconsistent_maxmv.json": json.dumps([mobile_schema])}, mobile_schema_dir
    )

    schemas = Schemas()
    schemas.load_schemas(browser_schema_dir, "browser")
    schemas.load_schemas(mobile_schema_dir, "mobile")

    with pytest.raises(
        TypeError,
        match=r"Error loading schema data - overwriting existing max_manifest_version value",
    ):
        schemas.parse_schemas()


if __name__ == "__main__":
    mozunit.main()

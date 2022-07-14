# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

import mozpack.path as mozpath
import mozunit

from textwrap import dedent

# Import test helpers module.
OUR_DIR = mozpath.abspath(mozpath.dirname(__file__))
sys.path.append(OUR_DIR)

import helpers

helpers.setup()

from GenerateWebIDLBindings import (
    APIFunction,
    Schemas,
    WebIDLHelpers,
    WEBEXT_STUBS_MAPPING,
)

original_stub_mapping_config = WEBEXT_STUBS_MAPPING.copy()


def teardown_function():
    WEBEXT_STUBS_MAPPING.clear()
    for key in original_stub_mapping_config:
        WEBEXT_STUBS_MAPPING[key] = original_stub_mapping_config[key]


def test_ambiguous_stub_mappings(write_jsonschema_fixtures):
    """
    Test generated webidl for methods that are either
    - being marked as ambiguous because of the "allowAmbiguousOptionalArguments" property
      in their JSONSchema definition
    - mapped to "AsyncAmbiguous" stub per WEBEXT_STUBS_MAPPING python script config
    """

    schema_dir = write_jsonschema_fixtures(
        {
            "test_api.json": dedent(
                """
      [
        {
          "namespace": "testAPINamespace",
          "functions": [
            {
              "name": "jsonSchemaAmbiguousMethod",
              "type": "function",
              "allowAmbiguousOptionalArguments": true,
              "async": true,
              "parameters": [
                {"type": "any", "name": "param1", "optional": true},
                {"type": "any", "name": "param2", "optional": true},
                {"type": "string", "name": "param3", "optional": true}
              ]
            },
            {
              "name": "configuredAsAmbiguousMethod",
              "type": "function",
              "async": "callback",
              "parameters": [
                {"name": "param1", "optional": true, "type": "object"},
                {"name": "callback", "type": "function", "parameters": []}
              ]
            }
          ]
        }
      ]
      """
            )
        }
    )

    assert "testAPINamespace.configuredAsAmbiguousMethod" not in WEBEXT_STUBS_MAPPING
    # NOTE: mocked config reverted in the teardown_method pytest hook.
    WEBEXT_STUBS_MAPPING[
        "testAPINamespace.configuredAsAmbiguousMethod"
    ] = "AsyncAmbiguous"

    schemas = Schemas()
    schemas.load_schemas(schema_dir, "toolkit")

    assert schemas.get_all_namespace_names() == ["testAPINamespace"]
    schemas.parse_schemas()

    apiNs = schemas.get_namespace("testAPINamespace")
    fnAmbiguousBySchema = apiNs.functions.get("jsonSchemaAmbiguousMethod")

    assert isinstance(fnAmbiguousBySchema, APIFunction)
    generated_webidl = WebIDLHelpers.to_webidl_definition(fnAmbiguousBySchema, None)
    expected_webidl = "\n".join(
        [
            '  [Throws, WebExtensionStub="AsyncAmbiguous"]',
            "  any jsonSchemaAmbiguousMethod(any... args);",
        ]
    )
    assert generated_webidl == expected_webidl

    fnAmbiguousByConfig = apiNs.functions.get("configuredAsAmbiguousMethod")
    assert isinstance(fnAmbiguousByConfig, APIFunction)
    generated_webidl = WebIDLHelpers.to_webidl_definition(fnAmbiguousByConfig, None)
    expected_webidl = "\n".join(
        [
            '  [Throws, WebExtensionStub="AsyncAmbiguous"]',
            "  any configuredAsAmbiguousMethod(any... args);",
        ]
    )
    assert generated_webidl == expected_webidl


if __name__ == "__main__":
    mozunit.main()

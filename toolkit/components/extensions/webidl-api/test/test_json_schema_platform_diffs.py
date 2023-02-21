# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import types
from textwrap import dedent

import helpers  # Import test helpers module.
import mozunit

helpers.setup()

from GenerateWebIDLBindings import Schemas
from InspectJSONSchema import run_inspect_command


def test_inspect_schema_platform_diffs(capsys, write_jsonschema_fixtures, tmpdir):
    """
    Test InspectJSONSchema --dump-platform-diff command.
    """
    browser_schema_dir = os.path.join(tmpdir, "browser")
    mobile_schema_dir = os.path.join(tmpdir, "mobile")
    os.mkdir(browser_schema_dir)
    os.mkdir(mobile_schema_dir)

    write_jsonschema_fixtures(
        {
            "test_api_browser.json": dedent(
                """
                [
                  {
                    "namespace": "apiWithDiff",
                    "functions": [
                      {
                        "name": "sharedMethod",
                        "type": "function",
                        "parameters": [
                          { "name": "sharedParam", "type": "string" },
                          { "name": "desktopOnlyMethodParam", "type": "string" }
                        ]
                      },
                      {
                        "name": "desktopMethod",
                        "type": "function",
                        "parameters": []
                      }
                    ],
                    "events": [
                      {
                        "name": "onSharedEvent",
                        "type": "function",
                        "parameters": [
                           { "name": "sharedParam", "type": "string" },
                           { "name": "desktopOnlyEventParam", "type": "string" }
                        ]
                      }
                    ],
                    "properties": {
                      "sharedProperty": { "type": "string", "value": "desktop-value" },
                      "desktopOnlyProperty": { "type": "string", "value": "desktop-only-value" }
                    }
                  }
                ]
                """
            )
        },
        browser_schema_dir,
    )

    write_jsonschema_fixtures(
        {
            "test_api_mobile.json": dedent(
                """
                [
                  {
                    "namespace": "apiWithDiff",
                    "functions": [
                      {
                        "name": "sharedMethod",
                        "type": "function",
                        "parameters": [
                          { "name": "sharedParam", "type": "string" },
                          { "name": "mobileOnlyMethodParam", "type": "string" }
                        ]
                      },
                      {
                        "name": "mobileMethod",
                        "type": "function",
                        "parameters": []
                      }
                    ],
                    "events": [
                      {
                        "name": "onSharedEvent",
                        "type": "function",
                        "parameters": [
                           { "name": "sharedParam", "type": "string" },
                           { "name": "mobileOnlyEventParam", "type": "string" }
                        ]
                      }
                    ],
                    "properties": {
                      "sharedProperty": { "type": "string", "value": "mobile-value" },
                      "mobileOnlyProperty": { "type": "string", "value": "mobile-only-value" }
                    }
                  }
                ]
                """
            )
        },
        mobile_schema_dir,
    )

    schemas = Schemas()
    schemas.load_schemas(browser_schema_dir, "browser")
    schemas.load_schemas(mobile_schema_dir, "mobile")

    assert schemas.get_all_namespace_names() == ["apiWithDiff"]
    apiNs = schemas.api_namespaces["apiWithDiff"]
    assert apiNs.in_browser
    assert apiNs.in_mobile

    apiNs.parse_schemas()

    fakeArgs = types.SimpleNamespace()
    fakeArgs.dump_namespaces_info = False
    fakeArgs.dump_platform_diffs = True
    fakeArgs.only_if_webidl_diffs = False
    fakeArgs.diff_command = None

    fakeParser = types.SimpleNamespace()
    fakeParser.print_help = lambda: None

    run_inspect_command(fakeArgs, schemas, fakeParser)

    captured = capsys.readouterr()
    assert "API schema desktop vs. mobile for apiWithDiff.sharedMethod" in captured.out
    assert '-   "name": "desktopOnlyMethodParam",' in captured.out
    assert '+   "name": "mobileOnlyMethodParam",' in captured.out
    assert "API schema desktop vs. mobile for apiWithDiff.onSharedEvent" in captured.out
    assert '-   "name": "desktopOnlyEventParam",' in captured.out
    assert '+   "name": "mobileOnlyEventParam",' in captured.out
    assert (
        "API schema desktop vs. mobile for apiWithDiff.sharedProperty" in captured.out
    )
    assert '- "value": "desktop-value"' in captured.out
    assert '+ "value": "mobile-value"' in captured.out
    assert captured.err == ""


if __name__ == "__main__":
    mozunit.main()

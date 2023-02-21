pytest test coverage for the GenerateWebIDLBindings.py script
=============================================================

This directory contains tests for the GenerateWebIDLBindings.py script,
which is used to parse the WebExtensions APIs schema files and generate
the corresponding WebIDL definitions.

See ["WebIDL WebExtensions API Bindings" section from the Firefox Developer documentation](https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/webidl_bindings.html)
for more details about how the script is used, this README covers only how
this test suite works.

Run tests
---------

The tests part of this test suite can be executed locally using the following `mach` command:

```
mach python-test toolkit/components/extensions/webidl-api/test
```

Write a new test file
---------------------

To add a new test file to this test suite:
- create a new python script file named as `test_....py`
- add the test file to the `python.ini` manifest
- In the new test file make sure to include:
  - copyright notes as the other test file in this directory
  - import the helper module and call its `setup()` method (`setup` makes sure to add
    the directory where the target script is in the python library paths and the
    `helpers` module does also enable the code coverage if the environment variable
    is detected):
    ```
    import helpers  # Import test helpers module.
    ...

    helpers.setup()
    ```
  - don't forget to call `mozunit.main` at the end of the test file:
    ```
    if __name__ == "__main__":
        mozunit.main()
    ```
  - add new test cases by defining new functions named as `test_...`,
    its parameter are the names of the pytest fixture functions to
    be passed to the test case:
    ```
    def test_something(base_schema, write_jsonschema_fixtures):
        ...
    ```
Create new test fixtures
------------------------

All the test fixture used by this set of tests are defined in `conftest.py`
and decorated with `@pytest.fixture`.

See the pytest documentation for more details about how the pytest fixture works:
- https://docs.pytest.org/en/latest/explanation/fixtures.html
- https://docs.pytest.org/en/latest/how-to/fixtures.html
- https://docs.pytest.org/en/latest/reference/fixtures.html

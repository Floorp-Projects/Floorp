# Updating glean_parser

Project FOG uses the `glean_parser` to generate code from metric definitions.
It depends on [glean-parser] from pypi.org

[glean-parser]: https://pypi.org/project/glean-parser/

To update the in-tree glean-parser run

```
./mach vendor python glean_parser==M.m.p
```

where `M.m.p` is the version number, e.g. 1.28.0.

```eval_rst
.. note::

    **Important**: the glean_parser and all of its dependencies must support Python 3.5, as discussed here.
    This is the minimum version supported by mach and installed on the CI images for running tests.
    This is enforced by the version ranges declared in the Python installation manifest.
```

## Version mismatch of the Python dependencies

The logic for handling version mismatches is very similar to the one for the Rust crates.
See [Updating the Glean SDK](updating_sdk.html) for details.
However, updating Python packages also requires to think about Python 3.5 (and Python 2, still) compatibility.

## Keeping versions in sync

The Glean SDK and `glean_parser` are currently released as separate projects.
However each Glean SDK release requires a specific `glean_parser` version.
When updating one or the other ensure versions stay compatible.
You can find the currently used `glean_parser` version in the Glean SDK source tree, e.g. in [sdk_generator.sh].

[sdk_generator.sh]: https://github.com/mozilla/glean/blob/main/glean-core/ios/sdk_generator.sh#L28

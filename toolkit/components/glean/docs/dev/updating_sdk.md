# Updating the Glean SDK

Project FOG uses the Glean SDK published as the [`glean`][glean-crate]
and [`glean-core`][glean-core] crates on crates.io.

[glean-crate]: https://crates.io/crates/glean
[glean-core]: https://crates.io/crates/glean-core

These two crates' versions are included in several places in mozilla-central.
To update them all, you should use the command
`mach update-glean <version, like "54.1.0">`.

This is a semi-manual process.
Please pay attention to the output of `mach update-glean` for instructions,
and follow them closely.

## Version mismatches of Rust dependencies

Other crates that are already vendored might require a different version of the same dependencies that the Glean SDK requires.
The general strategy for Rust dependencies is to keep one single version of the dependency in-tree
(see [comment #8 in bug 1591555](https://bugzilla.mozilla.org/show_bug.cgi?id=1591555#c8)).
This might be hard to do in reality since some dependencies might require tweaks in order to work.
The following strategy can be followed to decide on version mismatches:

* If the versions only **differ by the patch version**, Cargo will keep the vendored version,
  unless some other dependency pinned specific patch versions;
  assuming it doesn’t break the Glean SDK;
  if it does, follow the next steps;
* If the version of the **vendored dependency is newer** (greater major or minor version) than the version required by the Glean SDK,
  [file a bug in the Glean SDK component][glean-bug] to get Glean to require the same version;
    * You will have to abandon updating the Glean SDK to this version.
      You will have to wait for Glean SDK to update its dependency and for a new Glean SDK release.
      Then you will have to update to that new Glean SDK version.
* If the version of the **vendored dependency is older** (lower major or minor version), consider updating the vendored version to the newer one;
  seek review from the person who vendored that dependency in the first place;
  if that is not possible or breaks mozilla-central build, then consider keeping both versions vendored in-tree; please note that this option will probably only be approved for small crates,
  and will require updating the `TOLERATED_DUPES` list in `mach vendor`
  (instructions are provided as you go).

## Keeping versions in sync

The Glean SDK and `glean_parser` are currently released as separate projects.
However each Glean SDK release requires a specific `glean_parser` version.
When updating one or the other ensure versions stay compatible.
You can find the currently used `glean_parser` version in the Glean SDK source tree, e.g. in [sdk_generator.sh].

[sdk_generator.sh]: https://github.com/mozilla/glean/blob/main/glean-core/ios/sdk_generator.sh#L28
[glean-bug]: https://bugzilla.mozilla.org/enter_bug.cgi?product=Data+Platform+and+Tools&component=Glean%3A+SDK&priority=P3&status_whiteboard=%5Btelemetry%3Aglean-rs%3Am%3F%5D

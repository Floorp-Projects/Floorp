# Unreleased

# 0.3.3 (2019-12-1)

* Add missing `Hash` implementation for `AndroidHandle`.

# 0.3.2 (2019-11-29)

* Add `Hash` implementation for `RawWindowHandle`.

# 0.3.1 (2019-10-27)

* Remove `RawWindowHandle`'s `HasRawWindowHandle` implementation, as it was unsound (see [#35](https://github.com/rust-windowing/raw-window-handle/issues/35))
* Explicitly require that handles within `RawWindowHandle` be valid for the lifetime of the `HasRawWindowHandle` implementation that provided them.

# 0.3.0 (2019-10-5)

* **Breaking:** Rename `XLib.surface` to `XLib.window`, as that more accurately represents the underlying type.
* Implement `HasRawWindowHandle` for `RawWindowHandle`
* Add `HINSTANCE` field to `WindowsHandle`.

# 0.2.0 (2019-09-26)

* **Breaking:** Rename `X11` to `XLib`.
* Add XCB support.
* Add Web support.
* Add Android support.

# 0.1.2 (2019-08-13)

* Fix use of private `_non_exhaustive` field in platform handle structs preventing structs from getting initialized.

# 0.1.1 (2019-08-13)

* Flesh out Cargo.toml, adding crates.io info rendering tags.

# 0.1.0 (2019-08-13)

* Initial release.

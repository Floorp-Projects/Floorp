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

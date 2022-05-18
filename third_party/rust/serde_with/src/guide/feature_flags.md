# Available Feature Flags

This crate has the following features which can be enabled.
Each entry will explain the feature in more detail.

1. [`chrono`](#chrono)
2. [`guide`](#guide)
3. [`hex`](#hex)
4. [`json`](#json)
5. [`macros`](#macros)

## `chrono`

The `chrono` feature enables integration of `chrono` specific conversions.
This includes support for the timestamp and duration types.

This pulls in `chrono` as a dependency.

## `guide`

The `guide` feature enables inclusion of this user guide.
The feature only changes the rustdoc output and enables no other effects.

## `hex`

The `hex` feature enables serializing data in hex format.

This pulls in `hex` as a dependency.

## `json`

The `json` features enables JSON conversions from the `json` module.

This pulls in `serde_json` as a dependency.

## `macros`

The `macros` features enables all helper macros and derives.
It is enabled by default, since the macros provide a usability benefit, especially for `serde_as`.

This pulls in `serde_with_macros` as a dependency.

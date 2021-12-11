# Developing with a local Glean build

FOG uses a release version of Glean, as published on [crates.io][cratesio-glean].

For local development and try runs you can replace this Glean implementation with a local or remote version.

1. To tell `mach` where to find your Glean, patch the [top-level `Cargo.toml`][cargo-toml]. E.g. like this:

    ```
    glean = { git = "https://github.com/myfork/glean", branch = "my-feature-branch" }
    glean-core = { git = "https://github.com/myfork/glean", branch = "my-feature-branch" }
    ```

    Both crates are required to ensure they are in sync.

    You can specify the exact code to use by `branch`, `tag` or `rev` (Git commit).
    See the [cargo documentation for details][cargo-doc].

    You can also use a path dependency:

    ```
    glean = { path = "../glean/glean-core/rlb" }
    glean_core = { path = "../glean/glean-core" }
    ```

2. If the crate version in the patched repository is not
   [semver]-compatible with the version required by the
   `fog` and `fog_control` crates,
   you need to change the version in the following files to match the ones in your
   `glean` repo:

    ```
    toolkit/components/glean/Cargo.toml
    toolkit/components/glean/api/Cargo.toml
    ```

3. Update the Cargo lockfile:

    ```
    cargo update -p glean
    ```

4. Vendor the changed crates:

    ```
    ./mach vendor rust
    ```

    **Note:** If you're using a path dependency, `mach vendor rust` doesn't actually change files.
    Instead it pulls the files directly from the location on disk you specify.

5. Finally build Firefox:

    ```
    ./mach build
    ```

A remote reference works for try runs as well,
but a path dependency will not.

Please ensure to not land a non-release version of Glean.

[cratesio-glean]: https://crates.io/crates/glean
[cargo-toml]: https://searchfox.org/mozilla-central/rev/f07a609a76136ef779c65185165ff5ac513cc172/Cargo.toml#76
[cargo-doc]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html#specifying-dependencies-from-git-repositories
[semver]: https://semver.org/

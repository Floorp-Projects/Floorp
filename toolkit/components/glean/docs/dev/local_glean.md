# Developing with a local Glean build

FOG uses a release version of Glean, as published on [crates.io][cratesio-glean].

For local development and try runs you can replace this Glean implementation with a local or remote version.

1. To tell `mach` where to find your Glean, patch the [top-level `Cargo.toml`][cargo-toml]. E.g. like this:

    ```toml
    [patches.crates-io]
    glean = { git = "https://github.com/myfork/glean", branch = "my-feature-branch" }
    glean-core = { git = "https://github.com/myfork/glean", branch = "my-feature-branch" }
    ```

    Both crates are required to ensure they are in sync.

    You can specify the exact code to use by `branch`, `tag` or `rev` (Git commit).
    See the [cargo documentation for details][cargo-doc].

    You can also use a path dependency:

    ```toml
    [patches.crates-io]
    glean = { path = "../glean/glean-core/rlb" }
    glean-core = { path = "../glean/glean-core" }
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

    This tells FOG's crates that it needs your local Glean's version.

3. Update the Cargo lockfile:

    ```
    cargo update -p glean
    ```

4.  Mozilla's supply-chain management policy requires that third-party software
    (which includes the Glean SDK because it is distributed as though it is third-party)
    be audited and certified as safe.
    Your local Glean SDK probably hasn't been vetted. If you try to vendor right now,
    `./mach vendor rust` will complain something like:

    ```
    Vet error: There are some issues with your policy.audit-as-crates-io entries
    ```

    This is because your local Glean SDK is neither of a version nor is from a source that has been vetted.
    To allow your local Glean crates to be treated as crates.io-sourced crates for vetting,
    add the following sections to the top of `supply-chain/config.toml`:

    ```toml
    [policy.glean]
    audit-as-crates-io = true

    [policy.glean-core]
    audit-as-crates-io = true
    ```

    If your local Glean is of a non-vetted version, you can update `glean` and
    `glean-core`'s entries in `supply-chain/audits.toml` to the version you're using.
    If you don't, `./mach vendor rust` will complain and not complete.

    **Note:** Do not attempt to check these changes in.
    These changes bypass supply chain defenses.
    `@supply-chain-reviewers` may become cross as they `r-` your patch.

5. Vendor the changed crates:

    ```
    ./mach vendor rust
    ```

    **Note:** If you're using a path dependency, `mach vendor rust` doesn't actually change files.
    Instead it pulls the files directly from the location on disk you specify.

6. Finally, build Firefox:

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

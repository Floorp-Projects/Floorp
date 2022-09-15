# Releases

We use a GitHub Action to publish a crate named `prio` to [crates.io](https://crates.io). To cut a
release and publish:

- Bump the version number in `Cargo.toml` to e.g. `1.2.3` and merge that change to `main`
- Tag that commit on main as `v1.2.3`, either in `git` or in [GitHub's releases UI][releases].
- Publish a release in [GitHub's releases UI][releases].

Publishing the release will automatically publish the updated [`prio` crate][crate].

[releases]: https://github.com/divviup/libprio-rs/releases/new
[crate]: https://crates.io/crates/prio

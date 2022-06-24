# Dogear

**Dogear** is a library that implements bookmark tree merging for Firefox Sync. It takes two trees—a valid, consistent local tree, and a possibly inconsistent remote tree—and produces a complete merged tree, with all conflicts and inconsistencies resolved.

Dogear implements the merge algorithm only; it doesn't handle syncing, storage, or application. It's up to the crate that embeds Dogear to store local and incoming bookmarks, describe how to build a tree from a storage backend, persist the merged tree back to storage, and upload records for changed bookmarks.

## Requirements

* Rust 1.31.0 or higher


## Updating this package
Once a new version of Dogear is ready to release. The new version will need to be published to [crates.io](https://crates.io/crates/dogear). Dogear follows the documentation detailed in the [Cargo book](https://doc.rust-lang.org/cargo/reference/publishing.html#publishing-a-new-version-of-an-existing-crate).
### Steps to publish a new verison
1. Bump the version in the `Cargo.toml` file
2. Run `cargo publish --dry-run`
    - Validate it does what you want it to do
3. Run `cargo publish` and follow the steps cargo provides

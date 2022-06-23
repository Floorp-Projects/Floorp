# Dogear

**Dogear** is a library that implements bookmark tree merging for Firefox Sync. It takes two trees—a valid, consistent local tree, and a possibly inconsistent remote tree—and produces a complete merged tree, with all conflicts and inconsistencies resolved.

Dogear implements the merge algorithm only; it doesn't handle syncing, storage, or application. It's up to the crate that embeds Dogear to store local and incoming bookmarks, describe how to build a tree from a storage backend, persist the merged tree back to storage, and upload records for changed bookmarks.

## Requirements

* Rust 1.31.0 or higher

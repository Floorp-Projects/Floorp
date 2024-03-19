# Suggest

The **Suggest Rust component** provides address bar search suggestions from Mozilla. This includes suggestions from sponsors, as well as non-sponsored suggestions for other web destinations. These suggestions are part of the [Firefox Suggest](https://support.mozilla.org/en-US/kb/firefox-suggest-faq) feature.

This component is integrated into Firefox Desktop, Android, and iOS.

## Architecture

Search suggestions from Mozilla are stored in a [Remote Settings](https://remote-settings.readthedocs.io/en/latest/) collection. The Suggest component downloads these suggestions from Remote Settings, stores them in a local SQLite database, and makes them available to the Firefox address bar. Because these suggestions are stored and matched locally, Mozilla never sees the user's search queries.

This component follows the architecture of the other [Application Services Rust components](https://mozilla.github.io/application-services/book/index.html): a cross-platform Rust core, and platform-specific bindings for Firefox Desktop, Android, and iOS. These bindings are generated automatically using the [UniFFI](https://mozilla.github.io/uniffi-rs/) tool.

### For consumers

This section is for application developers. It describes how Firefox Desktop, Android, and iOS consume the Suggest Rust component.

The cornerstone of the component is the `SuggestStore` interface, which is the **store**. The store _ingests_ (downloads and persists) suggestions from Remote Settings, and returns matching suggestions as the user types. This is the main interface that applications use to interact with the component.

While the store provides most of the functionality, the application has a few responsibilities:

**1. Create and manage a `SuggestStore` as a singleton.** Under the hood, the store holds multiple connections to the database: a read-write connection for ingestion, and a read-only connection for queries. The store uses the right connection for each operation, so applications shouldn't create multiple stores. The application is responsible for specifying the correct platform-specific storage directory for the database. The database contains _cached data_, like suggestions, and _user data_, like which suggestions have been dismissed. For this reason, applications should persist the database in their durable storage or "profile" directory. Applications specify the storage directory, and create a store, using the `SuggestStoreBuilder` interface.

**2. Periodically call the store's `ingest()` method to ingest new suggestions.** While the store takes care of efficiently downloading and persisting suggestions from Remote Settings, the application is still responsible for scheduling this work. This is because the Suggest component doesn't have access to the various platform-specific background work scheduling mechanisms, like `nsIUpdateTimerManager` on Desktop, `WorkManager` on Android, or `BGTaskScheduler` on iOS. These are three different APIs with different constraints, written in three different languages. Instead of trying to bind to these different mechanisms, the component leaves it up to the application to use the right one on each platform. Ingestion is network- and disk I/O-bound, and should be done in the background.

**3. Use the store's `query()` and `interrupt()` methods to query for fresh suggestions as the user types.** The application passes the user's input, and additional options like which suggestion types to return, to `query()`. Querying the database is disk I/O-bound, so applications should use their respective platforms' facilities for asynchronous work—Kotlin coroutines on Android, and Swift actors on iOS; the UniFFI bindings for Desktop take care of dispatching work to a background thread pool—to avoid blocking the main thread. Running `query()` off-main-thread also lets applications `interrupt()` those queries from the main thread when the user's input changes. This avoids waiting for a query to return suggestions that are now stale.

### For contributors

This section is a primer for engineers contributing code to the Suggest Rust component.

`suggest.udl` describes the component's interface for foreign language consumers. UniFFI uses this file to generate the language bindings for each platform. If you're adding a new suggestion type, you'll want to update the declarations of `Suggestion` and `SuggestionProvider` in this file, as well as the definitions of those types in `suggestion.rs` and `provider.rs`, respectively.

`store.rs` contains the implementation of `SuggestStore` and most of the Suggest component's tests.

`schema.rs` manages the database schema. **Remember to bump the schema version and add a migration whenever you change the schema.**

`db.rs` interacts with the Suggest database. The `SuggestDao` type in this file is a ["data access object"](https://en.wikipedia.org/wiki/Data_access_object) (DAO) that contains all the SQL statements for querying and updating the SQLite database. By convention, `SuggestDao` methods that can write to the database take `&mut self`, and methods that only read take `&self`. The `SuggestDb::read()` and `SuggestDb::write()` methods take a closure that receives either `&SuggestDao` or `&mut SuggestDao`; this is how the DAO enforces that all writes are done in a transaction. If you're curious to learn about how we use SQLite, or you're diagnosing a slow query or adding a new suggestion type, you'll almost certainly want to look at this file.

`rs.rs` defines all the Remote Settings [record](https://docs.kinto-storage.org/en/stable/concepts.html#buckets-collections-and-records) and [attachment](https://docs.kinto-storage.org/en/stable/faq.html#can-i-store-files-inside-kinto) types. The records in the `quicksuggest` Remote Settings collection don't store the suggestions themselves. Instead, each record has a type, and a pointer to a JSON attachment that contains multiple suggestions of that type. This file defines [Serde](https://serde.rs/)-compatible types for all these records and attachments.

`errors.rs` contains all the errors that this component returns. We use the crate-internal `Error` type for all fallible operations within the component, and the public `SuggestApiError` type for errors that applications should handle.

There are other suggestion provider-specific files, like `yelp.rs`, `pocket.rs`, and `keyword.rs`, that aren't covered in this primer. If you're new to the component, we recommend starting with the highest-level interface in `store.rs` first, and jumping to the other files and types as you encounter them in the code.

## Documentation

Each Rust file contains [inline documentation](https://doc.rust-lang.org/rustdoc/what-is-rustdoc.html) for types, traits, functions, and methods.

Documentation for `pub` symbols is written with application developers in mind: even if you're a Desktop, Android or iOS developer, the Rust documentation is meant to give you an understanding of how the Gecko, Kotlin and Swift bindings work.

You can see the documentation for all public symbols by running from the command line:

```shell
cargo doc --open
```

By convention, symbols that are exported as part of the component's foreign language interface have `pub` visibility, and symbols that are only used within the component have `pub(crate)` or private visibility. As an exception to this convention, symbols with [doctests](https://doc.rust-lang.org/rustdoc/write-documentation/documentation-tests.html) are also `pub`, because doctests [can only link against public symbols](https://doc.rust-lang.org/rustdoc/write-documentation/documentation-tests.html#include-items-only-when-collecting-doctests).

If you're working on the component, you can see the documentation for `pub(crate)` and private symbols using:

```shell
cargo doc --document-private-items --open
```

💡 If you're adding a new suggestion type, the documentation for the `rs` module is a great place to start.

Please help us keep our documentation useful for everyone, and feel free to file bugs for anything that looks unclear or out-of-date!

## Tests

We use a technique called ["snapshot testing"](https://notlaura.com/what-is-a-snapshot-test/) with the [`expect-test`](https://docs.rs/expect-test/latest/expect_test/) crate for the Suggest component's tests. This technique makes it easier to compare and update all the expected outputs when adding, removing, and changing suggestion types.

The snapshot tests in `store.rs` look like this:

```rs
expect![["{expected-output}"]].assert_debug_eq(&actual_output);
```

The `expect-test` crate generates the `{expected-output}` string, and can update it automatically. If you add, remove, or change a suggestion type, or update the schema, and run `cargo test`, you'll likely see a few failures. `expect-test` will print a readable diff in the `cargo test` output, which you can audit for accuracy.

If the diff looks good, you can update the expectations in-place from the command line using:

```shell
env UPDATE_EXPECT=1 cargo test
```

Most of the tests in `store.rs` are integration-style tests that use a fake Remote Settings interface.

## ⚠️ Breaking Changes ⚠️

A "breaking change" is any code change that breaks the build, tests, or behavior of a consuming application.

These are some common changes that can break consumers:

* **Changing the signature of a method or function that's currently in use.** Adding, removing, or changing the type of an argument or a return value, or reordering arguments, is a breaking change on Desktop, Android, and iOS.
* **Removing or renaming a method, function, or type that's currently in use.**
* **Adding or removing a `[Throws]` attribute**. Changing a non-throwing function to throw an error, or vice versa, is a build breaking change on iOS. On Desktop and Android, changing a non-throwing function to a throwing function won't break the build, but can cause crashes if the consumer doesn't handle the new error.
* **Changing the fields of a dictionary or an enum.** Adding, removing, reordering, or changing the type of a dictionary or an `[Enum]` interface field is a guaranteed build breaking change on iOS. It may be a breaking change on Desktop and Android if the consumer creates new instances of the dictionary or enum.

When working on the Suggest component, this means:

* Adding a new suggestion type is generally _not_ breaking.
* Adding a new method to `SuggestStore` is _not_ breaking.
* Renaming or removing a `SuggestStore` method that's currently in use _is_ breaking.
* Adding a new field to an existing suggestion type _is_ breaking.

If you need to make a breaking change, don't panic! We have a [process for landing them in Application Services](https://mozilla.github.io/application-services/book/howtos/breaking-changes.html), and you can use a [branch build](https://mozilla.github.io/application-services/book/howtos/branch-builds.html) to verify that Android and iOS build and run their tests with your change.

## Bugs

We use Bugzilla to track bugs and feature work. You can use [this link](https://bugzilla.mozilla.org/enter_bug.cgi?product=Application+Services&component=Suggest) to file bugs in the `Application Services :: Suggest` bug component.

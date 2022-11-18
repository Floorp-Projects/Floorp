// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
#![allow(clippy::from_over_into)] // TODO: `Into` implementations in [safe/lmdb]/flags.rs

//! A simple, humane, typed key-value storage solution. It supports multiple backend
//! engines with varying guarantees, such as [LMDB](http://www.lmdb.tech/doc/) for
//! performance, or "SafeMode" for reliability.
//!
//! It aims to achieve the following:
//!
//! - Avoid sharp edges (e.g., obscure error codes for common situations).
//! - Correctly restrict access to one handle per process via a
//!   [Manager](struct.Manager.html).
//! - Use Rust's type system to make single-typed key stores safe and ergonomic.
//! - Encode and decode values via [bincode](https://docs.rs/bincode/)/[serde](https://docs.rs/serde/)
//!   and type tags, achieving platform-independent storage and input/output flexibility.
//!
//! It exposes these primary abstractions:
//!
//! - [Manager](struct.Manager.html): a singleton that controls access to environments
//! - [Rkv](struct.Rkv.html): an environment contains a set of key/value databases
//! - [SingleStore](store/single/struct.SingleStore.html): a database contains a set of
//!   key/value pairs
//!
//! Keys can be anything that implements `AsRef<[u8]>` or integers
//! (when accessing an [IntegerStore](store/integer/struct.IntegerStore.html)).
//!
//! Values can be any of the types defined by the [Value](value/enum.Value.html) enum,
//! including:
//!
//! - booleans (`Value::Bool`)
//! - integers (`Value::I64`, `Value::U64`)
//! - floats (`Value::F64`)
//! - strings (`Value::Str`)
//! - blobs (`Value::Blob`)
//!
//! See [Value](value/enum.Value.html) for the complete list of supported types.
//!
//! ## Basic Usage
//! ```
//! use rkv::{Manager, Rkv, SingleStore, Value, StoreOptions};
//! use rkv::backend::{SafeMode, SafeModeEnvironment};
//! use std::fs;
//! use tempfile::Builder;
//!
//! // First determine the path to the environment, which is represented on disk as a
//! // directory containing two files:
//! //
//! //   * a data file containing the key/value stores
//! //   * a lock file containing metadata about current transactions
//! //
//! // In this example, we use the `tempfile` crate to create the directory.
//! //
//! let root = Builder::new().prefix("simple-db").tempdir().unwrap();
//! fs::create_dir_all(root.path()).unwrap();
//! let path = root.path();
//!
//! // The `Manager` enforces that each process opens the same environment at most once by
//! // caching a handle to each environment that it opens. Use it to retrieve the handle
//! // to an opened environment—or create one if it hasn't already been opened:
//! let mut manager = Manager::<SafeModeEnvironment>::singleton().write().unwrap();
//! let created_arc = manager.get_or_create(path, Rkv::new::<SafeMode>).unwrap();
//! let env = created_arc.read().unwrap();
//!
//! // Then you can use the environment handle to get a handle to a datastore:
//! let store = env.open_single("mydb", StoreOptions::create()).unwrap();
//!
//! {
//!     // Use a write transaction to mutate the store via a `Writer`. There can be only
//!     // one writer for a given environment, so opening a second one will block until
//!     // the first completes.
//!     let mut writer = env.write().unwrap();
//!
//!     // Keys are `AsRef<[u8]>`, while values are `Value` enum instances. Use the `Blob`
//!     // variant to store arbitrary collections of bytes. Putting data returns a
//!     // `Result<(), StoreError>`, where StoreError is an enum identifying the reason
//!     // for a failure.
//!     store.put(&mut writer, "int", &Value::I64(1234)).unwrap();
//!     store.put(&mut writer, "uint", &Value::U64(1234_u64)).unwrap();
//!     store.put(&mut writer, "float", &Value::F64(1234.0.into())).unwrap();
//!     store.put(&mut writer, "instant", &Value::Instant(1528318073700)).unwrap();
//!     store.put(&mut writer, "boolean", &Value::Bool(true)).unwrap();
//!     store.put(&mut writer, "string", &Value::Str("Héllo, wörld!")).unwrap();
//!     store.put(&mut writer, "json", &Value::Json(r#"{"foo":"bar", "number": 1}"#)).unwrap();
//!     store.put(&mut writer, "blob", &Value::Blob(b"blob")).unwrap();
//!
//!     // You must commit a write transaction before the writer goes out of scope, or the
//!     // transaction will abort and the data won't persist.
//!     writer.commit().unwrap();
//! }
//!
//! {
//!     // Use a read transaction to query the store via a `Reader`. There can be multiple
//!     // concurrent readers for a store, and readers never block on a writer nor other
//!     // readers.
//!     let reader = env.read().expect("reader");
//!
//!     // Keys are `AsRef<u8>`, and the return value is `Result<Option<Value>, StoreError>`.
//!     println!("Get int {:?}", store.get(&reader, "int").unwrap());
//!     println!("Get uint {:?}", store.get(&reader, "uint").unwrap());
//!     println!("Get float {:?}", store.get(&reader, "float").unwrap());
//!     println!("Get instant {:?}", store.get(&reader, "instant").unwrap());
//!     println!("Get boolean {:?}", store.get(&reader, "boolean").unwrap());
//!     println!("Get string {:?}", store.get(&reader, "string").unwrap());
//!     println!("Get json {:?}", store.get(&reader, "json").unwrap());
//!     println!("Get blob {:?}", store.get(&reader, "blob").unwrap());
//!
//!     // Retrieving a non-existent value returns `Ok(None)`.
//!     println!("Get non-existent value {:?}", store.get(&reader, "non-existent").unwrap());
//!
//!     // A read transaction will automatically close once the reader goes out of scope,
//!     // so isn't necessary to close it explicitly, although you can do so by calling
//!     // `Reader.abort()`.
//! }
//!
//! {
//!     // Aborting a write transaction rolls back the change(s).
//!     let mut writer = env.write().unwrap();
//!     store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
//!     writer.abort();
//!     let reader = env.read().expect("reader");
//!     println!("It should be None! ({:?})", store.get(&reader, "foo").unwrap());
//! }
//!
//! {
//!     // Explicitly aborting a transaction is not required unless an early abort is
//!     // desired, since both read and write transactions will implicitly be aborted once
//!     // they go out of scope.
//!     {
//!         let mut writer = env.write().unwrap();
//!         store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
//!     }
//!     let reader = env.read().expect("reader");
//!     println!("It should be None! ({:?})", store.get(&reader, "foo").unwrap());
//! }
//!
//! {
//!     // Deleting a key/value pair also requires a write transaction.
//!     let mut writer = env.write().unwrap();
//!     store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
//!     store.put(&mut writer, "bar", &Value::Str("baz")).unwrap();
//!     store.delete(&mut writer, "foo").unwrap();
//!
//!     // A write transaction also supports reading, and the version of the store that it
//!     // reads includes the changes it has made regardless of the commit state of that
//!     // transaction.

//!     // In the code above, "foo" and "bar" were put into the store, then "foo" was
//!     // deleted so only "bar" will return a result when the database is queried via the
//!     // writer.
//!     println!("It should be None! ({:?})", store.get(&writer, "foo").unwrap());
//!     println!("Get bar ({:?})", store.get(&writer, "bar").unwrap());
//!
//!     // But a reader won't see that change until the write transaction is committed.
//!     {
//!         let reader = env.read().expect("reader");
//!         println!("Get foo {:?}", store.get(&reader, "foo").unwrap());
//!         println!("Get bar {:?}", store.get(&reader, "bar").unwrap());
//!     }
//!     writer.commit().unwrap();
//!     {
//!         let reader = env.read().expect("reader");
//!         println!("It should be None! ({:?})", store.get(&reader, "foo").unwrap());
//!         println!("Get bar {:?}", store.get(&reader, "bar").unwrap());
//!     }
//!
//!     // Committing a transaction consumes the writer, preventing you from reusing it by
//!     // failing at compile time with an error. This line would report "error[E0382]:
//!     // borrow of moved value: `writer`".
//!     // store.put(&mut writer, "baz", &Value::Str("buz")).unwrap();
//! }
//!
//! {
//!     // Clearing all the entries in the store with a write transaction.
//!     {
//!         let mut writer = env.write().unwrap();
//!         store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
//!         store.put(&mut writer, "bar", &Value::Str("baz")).unwrap();
//!         writer.commit().unwrap();
//!     }
//!
//!     {
//!         let mut writer = env.write().unwrap();
//!         store.clear(&mut writer).unwrap();
//!         writer.commit().unwrap();
//!     }
//!
//!     {
//!         let reader = env.read().expect("reader");
//!         println!("It should be None! ({:?})", store.get(&reader, "foo").unwrap());
//!         println!("It should be None! ({:?})", store.get(&reader, "bar").unwrap());
//!     }
//!
//! }
//!
//! ```

mod env;
mod error;
mod helpers;
mod manager;
mod readwrite;

pub mod backend;
#[cfg(feature = "lmdb")]
pub mod migrator;
pub mod store;
pub mod value;

pub use backend::{DatabaseFlags, EnvironmentFlags, WriteFlags};
pub use env::Rkv;
pub use error::{DataError, MigrateError, StoreError};
pub use manager::Manager;
#[cfg(feature = "lmdb")]
pub use migrator::Migrator;
pub use readwrite::{Readable, Reader, Writer};
pub use store::{keys::EncodableKey, single::SingleStore, CloseOptions, Options as StoreOptions};
pub use value::{OwnedValue, Value};

#[cfg(feature = "db-dup-sort")]
pub use store::multi::MultiStore;

#[cfg(feature = "db-int-key")]
pub use store::integer::IntegerStore;
#[cfg(feature = "db-int-key")]
pub use store::keys::PrimitiveInt;

#[cfg(all(feature = "db-dup-sort", feature = "db-int-key"))]
pub use store::integermulti::MultiIntegerStore;

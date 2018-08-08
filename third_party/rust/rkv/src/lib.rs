// Copyright 2018 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

//! a simple, humane, typed Rust interface to [LMDB](http://www.lmdb.tech/doc/)
//!
//! It aims to achieve the following:
//!
//! - Avoid LMDB's sharp edges (e.g., obscure error codes for common situations).
//! - Report errors via [failure](https://docs.rs/failure/).
//! - Correctly restrict access to one handle per process via a [Manager](struct.Manager.html).
//! - Use Rust's type system to make single-typed key stores (including LMDB's own integer-keyed stores) safe and ergonomic.
//! - Encode and decode values via [bincode](https://docs.rs/bincode/)/[serde](https://docs.rs/serde/)
//!   and type tags, achieving platform-independent storage and input/output flexibility.
//!
//! It exposes these primary abstractions:
//!
//! - [Manager](struct.Manager.html): a singleton that controls access to LMDB environments
//! - [Rkv](struct.Rkv.html): an LMDB environment, which contains a set of key/value databases
//! - [Store](struct.Store.html): an LMDB database, which contains a set of key/value pairs
//!
//! Keys can be anything that implements `AsRef<[u8]>` or integers (when accessing an [IntegerStore](struct.IntegerStore.html)).
//! Values can be any of the types defined by the [Value](value/enum.Value.html) enum, including:
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
//! extern crate rkv;
//! extern crate tempfile;
//!
//! use rkv::{Manager, Rkv, Store, Value};
//! use std::fs;
//! use tempfile::Builder;
//!
//! // First determine the path to the environment, which is represented
//! // on disk as a directory containing two files:
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
//! // The Manager enforces that each process opens the same environment
//! // at most once by caching a handle to each environment that it opens.
//! // Retrieve the handle to an opened environment—or create one if it hasn't
//! // already been opened—by calling `Manager.get_or_create()`, passing it
//! // an `Rkv` method that opens an environment (`Rkv::new` in this case):
//! let created_arc = Manager::singleton().write().unwrap().get_or_create(path, Rkv::new).unwrap();
//! let env = created_arc.read().unwrap();
//!
//! // Call `Rkv.open_or_create_default()` to get a handle to the default
//! // (unnamed) store for the environment.
//! let store: Store = env.open_or_create_default().unwrap();
//!
//! {
//!     // Use a write transaction to mutate the store by calling
//!     // `Rkv.write()` to create a `Writer`.  There can be only one
//!     // writer for a given store; opening a second one will block
//!     // until the first completes.
//!     let mut writer = env.write().unwrap();
//!
//!     // Writer takes a `Store` reference as the first argument.
//!     // Keys are `AsRef<[u8]>`, while values are `Value` enum instances.
//!     // Use the `Blob` variant to store arbitrary collections of bytes.
//!     writer.put(&store, "int", &Value::I64(1234)).unwrap();
//!     writer.put(&store, "uint", &Value::U64(1234_u64)).unwrap();
//!     writer.put(&store, "float", &Value::F64(1234.0.into())).unwrap();
//!     writer.put(&store, "instant", &Value::Instant(1528318073700)).unwrap();
//!     writer.put(&store, "boolean", &Value::Bool(true)).unwrap();
//!     writer.put(&store, "string", &Value::Str("héllo, yöu")).unwrap();
//!     writer.put(&store, "json", &Value::Json(r#"{"foo":"bar", "number": 1}"#)).unwrap();
//!     writer.put(&store, "blob", &Value::Blob(b"blob")).unwrap();
//!
//!     // You must commit a write transaction before the writer goes out
//!     // of scope, or the transaction will abort and the data won't persist.
//!     writer.commit().unwrap();
//! }
//!
//! {
//!     // Use a read transaction to query the store by calling `Rkv.read()`
//!     // to create a `Reader`.  There can be unlimited concurrent readers
//!     // for a store, and readers never block on a writer nor other readers.
//!     let reader = env.read().expect("reader");
//!
//!     // To retrieve data, call `Reader.get()`, passing it the target store
//!     // and the key for the value to retrieve.
//!     println!("Get int {:?}", reader.get(&store, "int").unwrap());
//!     println!("Get uint {:?}", reader.get(&store, "uint").unwrap());
//!     println!("Get float {:?}", reader.get(&store, "float").unwrap());
//!     println!("Get instant {:?}", reader.get(&store, "instant").unwrap());
//!     println!("Get boolean {:?}", reader.get(&store, "boolean").unwrap());
//!     println!("Get string {:?}", reader.get(&store, "string").unwrap());
//!     println!("Get json {:?}", reader.get(&store, "json").unwrap());
//!     println!("Get blob {:?}", reader.get(&store, "blob").unwrap());
//!
//!     // Retrieving a non-existent value returns `Ok(None)`.
//!     println!("Get non-existent value {:?}", reader.get(&store, "non-existent"));
//!
//!     // A read transaction will automatically close once the reader
//!     // goes out of scope, so isn't necessary to close it explicitly,
//!     // although you can do so by calling `Reader.abort()`.
//! }
//!
//! {
//!     // Aborting a write transaction rolls back the change(s).
//!     let mut writer = env.write().unwrap();
//!     writer.put(&store, "foo", &Value::Str("bar")).unwrap();
//!     writer.abort();
//!
//!     let reader = env.read().expect("reader");
//!     println!("It should be None! ({:?})", reader.get(&store, "foo").unwrap());
//! }
//!
//! {
//!     // Explicitly aborting a transaction is not required unless an early
//!     // abort is desired, since both read and write transactions will
//!     // implicitly be aborted once they go out of scope.
//!     {
//!         let mut writer = env.write().unwrap();
//!         writer.put(&store, "foo", &Value::Str("bar")).unwrap();
//!     }
//!     let reader = env.read().expect("reader");
//!     println!("It should be None! ({:?})", reader.get(&store, "foo").unwrap());
//! }
//!
//! {
//!     // Deleting a key/value pair also requires a write transaction.
//!     let mut writer = env.write().unwrap();
//!     writer.put(&store, "foo", &Value::Str("bar")).unwrap();
//!     writer.put(&store, "bar", &Value::Str("baz")).unwrap();
//!     writer.delete(&store, "foo").unwrap();
//!
//!     // A write transaction also supports reading, the version of the
//!     // store that it reads includes changes it has made regardless of
//!     // the commit state of that transaction.
//!     // In the code above, "foo" and "bar" were put into the store,
//!     // then "foo" was deleted so only "bar" will return a result.
//!     println!("It should be None! ({:?})", writer.get(&store, "foo").unwrap());
//!     println!("Get bar ({:?})", writer.get(&store, "bar").unwrap());
//!     writer.commit().unwrap();
//!     let reader = env.read().expect("reader");
//!     println!("It should be None! ({:?})", reader.get(&store, "foo").unwrap());
//!     println!("Get bar {:?}", reader.get(&store, "bar").unwrap());
//!
//!     // Committing a transaction consumes the writer, preventing you
//!     // from reusing it by failing at compile time with an error.
//!     // This line would report error[E0382]: use of moved value: `writer`.
//!     // writer.put(&store, "baz", &Value::Str("buz")).unwrap();
//! }
//! ```

#![allow(dead_code)]

#[macro_use]
extern crate arrayref;
#[macro_use]
extern crate failure;
#[macro_use]
extern crate lazy_static;

extern crate bincode;
extern crate lmdb;
extern crate ordered_float;
extern crate serde; // So we can specify trait bounds. Everything else is bincode.
extern crate url;
extern crate uuid;

pub use lmdb::{
    DatabaseFlags,
    EnvironmentBuilder,
    EnvironmentFlags,
    WriteFlags,
};

mod env;
pub mod error;
mod integer;
mod manager;
mod readwrite;
pub mod value;

pub use env::Rkv;

pub use error::{
    DataError,
    StoreError,
};

pub use integer::{
    IntegerReader,
    IntegerStore,
    IntegerWriter,
    PrimitiveInt,
};

pub use manager::Manager;

pub use readwrite::{
    Reader,
    Store,
    Writer,
};

pub use value::Value;

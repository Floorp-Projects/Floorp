// Copyright 2018 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::marker::PhantomData;

use bincode::serialize;

use serde::Serialize;

use lmdb::Database;

use error::{
    DataError,
    StoreError,
};

use value::Value;

use readwrite::{
    Reader,
    Store,
    Writer,
};

pub trait EncodableKey {
    fn to_bytes(&self) -> Result<Vec<u8>, DataError>;
}

pub trait PrimitiveInt: EncodableKey {}

impl PrimitiveInt for u32 {}

impl<T> EncodableKey for T
where
    T: Serialize,
{
    fn to_bytes(&self) -> Result<Vec<u8>, DataError> {
        serialize(self)         // TODO: limited key length.
            .map_err(|e| e.into())
    }
}

pub(crate) struct Key<K> {
    bytes: Vec<u8>,
    phantom: PhantomData<K>,
}

impl<K> AsRef<[u8]> for Key<K>
where
    K: EncodableKey,
{
    fn as_ref(&self) -> &[u8] {
        self.bytes.as_ref()
    }
}

impl<K> Key<K>
where
    K: EncodableKey,
{
    fn new(k: K) -> Result<Key<K>, DataError> {
        Ok(Key {
            bytes: k.to_bytes()?,
            phantom: PhantomData,
        })
    }
}

pub struct IntegerReader<'env, K>
where
    K: PrimitiveInt,
{
    inner: Reader<'env, Key<K>>,
}

impl<'env, K> IntegerReader<'env, K>
where
    K: PrimitiveInt,
{
    pub(crate) fn new(reader: Reader<Key<K>>) -> IntegerReader<K> {
        IntegerReader {
            inner: reader,
        }
    }

    pub fn get<'s>(&'s self, store: &'s IntegerStore, k: K) -> Result<Option<Value<'s>>, StoreError> {
        self.inner.get(&store.inner, Key::new(k)?)
    }

    pub fn abort(self) {
        self.inner.abort();
    }
}

pub struct IntegerWriter<'env, K>
where
    K: PrimitiveInt,
{
    inner: Writer<'env, Key<K>>,
}

impl<'env, K> IntegerWriter<'env, K>
where
    K: PrimitiveInt,
{
    pub(crate) fn new(writer: Writer<Key<K>>) -> IntegerWriter<K> {
        IntegerWriter {
            inner: writer,
        }
    }

    pub fn get<'s>(&'s self, store: &'s IntegerStore, k: K) -> Result<Option<Value<'s>>, StoreError> {
        self.inner.get(&store.inner, Key::new(k)?)
    }

    pub fn put<'s>(&'s mut self, store: &'s IntegerStore, k: K, v: &Value) -> Result<(), StoreError> {
        self.inner.put(&store.inner, Key::new(k)?, v)
    }

    fn abort(self) {
        self.inner.abort();
    }

    fn commit(self) -> Result<(), StoreError> {
        self.inner.commit()
    }
}

pub struct IntegerStore {
    inner: Store,
}

impl IntegerStore {
    pub fn new(db: Database) -> IntegerStore {
        IntegerStore {
            inner: Store::new(db),
        }
    }
}

#[cfg(test)]
mod tests {
    extern crate tempfile;

    use self::tempfile::Builder;
    use std::fs;

    use super::*;
    use *;

    #[test]
    fn test_integer_keys() {
        let root = Builder::new().prefix("test_integer_keys").tempdir().expect("tempdir");
        fs::create_dir_all(root.path()).expect("dir created");
        let k = Rkv::new(root.path()).expect("new succeeded");
        let s = k.open_or_create_integer("s").expect("open");

        let mut writer = k.write_int::<u32>().expect("writer");

        writer.put(&s, 123, &Value::Str("hello!")).expect("write");
        assert_eq!(writer.get(&s, 123).expect("read"), Some(Value::Str("hello!")));
        writer.commit().expect("committed");

        let reader = k.read_int::<u32>().expect("reader");
        assert_eq!(reader.get(&s, 123).expect("read"), Some(Value::Str("hello!")));
    }
}

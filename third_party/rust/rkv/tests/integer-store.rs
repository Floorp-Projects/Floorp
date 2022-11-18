// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
#![cfg(feature = "db-int-key")]
#![allow(clippy::many_single_char_names)]

use std::fs;

use serde_derive::Serialize;
use tempfile::Builder;

use rkv::{backend::SafeMode, PrimitiveInt, Rkv, StoreOptions, Value};

#[test]
fn test_integer_keys() {
    let root = Builder::new()
        .prefix("test_integer_keys")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    let s = k.open_integer("s", StoreOptions::create()).expect("open");

    macro_rules! test_integer_keys {
        ($store:expr, $key:expr) => {{
            let mut writer = k.write().expect("writer");

            $store
                .put(&mut writer, $key, &Value::Str("hello!"))
                .expect("write");
            assert_eq!(
                $store.get(&writer, $key).expect("read"),
                Some(Value::Str("hello!"))
            );
            writer.commit().expect("committed");

            let reader = k.read().expect("reader");
            assert_eq!(
                $store.get(&reader, $key).expect("read"),
                Some(Value::Str("hello!"))
            );
        }};
    }

    // The integer module provides only the u32 integer key variant
    // of IntegerStore, so we can use it without further ado.
    test_integer_keys!(s, std::u32::MIN);
    test_integer_keys!(s, std::u32::MAX);

    // If you want to use another integer key variant, you need to implement
    // a newtype, implement PrimitiveInt, and implement or derive Serialize
    // for it.  Here we do so for the i32 type.

    // DANGER!  Doing this enables you to open a store with multiple,
    // different integer key types, which may result in unexpected behavior.
    // Make sure you know what you're doing!

    let t = k.open_integer("s", StoreOptions::create()).expect("open");

    #[derive(Serialize)]
    struct I32(i32);
    impl PrimitiveInt for I32 {}
    test_integer_keys!(t, I32(std::i32::MIN));
    test_integer_keys!(t, I32(std::i32::MAX));

    let u = k.open_integer("s", StoreOptions::create()).expect("open");

    #[derive(Serialize)]
    struct U16(u16);
    impl PrimitiveInt for U16 {}
    test_integer_keys!(u, U16(std::u16::MIN));
    test_integer_keys!(u, U16(std::u16::MAX));

    let v = k.open_integer("s", StoreOptions::create()).expect("open");

    #[derive(Serialize)]
    struct U64(u64);
    impl PrimitiveInt for U64 {}
    test_integer_keys!(v, U64(std::u64::MIN));
    test_integer_keys!(v, U64(std::u64::MAX));
}

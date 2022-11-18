// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

//! A command-line utility to create an LMDB environment containing random data.
//! It requires one flag, `-s path/to/environment`, which specifies the location
//! where the tool should create the environment.  Optionally, you may specify
//! the number of key/value pairs to create via the `-n <number>` flag
//! (for which the default value is 50).

use std::{env::args, fs, fs::File, io::Read, path::Path};

use rkv::{
    backend::{BackendEnvironmentBuilder, Lmdb},
    Rkv, StoreOptions, Value,
};

fn main() {
    let mut args = args();
    let mut database = None;
    let mut path = None;
    let mut num_pairs = 50;

    // The first arg is the name of the program, which we can ignore.
    args.next();

    while let Some(arg) = args.next() {
        if &arg[0..1] == "-" {
            match &arg[1..] {
                "s" => {
                    database = match args.next() {
                        None => panic!("-s must be followed by database arg"),
                        Some(str) => Some(str),
                    };
                }
                "n" => {
                    num_pairs = match args.next() {
                        None => panic!("-s must be followed by number of pairs"),
                        Some(str) => str.parse().expect("number"),
                    };
                }
                str => panic!("arg -{} not recognized", str),
            }
        } else {
            if path.is_some() {
                panic!("must provide only one path to the LMDB environment");
            }
            path = Some(arg);
        }
    }

    if path.is_none() {
        panic!("must provide a path to the LMDB environment");
    }

    let path = path.unwrap();
    fs::create_dir_all(&path).expect("dir created");

    let mut builder = Rkv::environment_builder::<Lmdb>();
    builder.set_max_dbs(2);
    // Allocate enough map to accommodate the largest random collection.
    // We currently do this by allocating twice the maximum possible size
    // of the pairs (assuming maximum key and value sizes).
    builder.set_map_size((511 + 65535) * num_pairs * 2);
    let rkv = Rkv::from_builder(Path::new(&path), builder).expect("Rkv");
    let store = rkv
        .open_single(database.as_deref(), StoreOptions::create())
        .expect("opened");
    let mut writer = rkv.write().expect("writer");

    // Generate random values for the number of keys and key/value lengths.
    // On Linux, "Just use /dev/urandom!" <https://www.2uo.de/myths-about-urandom/>.
    // On macOS it doesn't matter (/dev/random and /dev/urandom are identical).
    let mut random = File::open("/dev/urandom").unwrap();
    let mut nums = [0u8; 4];
    random.read_exact(&mut nums).unwrap();

    // Generate 0–255 pairs.
    for _ in 0..num_pairs {
        // Generate key and value lengths.  The key must be 1–511 bytes long.
        // The value length can be 0 and is essentially unbounded; we generate
        // value lengths of 0–0xffff (65535).
        // NB: the modulus method for generating a random number within a range
        // introduces distribution skew, but we don't need it to be perfect.
        let key_len = ((u16::from(nums[0]) + (u16::from(nums[1]) << 8)) % 511 + 1) as usize;
        let value_len = (u16::from(nums[2]) + (u16::from(nums[3]) << 8)) as usize;

        let mut key: Vec<u8> = vec![0; key_len];
        random.read_exact(&mut key[0..key_len]).unwrap();

        let mut value: Vec<u8> = vec![0; value_len];
        random.read_exact(&mut value[0..value_len]).unwrap();

        store
            .put(&mut writer, key, &Value::Blob(&value))
            .expect("wrote");
    }

    writer.commit().expect("committed");
}

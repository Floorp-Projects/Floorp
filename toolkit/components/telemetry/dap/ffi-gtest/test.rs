/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::Deserialize;
use std::fs::File;
use std::io::Cursor;

use dap_ffi::prg::PrgAes128Alt;
use dap_ffi::types::Report;

use prio::codec::{Decode, Encode};
use prio::vdaf::prg::{Prg, SeedStream};

#[no_mangle]
pub extern "C" fn dap_test_encoding() -> bool {
    let r = Report::new_dummy();
    let mut encoded = Vec::<u8>::new();
    Report::encode(&r, &mut encoded);
    let decoded = Report::decode(&mut Cursor::new(&encoded)).expect("Report decoding failed!");
    if r != decoded {
        println!("Report:");
        println!("{:?}", r);
        println!("Encoded Report:");
        println!("{:?}", encoded);
        println!("Decoded Report:");
        println!("{:?}", decoded);

        return false;
    }

    true
}

#[derive(Deserialize, Debug)]
struct PrgTestCase {
    seed: [u8; 16],
    info_string: Vec<u8>,
    buffer1_out: Vec<u8>,
    buffer2_out: Vec<u8>,
}

#[no_mangle]
pub extern "C" fn dap_test_prg() {
    let file = File::open("PrgAes128_tests.json").unwrap();
    let testcases: Vec<PrgTestCase> = serde_json::from_reader(file).unwrap();
    for testcase in testcases {
        let mut p = PrgAes128Alt::init(&testcase.seed);
        p.update(&testcase.info_string);
        let mut s = p.into_seed_stream();
        let mut b1 = vec![0u8; testcase.buffer1_out.len()];
        s.fill(&mut b1);
        assert_eq!(b1, testcase.buffer1_out);
        let mut b2 = vec![0u8; testcase.buffer2_out.len()];
        s.fill(&mut b2);
        assert_eq!(b2, testcase.buffer2_out);
    }
}

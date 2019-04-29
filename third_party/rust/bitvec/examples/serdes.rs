/*! This example shows off de/serializing a bit sequence using serde.

The example uses JSON for simplicity of demonstration; it should work with all
serde-compatible de/ser protocols.
!*/

extern crate bitvec;
extern crate serde;
extern crate serde_json;

use bitvec::prelude::*;

#[cfg(all(feature = "alloc", feature = "serdes"))]
fn main() {
	let bv = bitvec![1, 0, 1, 1, 0, 0, 1, 0];
	let json = serde_json::to_string(&bv).expect("cannot fail to serialize");
	assert_eq!(json.trim(),r#"{"elts":1,"head":0,"tail":8,"data":[178]}"#);

	let bb: BitBox = serde_json::from_str(&json).expect("cannot fail to deserialize");
	assert!(bb[0]);
	assert_eq!(bb.as_slice()[0], 178);
}

#[cfg(not(all(feature = "alloc", feature = "serdes")))]
fn main() {
	println!("This test needs to be compiled with `--features serdes` to work");
}

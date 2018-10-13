extern crate argon2rs;
use argon2rs::verifier::Encoded;
use argon2rs::defaults::{KIB, LANES, PASSES};
use argon2rs::{Argon2, Variant};

pub fn main() {
    // A typical password hashing scenario:
    //
    // 1. Hash a password into a secure, storable encoding:
    let a2 = Argon2::new(PASSES, LANES, KIB, Variant::Argon2i).unwrap();
    let enc0 = Encoded::new(a2,
                            b"password goes here",
                            b"sodium chloride",
                            b"",
                            b"");
    let bytes0 = enc0.to_u8();
    println!("storable encoding 0: {}",
             String::from_utf8(bytes0.clone()).unwrap());

    // or, if you're in a hurry and/or would rather rely on algorithm defaults:
    let bytes1 = Encoded::default2i(b"another password",
                                    b"salt required",
                                    b"key",
                                    b"")
                     .to_u8();
    println!("storable encoding 1: {}",
             String::from_utf8(bytes1.clone()).unwrap());

    // 2. Verify later-received input against a previously created encoding.
    let enc0 = Encoded::from_u8(&bytes0[..]).unwrap();
    assert!(enc0.verify(b"password goes here"));

    let enc1 = Encoded::from_u8(&bytes1[..]).unwrap();
    assert!(enc1.verify(b"another password"));
}

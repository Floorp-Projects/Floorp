# rust-sha1

Minimal implementation of SHA1 for Rust.  This might go away in the future
if rust-crypto or some libraries like that split into smaller parts.

Right now SHA1 is quite frequently used and many things want to have an
implementation of it, that does not pull in too much other stuff.

This is largely based on the hash code in crypto-rs by Koka El Kiwi.

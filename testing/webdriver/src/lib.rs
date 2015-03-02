#![feature(old_io)]
#![feature(core)]
#![feature(collections)]
#![allow(non_snake_case)]

#[macro_use]
extern crate log;
extern crate "rustc-serialize" as rustc_serialize;
extern crate hyper;
extern crate regex;


#[macro_use] pub mod macros;
mod httpapi;
pub mod command;
pub mod common;
pub mod error;
pub mod server;
pub mod response;


#[test]
fn it_works() {
}

//Until it's clear what the unstable things are replaced by
#![allow(unstable)]
#![allow(non_snake_case)]

#[macro_use] extern crate log;
extern crate "rustc-serialize" as rustc_serialize;
extern crate core;
extern crate hyper;
extern crate regex;

macro_rules! try_opt {
    ($expr:expr, $err_type:expr, $err_msg:expr) => ({
        match $expr {
            Some(x) => x,
            None => return Err(WebDriverError::new($err_type, $err_msg))
        }
    })
}

pub mod command;
pub mod common;
pub mod httpserver;
pub mod response;
mod messagebuilder;


#[test]
fn it_works() {
}

#![allow(non_snake_case)]

#[macro_use]
extern crate log;
extern crate rustc_serialize;
extern crate hyper;
extern crate regex;

#[macro_use] pub mod macros;
pub mod httpapi;
pub mod command;
pub mod common;
pub mod error;
pub mod server;
pub mod response;


#[cfg(test)]
mod nullable_tests {
    use super::common::Nullable;

    #[test]
    fn test_nullable_map() {
        let mut test = Nullable::Value(21);

        assert_eq!(test.map(|x| x << 1), Nullable::Value(42));

        test = Nullable::Null;

        assert_eq!(test.map(|x| x << 1), Nullable::Null);
    }

    #[test]
    fn test_nullable_into() {
        let test: Option<i32> = Nullable::Value(42).into();

        assert_eq!(test, Some(42));
    }
}

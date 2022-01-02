#![deny(warnings)]

extern crate toml;
extern crate serde_json;

use std::fs::File;
use std::env;
use std::io;
use std::io::prelude::*;

use toml::Value as Toml;
use serde_json::Value as Json;

fn main() {
    let mut args = env::args();
    let mut input = String::new();
    if args.len() > 1 {
        let name = args.nth(1).unwrap();
        File::open(&name).and_then(|mut f| {
            f.read_to_string(&mut input)
        }).unwrap();
    } else {
        io::stdin().read_to_string(&mut input).unwrap();
    }

    match input.parse() {
        Ok(toml) => {
            let json = convert(toml);
            println!("{}", serde_json::to_string_pretty(&json).unwrap());
        }
        Err(error) => println!("failed to parse TOML: {}", error),
    }
}

fn convert(toml: Toml) -> Json {
    match toml {
        Toml::String(s) => Json::String(s),
        Toml::Integer(i) => Json::Number(i.into()),
        Toml::Float(f) => {
            let n = serde_json::Number::from_f64(f)
                        .expect("float infinite and nan not allowed");
            Json::Number(n)
        }
        Toml::Boolean(b) => Json::Bool(b),
        Toml::Array(arr) => Json::Array(arr.into_iter().map(convert).collect()),
        Toml::Table(table) => Json::Object(table.into_iter().map(|(k, v)| {
            (k, convert(v))
        }).collect()),
        Toml::Datetime(dt) => Json::String(dt.to_string()),
    }
}

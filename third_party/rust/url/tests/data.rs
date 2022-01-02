// Copyright 2013-2014 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Data-driven tests

extern crate rustc_test as test;
extern crate serde_json;
extern crate url;

use serde_json::Value;
use std::str::FromStr;
use url::{quirks, Url};

fn check_invariants(url: &Url) {
    url.check_invariants().unwrap();
    #[cfg(feature = "serde")]
    {
        let bytes = serde_json::to_vec(url).unwrap();
        let new_url: Url = serde_json::from_slice(&bytes).unwrap();
        assert_eq!(url, &new_url);
    }
}

fn run_parsing(input: &str, base: &str, expected: Result<ExpectedAttributes, ()>) {
    let base = match Url::parse(&base) {
        Ok(base) => base,
        Err(_) if expected.is_err() => return,
        Err(message) => panic!("Error parsing base {:?}: {}", base, message),
    };
    let (url, expected) = match (base.join(&input), expected) {
        (Ok(url), Ok(expected)) => (url, expected),
        (Err(_), Err(())) => return,
        (Err(message), Ok(_)) => panic!("Error parsing URL {:?}: {}", input, message),
        (Ok(_), Err(())) => panic!("Expected a parse error for URL {:?}", input),
    };

    check_invariants(&url);

    macro_rules! assert_eq {
        ($expected: expr, $got: expr) => {{
            let expected = $expected;
            let got = $got;
            assert!(
                expected == got,
                "\n{:?}\n!= {}\n{:?}\nfor URL {:?}\n",
                got,
                stringify!($expected),
                expected,
                url
            );
        }};
    }

    macro_rules! assert_attributes {
        ($($attr: ident)+) => {
            {
                $(
                    assert_eq!(expected.$attr, quirks::$attr(&url));
                )+;
            }
        }
    }

    assert_attributes!(href protocol username password host hostname port pathname search hash);

    if let Some(expected_origin) = expected.origin {
        assert_eq!(expected_origin, quirks::origin(&url));
    }
}

struct ExpectedAttributes {
    href: String,
    origin: Option<String>,
    protocol: String,
    username: String,
    password: String,
    host: String,
    hostname: String,
    port: String,
    pathname: String,
    search: String,
    hash: String,
}

trait JsonExt {
    fn take_key(&mut self, key: &str) -> Option<Value>;
    fn string(self) -> String;
    fn take_string(&mut self, key: &str) -> String;
}

impl JsonExt for Value {
    fn take_key(&mut self, key: &str) -> Option<Value> {
        self.as_object_mut().unwrap().remove(key)
    }

    fn string(self) -> String {
        if let Value::String(s) = self {
            s
        } else {
            panic!("Not a Value::String")
        }
    }

    fn take_string(&mut self, key: &str) -> String {
        self.take_key(key).unwrap().string()
    }
}

fn collect_parsing<F: FnMut(String, test::TestFn)>(add_test: &mut F) {
    // Copied form https://github.com/w3c/web-platform-tests/blob/master/url/
    let mut json = Value::from_str(include_str!("urltestdata.json"))
        .expect("JSON parse error in urltestdata.json");
    for entry in json.as_array_mut().unwrap() {
        if entry.is_string() {
            continue; // ignore comments
        }
        let base = entry.take_string("base");
        let input = entry.take_string("input");
        let expected = if entry.take_key("failure").is_some() {
            Err(())
        } else {
            Ok(ExpectedAttributes {
                href: entry.take_string("href"),
                origin: entry.take_key("origin").map(|s| s.string()),
                protocol: entry.take_string("protocol"),
                username: entry.take_string("username"),
                password: entry.take_string("password"),
                host: entry.take_string("host"),
                hostname: entry.take_string("hostname"),
                port: entry.take_string("port"),
                pathname: entry.take_string("pathname"),
                search: entry.take_string("search"),
                hash: entry.take_string("hash"),
            })
        };
        add_test(
            format!("{:?} @ base {:?}", input, base),
            test::TestFn::dyn_test_fn(move || run_parsing(&input, &base, expected)),
        );
    }
}

fn collect_setters<F>(add_test: &mut F)
where
    F: FnMut(String, test::TestFn),
{
    let mut json = Value::from_str(include_str!("setters_tests.json"))
        .expect("JSON parse error in setters_tests.json");

    macro_rules! setter {
        ($attr: expr, $setter: ident) => {{
            let mut tests = json.take_key($attr).unwrap();
            for mut test in tests.as_array_mut().unwrap().drain(..) {
                let comment = test.take_key("comment")
                    .map(|s| s.string())
                    .unwrap_or(String::new());
                let href = test.take_string("href");
                let new_value = test.take_string("new_value");
                let name = format!("{:?}.{} = {:?} {}", href, $attr, new_value, comment);
                let mut expected = test.take_key("expected").unwrap();
                add_test(name, test::TestFn::dyn_test_fn(move || {
                    let mut url = Url::parse(&href).unwrap();
                    check_invariants(&url);
                    let _ = quirks::$setter(&mut url, &new_value);
                    assert_attributes!(url, expected,
                        href protocol username password host hostname port pathname search hash);
                    check_invariants(&url);
                }))
            }
        }}
    }
    macro_rules! assert_attributes {
        ($url: expr, $expected: expr, $($attr: ident)+) => {
            $(
                if let Some(value) = $expected.take_key(stringify!($attr)) {
                    assert_eq!(quirks::$attr(&$url), value.string())
                }
            )+
        }
    }
    setter!("protocol", set_protocol);
    setter!("username", set_username);
    setter!("password", set_password);
    setter!("hostname", set_hostname);
    setter!("host", set_host);
    setter!("port", set_port);
    setter!("pathname", set_pathname);
    setter!("search", set_search);
    setter!("hash", set_hash);
}

fn main() {
    let mut tests = Vec::new();
    {
        let mut add_one = |name: String, run: test::TestFn| {
            tests.push(test::TestDescAndFn {
                desc: test::TestDesc::new(test::DynTestName(name)),
                testfn: run,
            })
        };
        collect_parsing(&mut add_one);
        collect_setters(&mut add_one);
    }
    test::test_main(&std::env::args().collect::<Vec<_>>(), tests)
}

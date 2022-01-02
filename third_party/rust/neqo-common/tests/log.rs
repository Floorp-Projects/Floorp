// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::use_self)]

use neqo_common::{qdebug, qerror, qinfo, qtrace, qwarn};

#[test]
fn basic() {
    qerror!("error");
    qwarn!("warn");
    qinfo!("info");
    qdebug!("debug");
    qtrace!("trace");
}

#[test]
fn args() {
    let num = 1;
    let obj = std::time::Instant::now();
    qerror!("error {} {:?}", num, obj);
    qwarn!("warn {} {:?}", num, obj);
    qinfo!("info {} {:?}", num, obj);
    qdebug!("debug {} {:?}", num, obj);
    qtrace!("trace {} {:?}", num, obj);
}

#[test]
fn context() {
    let context = "context";
    qerror!([context], "error");
    qwarn!([context], "warn");
    qinfo!([context], "info");
    qdebug!([context], "debug");
    qtrace!([context], "trace");
}

#[test]
fn context_comma() {
    let obj = vec![1, 2, 3];
    let context = "context";
    qerror!([context], "error {:?}", obj);
    qwarn!([context], "warn {:?}", obj);
    qinfo!([context], "info {:?}", obj);
    qdebug!([context], "debug {:?}", obj);
    qtrace!([context], "trace {:?}", obj);
}

#[test]
fn context_expr() {
    let context = vec![1, 2, 3];
    qerror!([format!("{:x?}", context)], "error");
    qwarn!([format!("{:x?}", context)], "warn");
    qinfo!([format!("{:x?}", context)], "info");
    qdebug!([format!("{:x?}", context)], "debug");
    qtrace!([format!("{:x?}", context)], "trace");
}

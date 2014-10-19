extern crate hyper;
extern crate serialize;
extern crate uuid;
extern crate regex;

use std::io::net::ip::Ipv4Addr;
use httpserver::start;

mod common;
mod command;
mod httpserver;
mod marionette;
mod messagebuilder;
mod response;


fn main() {
    start(Ipv4Addr(127, 0, 0, 1), 1337);
}

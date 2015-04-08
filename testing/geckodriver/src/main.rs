#![feature(std_misc)]
#![feature(io)]
#![feature(net)]
#![feature(path)]
#![feature(exit_status)]
#![feature(collections)]
#[macro_use]
extern crate log;
extern crate "rustc-serialize" as rustc_serialize;
extern crate argparse;
extern crate env_logger;
extern crate hyper;
extern crate mozprofile;
extern crate mozrunner;
extern crate regex;
#[macro_use]
extern crate webdriver;

use std::env;
use std::net::SocketAddr;
use std::str::FromStr;
use std::path::PathBuf;

use argparse::{ArgumentParser, StoreTrue, Store};
use webdriver::server::start;

use marionette::{MarionetteHandler, BrowserLauncher, MarionetteSettings};

macro_rules! try_opt {
    ($expr:expr, $err_type:expr, $err_msg:expr) => ({
        match $expr {
            Some(x) => x,
            None => return Err(WebDriverError::new($err_type, $err_msg))
        }
    })
}

mod marionette;

static DEFAULT_ADDR: &'static str = "127.0.0.1:4444";

struct Options {
    binary: String,
    webdriver_port: u16,
    marionette_port: u16,
    connect_existing: bool
}

fn parse_args() -> Result<Options, ()> {
    let mut opts = Options {
        binary: "".to_string(),
        webdriver_port: 4444u16,
        marionette_port: 2828u16,
        connect_existing: false
    };

    //Limit the scope of the opts borrow
    {
        let mut parser = ArgumentParser::new();
        parser.set_description("WebDriver to marionette proxy.");
        parser.refer(&mut opts.binary)
            .add_option(&["-b", "--binary"], Store,
                        "Path to the Firefox binary");
        parser.refer(&mut opts.webdriver_port)
            .add_option(&["--webdriver-port"], Store,
                        "Port to run webdriver on");
        parser.refer(&mut opts.marionette_port)
            .add_option(&["--marionette-port"], Store,
                        "Port to run marionette on");
        parser.refer(&mut opts.connect_existing)
            .add_option(&["--connect-existing"], StoreTrue,
                        "Connect to an existing firefox process");
        if let Err(e) = parser.parse_args() {
            env::set_exit_status(e);
            return Err(())
        };
    }

    Ok(opts)
}


// Valid addresses to parse are "HOST:PORT" or ":PORT".
// If the host isn't specified, 127.0.0.1 will be assumed.
fn parse_addr(s: &str) -> Result<SocketAddr, String> {
    let mut parts: Vec<&str> = s.splitn(1, ':').collect();
    if parts.len() == 2 {
        parts[0] = "127.0.0.1";
    }
    let full_addr = parts.connect(":");
    match FromStr::from_str(&full_addr[..]) {
        Ok(addr) => Ok(addr),
        Err(_) => Err(format!("illegal address: {}", s))
    }
}

fn main() {
    env_logger::init().unwrap();
    let opts = match parse_args() {
        Ok(args) => args,
        Err(_) => return
    };
    let addr = match parse_addr(DEFAULT_ADDR) {
        Ok(x) => x,
        Err(e) => {
            println!("{}", e);
            return
        }
    };

    let launcher = if opts.connect_existing {
        BrowserLauncher::None
    } else {
        BrowserLauncher::BinaryLauncher(PathBuf::new(&opts.binary))
    };

    let settings = MarionetteSettings::new(opts.marionette_port, launcher);

    //TODO: what if binary isn't a valid path?
    start(addr.ip(), opts.webdriver_port, MarionetteHandler::new(settings));
}

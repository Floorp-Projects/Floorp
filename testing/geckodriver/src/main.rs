#[macro_use]
extern crate log;
extern crate rustc_serialize;
extern crate argparse;
extern crate env_logger;
extern crate hyper;
extern crate mozprofile;
extern crate mozrunner;
extern crate regex;
#[macro_use]
extern crate webdriver;

use std::process::exit;
use std::net::SocketAddr;
use std::str::FromStr;
use std::path::Path;

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

fn parse_args() -> Options {
    let mut opts = Options {
        binary: "".to_string(),
        webdriver_port: 4444u16,
        marionette_port: 2828u16,
        connect_existing: false
    };

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
        parser.parse_args_or_exit()
    }
    opts
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
    let opts = parse_args();
    let addr = match parse_addr(DEFAULT_ADDR) {
        Ok(x) => x,
        Err(e) => {
            println!("{}", e);
            exit(1);
        }
    };

    let launcher = if opts.connect_existing {
        BrowserLauncher::None
    } else {
        BrowserLauncher::BinaryLauncher(Path::new(&opts.binary).to_path_buf())
    };

    let settings = MarionetteSettings::new(opts.marionette_port, launcher);

    //TODO: what if binary isn't a valid path?
    start(addr, MarionetteHandler::new(settings));
}

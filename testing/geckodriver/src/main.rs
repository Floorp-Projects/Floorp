#[macro_use]
extern crate log;
#[macro_use]
extern crate lazy_static;
extern crate argparse;
extern crate env_logger;
extern crate hyper;
extern crate mozprofile;
extern crate mozrunner;
extern crate regex;
extern crate rustc_serialize;
#[macro_use]
extern crate webdriver;
extern crate zip;

use std::borrow::ToOwned;
use std::env;
use std::io::Write;
use std::net::{SocketAddr, IpAddr};
use std::path::Path;
use std::process;
use std::str::FromStr;

use argparse::{ArgumentParser, IncrBy, StoreTrue, Store};
use webdriver::server::start;

use marionette::{MarionetteHandler, BrowserLauncher, LogLevel, MarionetteSettings, extension_routes};

macro_rules! try_opt {
    ($expr:expr, $err_type:expr, $err_msg:expr) => ({
        match $expr {
            Some(x) => x,
            None => return Err(WebDriverError::new($err_type, $err_msg))
        }
    })
}

macro_rules! println_stderr {
    ($($arg:tt)*) => { {
        let r = writeln!(&mut ::std::io::stderr(), $($arg)*);
        r.expect("failed printing to stderr");
    } }
}

macro_rules! err {
    ($($arg:tt)*) => { {
        let prog = env::args().next().unwrap();
        println_stderr!("{}: error: {}", prog, format_args!($($arg)*));
        process::exit(64);
    } }
}

mod marionette;

struct Options {
    binary: String,
    webdriver_host: String,
    webdriver_port: u16,
    marionette_port: u16,
    connect_existing: bool,
    e10s: bool,
    log_level: String,
    verbosity: u8,
}


fn parse_args() -> Options {
    let mut opts = Options {
        binary: "".to_owned(),
        webdriver_host: "127.0.0.1".to_owned(),
        webdriver_port: 4444u16,
        marionette_port: 2828u16,
        connect_existing: false,
        e10s: false,
        log_level: "".to_owned(),
        verbosity: 0,
    };

    {
        let mut parser = ArgumentParser::new();
        parser.set_description("WebDriver to Marionette proxy.");
        parser.refer(&mut opts.binary)
            .add_option(&["-b", "--binary"], Store,
                        "Path to the Firefox binary");
        parser.refer(&mut opts.webdriver_host)
            .add_option(&["--webdriver-host"], Store,
                        "Host to run webdriver server on");
        parser.refer(&mut opts.webdriver_port)
            .add_option(&["--webdriver-port"], Store,
                        "Port to run webdriver on");
        parser.refer(&mut opts.marionette_port)
            .add_option(&["--marionette-port"], Store,
                        "Port to run marionette on");
        parser.refer(&mut opts.connect_existing)
            .add_option(&["--connect-existing"], StoreTrue,
                        "Connect to an existing firefox process");
        parser.refer(&mut opts.e10s)
            .add_option(&["--e10s"], StoreTrue,
                        "Load Firefox with an e10s profile");
        parser.refer(&mut opts.log_level)
            .add_option(&["--log"], Store,
            "Desired verbosity level of Gecko \
            (fatal, error, warn, info, config, debug, trace)")
            .metavar("LEVEL");
        parser.refer(&mut opts.verbosity)
            .add_option(&["-v"], IncrBy(1),
            "Shorthand to increase verbosity of output \
            to include debug messages with -v, \
            and trace messages with -vv");
        parser.parse_args_or_exit();
    }

    if opts.binary == "" && !opts.connect_existing {
        err!("path to browser binary required unless --connect-existing");
    }

    opts
}

fn main() {
    env_logger::init().unwrap();
    let opts = parse_args();

    let host = &opts.webdriver_host[..];
    let port = opts.webdriver_port;
    let addr = IpAddr::from_str(host).map(
        |x| SocketAddr::new(x, port)).unwrap_or_else(
        |_| { err!("invalid host address"); });

    // overrides defaults in Gecko
    // which are info for optimised builds
    // and debug for debug builds
    let log_level = if opts.log_level.len() > 0 && opts.verbosity > 0 {
        err!("conflicting logging- and verbosity arguments");
    } else if opts.log_level.len() > 0 {
        match LogLevel::from_str(&opts.log_level) {
            Ok(level) => Some(level),
            Err(_) => { err!("unknown log level: {}", opts.log_level); },
        }
    } else {
        match opts.verbosity {
            0 => None,
            1 => Some(LogLevel::Debug),
            _ => Some(LogLevel::Trace),
        }
    };

    // TODO: what if binary isn't a valid path?
    let launcher = if opts.connect_existing {
        BrowserLauncher::None
    } else {
        BrowserLauncher::BinaryLauncher(Path::new(&opts.binary).to_path_buf())
    };

    let settings = MarionetteSettings {
        port: opts.marionette_port,
        launcher: launcher,
        e10s: opts.e10s,
        log_level: log_level,
    };
    start(addr, MarionetteHandler::new(settings), extension_routes());
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;
    use marionette::{MarionetteSettings, MarionetteHandler, BrowserLauncher};
    use webdriver::command::NewSessionParameters;
    use rustc_serialize::json::Json;
    use std::fs::File;
    use rustc_serialize::base64::{ToBase64, Config, CharacterSet, Newline};
    use mozprofile::preferences::Pref;
    use std::io::Read;

    #[test]
    fn test_profile() {
        let mut profile_data = Vec::with_capacity(1024);
        let mut profile = File::open("src/tests/profile.zip").unwrap();
        profile.read_to_end(&mut profile_data).unwrap();
        let base64_config = Config {
            char_set: CharacterSet::Standard,
            newline: Newline::LF,
            pad: true,
            line_length: None
        };
        let encoded_profile = Json::String(profile_data.to_base64(base64_config));

        let desired: BTreeMap<String, Json> = BTreeMap::new();
        let mut required: BTreeMap<String, Json> = BTreeMap::new();
        required.insert("firefox_profile".into(), encoded_profile);
        let capabilities = NewSessionParameters {
            desired: desired,
            required: required
        };

        let settings = MarionetteSettings {
            port: 2828,
            launcher: BrowserLauncher::None,
            e10s: false,
            log_level: None,
        };
        let handler = MarionetteHandler::new(settings);

        let mut gecko_profile = handler.load_profile(&capabilities).unwrap().unwrap();
        handler.set_prefs(&mut gecko_profile, true).unwrap();

        let prefs = gecko_profile.user_prefs().unwrap();

        println!("{:?}",prefs.prefs);

        assert_eq!(prefs.get("startup.homepage_welcome_url"),
                   Some(&Pref::new("data:text/html,PASS")));
        assert_eq!(prefs.get("marionette.defaultPrefs.enabled"),
                   Some(&Pref::new(true)));
    }

}

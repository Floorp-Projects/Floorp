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
use std::process::exit;
use std::net::{SocketAddr, IpAddr};
use std::str::FromStr;
use std::path::Path;

use argparse::{ArgumentParser, StoreTrue, Store};
use webdriver::server::start;

use marionette::{MarionetteHandler, BrowserLauncher, MarionetteSettings, extension_routes};

macro_rules! try_opt {
    ($expr:expr, $err_type:expr, $err_msg:expr) => ({
        match $expr {
            Some(x) => x,
            None => return Err(WebDriverError::new($err_type, $err_msg))
        }
    })
}

mod marionette;

struct Options {
    binary: String,
    webdriver_host: String,
    webdriver_port: u16,
    marionette_port: u16,
    connect_existing: bool,
    e10s: bool
}


fn parse_args() -> Options {
    let mut opts = Options {
        binary: "".to_owned(),
        webdriver_host: "127.0.0.1".to_owned(),
        webdriver_port: 4444u16,
        marionette_port: 2828u16,
        connect_existing: false,
        e10s: false
    };

    {
        let mut parser = ArgumentParser::new();
        parser.set_description("WebDriver to marionette proxy.");
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
        parser.parse_args_or_exit();
    }

    if opts.binary == "" && !opts.connect_existing {
        println!("Must supply a binary path or --connect-existing\n");
        exit(1)
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
        |_| {
            println!("Invalid host address");
            exit(1);
        }
        );

    let launcher = if opts.connect_existing {
        BrowserLauncher::None
    } else {
        BrowserLauncher::BinaryLauncher(Path::new(&opts.binary).to_path_buf())
    };

    let settings = MarionetteSettings::new(opts.marionette_port,
                                           launcher,
                                           opts.e10s);

    //TODO: what if binary isn't a valid path?
    start(addr, MarionetteHandler::new(settings), extension_routes());
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;
    use marionette::{MarionetteSettings, MarionetteHandler, BrowserLauncher};
    use webdriver::command::CapabilitiesParameters;
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
        let capabilities = CapabilitiesParameters {
            desired: desired,
            required: required
        };

        let handler = MarionetteHandler::new(
            MarionetteSettings::new(2828u16, BrowserLauncher::None, false));

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

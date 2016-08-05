#[macro_use]
extern crate clap;
#[macro_use]
extern crate log;
#[macro_use]
extern crate lazy_static;
extern crate env_logger;
extern crate hyper;
extern crate mozprofile;
extern crate mozrunner;
extern crate regex;
extern crate rustc_serialize;
#[macro_use]
extern crate webdriver;
extern crate zip;

use clap::{App, Arg};
use marionette::{MarionetteHandler, LogLevel, MarionetteSettings, extension_routes};
use std::borrow::ToOwned;
use std::io::Write;
use std::net::{SocketAddr, IpAddr};
use std::path::PathBuf;
use std::str::FromStr;
use webdriver::server::start;

macro_rules! try_opt {
    ($expr:expr, $err_type:expr, $err_msg:expr) => ({
        match $expr {
            Some(x) => x,
            None => return Err(WebDriverError::new($err_type, $err_msg))
        }
    })
}

mod marionette;

lazy_static! {
    pub static ref VERSION: String =
        format!("{}\n\n{}", crate_version!(), "The source is available at https://github.com/mozilla/geckodriver

This program is subject to the terms of the Mozilla Public License 2.0.
You can obtain a copy of the license at https://mozilla.org/MPL/2.0/.");
}

type ProgramResult = std::result::Result<(), (ExitCode, String)>;

enum ExitCode {
    Ok = 0,
    Usage = 64,
}

fn app<'a, 'b>() -> App<'a, 'b> {
    App::new("geckodriver")
        .about("WebDriver implementation for Firefox.")
        .version(&**VERSION)
        .arg(Arg::with_name("webdriver_host")
             .long("host")
             .value_name("HOST")
             .help("Host ip to use for WebDriver server (default: 127.0.0.1)")
             .takes_value(true))
        .arg(Arg::with_name("webdriver_port")
             .short("p")
             .long("port")
             .value_name("PORT")
             .help("Port to use for WebDriver server (default: 4444)")
             .takes_value(true))
        .arg(Arg::with_name("binary")
             .short("b")
             .long("binary")
             .value_name("BINARY")
             .help("Path to the Firefox binary, if no binary capability provided")
             .takes_value(true))
        .arg(Arg::with_name("marionette_port")
             .long("marionette-port")
             .value_name("PORT")
             .help("Port to use to connect to gecko (default: random free port)")
             .takes_value(true))
        .arg(Arg::with_name("connect_existing")
             .long("connect-existing")
             .requires("marionette_port")
             .help("Connect to an existing Firefox instance"))
        .arg(Arg::with_name("verbosity")
             .short("v")
             .multiple(true)
             .conflicts_with("log_level")
             .help("Set the level of verbosity. Pass once for debug level logging and twice for trace level logging"))
        .arg(Arg::with_name("log_level")
             .long("log")
             .takes_value(true)
             .value_name("LEVEL")
             .possible_values(
                 &["fatal", "error", "warn", "info", "config", "debug", "trace"])
             .help("Set Gecko log level"))
}

fn print_err(reason: &str) {
    let _ = writeln!(&mut ::std::io::stderr(), "\n{}", reason);
}

fn run() -> ProgramResult {
    let matches = app().get_matches();

    let host = matches.value_of("webdriver_host").unwrap_or("127.0.0.1");
    let port = match u16::from_str(matches.value_of("webdriver_port").unwrap_or("4444")) {
        Ok(x) => x,
        Err(_) => return Err((ExitCode::Usage, "Invalid WebDriver port".to_owned())),
    };
    let addr = match IpAddr::from_str(host) {
        Ok(addr) => SocketAddr::new(addr, port),
        Err(_) => return Err((ExitCode::Usage, "invalid host address".to_owned())),
    };

    let binary = matches.value_of("binary").map(|x| PathBuf::from(x));

    let marionette_port = match matches.value_of("marionette_port") {
        Some(x) => match u16::from_str(x) {
            Ok(x) => Some(x),
            Err(_) => return Err((ExitCode::Usage, "Invalid Marionette port".to_owned())),
        },
        None => None
    };

    // overrides defaults in Gecko
    // which are info for optimised builds
    // and debug for debug builds
    let log_level =
        if matches.is_present("log_level") {
            LogLevel::from_str(matches.value_of("log_level").unwrap()).ok()
        } else {
            match matches.occurrences_of("v") {
                0 => None,
                1 => Some(LogLevel::Debug),
                _ => Some(LogLevel::Trace),
            }
        };

    let settings = MarionetteSettings {
        port: marionette_port,
        binary: binary,
        connect_existing: matches.is_present("connect_existing"),
        log_level: log_level,
    };

    start(addr, MarionetteHandler::new(settings), extension_routes());

    Ok(())
}

fn main() {
    let _ = env_logger::init();

    let exit_code = match run() {
        Ok(_) => ExitCode::Ok,
        Err((exit_code, reason)) => {
            print_err(&reason.to_string());
            exit_code
        },
    };

    std::io::stdout().flush().unwrap();
    std::process::exit(exit_code as i32);
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;
    use marionette::{MarionetteSettings, MarionetteHandler};
    use webdriver::command::NewSessionParameters;
    use rustc_serialize::json::Json;
    use std::fs::File;
    use rustc_serialize::base64::{ToBase64, Config, CharacterSet, Newline};
    use mozprofile::preferences::Pref;
    use std::io::Read;

    const MARIONETTE_PORT: u16 = 2828;

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
            port: None,
            binary: None,
            connect_existing: false,
            log_level: None,
        };
        let handler = MarionetteHandler::new(settings);

        let mut gecko_profile = handler.load_profile(&capabilities).unwrap().unwrap();
        handler.set_prefs(MARIONETTE_PORT, &mut gecko_profile, true).unwrap();

        let prefs = gecko_profile.user_prefs().unwrap();

        println!("{:?}",prefs.prefs);

        assert_eq!(prefs.get("startup.homepage_welcome_url"),
                   Some(&Pref::new("data:text/html,PASS")));
        assert_eq!(prefs.get("marionette.defaultPrefs.enabled"),
                   Some(&Pref::new(true)));
    }

}

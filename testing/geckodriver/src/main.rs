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

macro_rules! try_opt {
    ($expr:expr, $err_type:expr, $err_msg:expr) => ({
        match $expr {
            Some(x) => x,
            None => return Err(WebDriverError::new($err_type, $err_msg))
        }
    })
}

mod marionette;

type ProgramResult = std::result::Result<(), (ExitCode, String)>;

enum ExitCode {
    Ok = 0,
    Usage = 64,
}

fn app<'a, 'b>() -> App<'a, 'b> {
    App::new(format!("geckodriver {}", crate_version!()))
        .about("WebDriver implementation for Firefox.")
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
             .help("Path to the Firefox binary")
             .takes_value(true))
        .arg(Arg::with_name("marionette_port")
             .long("marionette-port")
             .value_name("PORT")
             .help("Port to use to connect to Gecko (default: random free port)")
             .takes_value(true))
        .arg(Arg::with_name("connect_existing")
             .long("connect-existing")
             .requires("marionette_port")
             .help("Connect to an existing Firefox instance"))
        .arg(Arg::with_name("verbosity")
             .short("v")
             .multiple(true)
             .conflicts_with("log_level")
             .help("Log level verbosity (-v for debug and -vv for trace level)"))
        .arg(Arg::with_name("log_level")
             .long("log")
             .takes_value(true)
             .value_name("LEVEL")
             .possible_values(
                 &["fatal", "error", "warn", "info", "config", "debug", "trace"])
             .help("Set Gecko log level"))
         .arg(Arg::with_name("version")
             .short("V")
             .long("version")
             .help("Prints version and copying information"))
}

fn run() -> ProgramResult {
    let matches = app().get_matches();

    if matches.is_present("version") {
        println!("geckodriver {}\n\n{}", crate_version!(),
"The source code of this program is available at
https://github.com/mozilla/geckodriver.

This program is subject to the terms of the Mozilla Public License 2.0.
You can obtain a copy of the license at https://mozilla.org/MPL/2.0/.");
        return Ok(())
    }

    let host = matches.value_of("webdriver_host").unwrap_or("127.0.0.1");
    let port = match u16::from_str(matches.value_of("webdriver_port").unwrap_or("4444")) {
        Ok(x) => x,
        Err(_) => return Err((ExitCode::Usage, "invalid WebDriver port".to_owned())),
    };
    let addr = match IpAddr::from_str(host) {
        Ok(addr) => SocketAddr::new(addr, port),
        Err(_) => return Err((ExitCode::Usage, "invalid host address".to_owned())),
    };

    let binary = matches.value_of("binary").map(|x| PathBuf::from(x));

    let marionette_port = match matches.value_of("marionette_port") {
        Some(x) => match u16::from_str(x) {
            Ok(x) => Some(x),
            Err(_) => return Err((ExitCode::Usage, "invalid Marionette port".to_owned())),
        },
        None => None
    };

    // overrides defaults in Gecko
    // which are info for optimised builds
    // and debug for debug builds
    let log_level = if matches.is_present("log_level") {
        LogLevel::from_str(matches.value_of("log_level").unwrap()).ok()
    } else {
        match matches.occurrences_of("verbosity") {
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

    let handler = MarionetteHandler::new(settings);
    let listening = try!(webdriver::server::start(addr, handler, extension_routes())
        .or(Err((ExitCode::Usage, "Invalid host address".to_owned()))));
    info!("Listening on {}", listening.socket);

    Ok(())
}

fn main() {
    let _ = env_logger::init();

    let exit_code = match run() {
        Ok(_) => ExitCode::Ok,
        Err((exit_code, reason)) => {
            error!("{}", reason);
            exit_code
        },
    };

    std::io::stdout().flush().unwrap();
    std::process::exit(exit_code as i32);
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;
    use marionette::{FirefoxOptions, MarionetteHandler};
    use webdriver::command::NewSessionParameters;
    use rustc_serialize::json::Json;
    use std::fs::File;
    use rustc_serialize::base64::{ToBase64, Config, CharacterSet, Newline};
    use mozprofile::preferences::Pref;
    use std::io::Read;
    use std::default::Default;

    fn example_profile() -> Json {
        let mut profile_data = Vec::with_capacity(1024);
        let mut profile = File::open("src/tests/profile.zip").unwrap();
        profile.read_to_end(&mut profile_data).unwrap();
        let base64_config = Config {
            char_set: CharacterSet::Standard,
            newline: Newline::LF,
            pad: true,
            line_length: None
        };
        Json::String(profile_data.to_base64(base64_config))
    }

    fn capabilities() -> NewSessionParameters {
        let desired: BTreeMap<String, Json> = BTreeMap::new();
        let required: BTreeMap<String, Json> = BTreeMap::new();
        NewSessionParameters {
            desired: desired,
            required: required
        }
    }

    #[test]
    fn test_profile() {
        let encoded_profile = example_profile();

        let mut capabilities = capabilities();
        let mut firefox_options: BTreeMap<String, Json> = BTreeMap::new();
        firefox_options.insert("profile".into(), encoded_profile);
        capabilities.required.insert("firefoxOptions".into(), Json::Object(firefox_options));

        let options = FirefoxOptions::from_capabilities(&mut capabilities).unwrap();
        let mut profile = options.profile.unwrap();
        let prefs = profile.user_prefs().unwrap();

        println!("{:?}",prefs.prefs);

        assert_eq!(prefs.get("startup.homepage_welcome_url"),
                   Some(&Pref::new("data:text/html,PASS")));
    }

    #[test]
    fn test_prefs() {
        let encoded_profile = example_profile();

        let mut capabilities = capabilities();
        let mut firefox_options: BTreeMap<String, Json> = BTreeMap::new();
        firefox_options.insert("profile".into(), encoded_profile);
        let mut prefs: BTreeMap<String, Json> = BTreeMap::new();
        prefs.insert("browser.display.background_color".into(), Json::String("#00ff00".into()));
        firefox_options.insert("prefs".into(), Json::Object(prefs));
        capabilities.required.insert("firefoxOptions".into(), Json::Object(firefox_options));


        let options = FirefoxOptions::from_capabilities(&mut capabilities).unwrap();
        let mut profile = options.profile.unwrap();

        let handler = MarionetteHandler::new(Default::default());
        handler.set_prefs(2828, &mut profile, true, options.prefs).unwrap();

        let prefs_set = profile.user_prefs().unwrap();
        println!("{:?}",prefs_set.prefs);
        assert_eq!(prefs_set.get("startup.homepage_welcome_url"),
                   Some(&Pref::new("data:text/html,PASS")));
        assert_eq!(prefs_set.get("browser.display.background_color"),
                   Some(&Pref::new("#00ff00")));
        assert_eq!(prefs_set.get("marionette.defaultPrefs.port"),
                   Some(&Pref::new(2828)));
    }
}

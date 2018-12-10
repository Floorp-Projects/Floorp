extern crate base64;
extern crate chrono;
#[macro_use]
extern crate clap;
#[macro_use]
extern crate lazy_static;
extern crate hyper;
extern crate mozprofile;
extern crate mozrunner;
extern crate mozversion;
extern crate regex;
extern crate serde;
#[macro_use]
extern crate serde_derive;
extern crate serde_json;
extern crate uuid;
extern crate webdriver;
extern crate zip;

#[macro_use]
extern crate log;

use std::io::Write;
use std::net::{IpAddr, SocketAddr};
use std::path::PathBuf;
use std::str::FromStr;

use clap::{App, Arg};

macro_rules! try_opt {
    ($expr:expr, $err_type:expr, $err_msg:expr) => {{
        match $expr {
            Some(x) => x,
            None => return Err(WebDriverError::new($err_type, $err_msg)),
        }
    }};
}

mod build;
mod capabilities;
mod command;
mod logging;
mod marionette;
mod prefs;

#[cfg(test)]
pub mod test;

use crate::build::BuildInfo;
use crate::command::extension_routes;
use crate::marionette::{MarionetteHandler, MarionetteSettings};

type ProgramResult = std::result::Result<(), (ExitCode, String)>;

enum ExitCode {
    Ok = 0,
    Usage = 64,
    Unavailable = 69,
}

fn print_version() {
    println!("geckodriver {}", BuildInfo);
    println!("");
    println!("The source code of this program is available from");
    println!("testing/geckodriver in https://hg.mozilla.org/mozilla-central.");
    println!("");
    println!("This program is subject to the terms of the Mozilla Public License 2.0.");
    println!("You can obtain a copy of the license at https://mozilla.org/MPL/2.0/.");
}

fn app<'a, 'b>() -> App<'a, 'b> {
    App::new(format!("geckodriver {}", crate_version!()))
        .about("WebDriver implementation for Firefox.")
        .arg(
            Arg::with_name("webdriver_host")
                .long("host")
                .value_name("HOST")
                .help("Host ip to use for WebDriver server (default: 127.0.0.1)")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("webdriver_port")
                .short("p")
                .long("port")
                .value_name("PORT")
                .help("Port to use for WebDriver server (default: 4444)")
                .takes_value(true)
                .alias("webdriver-port"),
        )
        .arg(
            Arg::with_name("binary")
                .short("b")
                .long("binary")
                .value_name("BINARY")
                .help("Path to the Firefox binary")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("marionette_host")
                .long("marionette-host")
                .value_name("HOST")
                .help("Host to use to connect to Gecko (default: 127.0.0.1)")
                .takes_value(true)
        )
        .arg(
            Arg::with_name("marionette_port")
                .long("marionette-port")
                .value_name("PORT")
                .help("Port to use to connect to Gecko (default: system-allocated port)")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("connect_existing")
                .long("connect-existing")
                .requires("marionette_port")
                .help("Connect to an existing Firefox instance"),
        )
        .arg(
            Arg::with_name("jsdebugger")
                .long("jsdebugger")
                .takes_value(false)
                .help("Attach browser toolbox debugger for Firefox"),
        )
        .arg(
            Arg::with_name("verbosity")
                .short("v")
                .multiple(true)
                .conflicts_with("log_level")
                .help("Log level verbosity (-v for debug and -vv for trace level)"),
        )
        .arg(
            Arg::with_name("log_level")
                .long("log")
                .takes_value(true)
                .value_name("LEVEL")
                .possible_values(&["fatal", "error", "warn", "info", "config", "debug", "trace"])
                .help("Set Gecko log level"),
        )
        .arg(
            Arg::with_name("version")
                .short("V")
                .long("version")
                .help("Prints version and copying information"),
        )
}

fn run() -> ProgramResult {
    let matches = app().get_matches();

    if matches.is_present("version") {
        print_version();
        return Ok(());
    }

    let host = matches.value_of("webdriver_host").unwrap_or("127.0.0.1");
    let port = match u16::from_str(
        matches
            .value_of("webdriver_port")
            .or(matches.value_of("webdriver_port_alias"))
            .unwrap_or("4444"),
    ) {
        Ok(x) => x,
        Err(_) => return Err((ExitCode::Usage, "invalid WebDriver port".into())),
    };
    let addr = match IpAddr::from_str(host) {
        Ok(addr) => SocketAddr::new(addr, port),
        Err(_) => return Err((ExitCode::Usage, "invalid host address".into())),
    };

    let binary = matches.value_of("binary").map(PathBuf::from);

    let marionette_host = matches.value_of("marionette_host")
        .unwrap_or("127.0.0.1").to_string();
    let marionette_port = match matches.value_of("marionette_port") {
        Some(x) => match u16::from_str(x) {
            Ok(x) => Some(x),
            Err(_) => return Err((ExitCode::Usage, "invalid Marionette port".into())),
        },
        None => None,
    };

    let log_level = if matches.is_present("log_level") {
        logging::Level::from_str(matches.value_of("log_level").unwrap()).ok()
    } else {
        match matches.occurrences_of("verbosity") {
            0 => Some(logging::Level::Info),
            1 => Some(logging::Level::Debug),
            _ => Some(logging::Level::Trace),
        }
    };
    if let Some(ref level) = log_level {
        logging::init_with_level(*level).unwrap();
    } else {
        logging::init().unwrap();
    }

    let settings = MarionetteSettings {
        host: marionette_host,
        port: marionette_port,
        binary,
        connect_existing: matches.is_present("connect_existing"),
        jsdebugger: matches.is_present("jsdebugger"),
    };
    let handler = MarionetteHandler::new(settings);
    let listening = webdriver::server::start(addr, handler, &extension_routes()[..])
        .map_err(|err| (ExitCode::Unavailable, err.to_string()))?;
    debug!("Listening on {}", listening.socket);

    Ok(())
}

fn main() {
    let exit_code = match run() {
        Ok(_) => ExitCode::Ok,
        Err((exit_code, reason)) => {
            error!("{}", reason);
            exit_code
        }
    };

    std::io::stdout().flush().unwrap();
    std::process::exit(exit_code as i32);
}

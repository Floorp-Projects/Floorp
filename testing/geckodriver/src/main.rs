#![forbid(unsafe_code)]

extern crate base64;
extern crate chrono;
#[macro_use]
extern crate clap;
#[macro_use]
extern crate lazy_static;
extern crate hyper;
extern crate marionette as marionette_rs;
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

use std::env;
use std::fmt;
use std::io;
use std::net::{IpAddr, SocketAddr};
use std::path::PathBuf;
use std::result;
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

use crate::command::extension_routes;
use crate::logging::Level;
use crate::marionette::{MarionetteHandler, MarionetteSettings};

const EXIT_SUCCESS: i32 = 0;
const EXIT_USAGE: i32 = 64;
const EXIT_UNAVAILABLE: i32 = 69;

enum FatalError {
    Parsing(clap::Error),
    Usage(String),
    Server(io::Error),
}

impl FatalError {
    fn exit_code(&self) -> i32 {
        use FatalError::*;
        match *self {
            Parsing(_) | Usage(_) => EXIT_USAGE,
            Server(_) => EXIT_UNAVAILABLE,
        }
    }

    fn help_included(&self) -> bool {
        match *self {
            FatalError::Parsing(_) => true,
            _ => false,
        }
    }
}

impl From<clap::Error> for FatalError {
    fn from(err: clap::Error) -> FatalError {
        FatalError::Parsing(err)
    }
}

impl From<io::Error> for FatalError {
    fn from(err: io::Error) -> FatalError {
        FatalError::Server(err)
    }
}

// harmonise error message from clap to avoid duplicate "error:" prefix
impl fmt::Display for FatalError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use FatalError::*;
        let s = match *self {
            Parsing(ref err) => err.to_string(),
            Usage(ref s) => format!("error: {}", s),
            Server(ref err) => format!("error: {}", err.to_string()),
        };
        write!(f, "{}", s)
    }
}

macro_rules! usage {
    ($msg:expr) => {
        return Err(FatalError::Usage($msg.to_string()));
    };

    ($fmt:expr, $($arg:tt)+) => {
        return Err(FatalError::Usage(format!($fmt, $($arg)+)));
    };
}

type ProgramResult<T> = result::Result<T, FatalError>;

enum Operation {
    Help,
    Version,
    Server {
        log_level: Option<Level>,
        address: SocketAddr,
        settings: MarionetteSettings,
    },
}

fn parse_args(app: &mut App) -> ProgramResult<Operation> {
    let matches = app.get_matches_from_safe_borrow(env::args())?;

    let log_level = if matches.is_present("log_level") {
        Level::from_str(matches.value_of("log_level").unwrap()).ok()
    } else {
        Some(match matches.occurrences_of("verbosity") {
            0 => Level::Info,
            1 => Level::Debug,
            _ => Level::Trace,
        })
    };

    let host = matches.value_of("webdriver_host").unwrap();
    let port = {
        let s = matches.value_of("webdriver_port").unwrap();
        match u16::from_str(s) {
            Ok(n) => n,
            Err(e) => usage!("invalid --port: {}: {}", e, s),
        }
    };
    let address = match IpAddr::from_str(host) {
        Ok(addr) => SocketAddr::new(addr, port),
        Err(e) => usage!("{}: {}:{}", e, host, port),
    };

    let binary = matches.value_of("binary").map(PathBuf::from);

    let marionette_host = matches.value_of("marionette_host").unwrap();
    let marionette_port = match matches.value_of("marionette_port") {
        Some(s) => match u16::from_str(s) {
            Ok(n) => Some(n),
            Err(e) => usage!("invalid --marionette-port: {}", e),
        },
        None => None,
    };

    let op = if matches.is_present("help") {
        Operation::Help
    } else if matches.is_present("version") {
        Operation::Version
    } else {
        let settings = MarionetteSettings {
            host: marionette_host.to_string(),
            port: marionette_port,
            binary,
            connect_existing: matches.is_present("connect_existing"),
            jsdebugger: matches.is_present("jsdebugger"),
        };
        Operation::Server {
            log_level,
            address,
            settings,
        }
    };

    Ok(op)
}

fn inner_main(app: &mut App) -> ProgramResult<()> {
    match parse_args(app)? {
        Operation::Help => print_help(app),
        Operation::Version => print_version(),

        Operation::Server {
            log_level,
            address,
            settings,
        } => {
            if let Some(ref level) = log_level {
                logging::init_with_level(*level).unwrap();
            } else {
                logging::init().unwrap();
            }

            let handler = MarionetteHandler::new(settings);
            let listening = webdriver::server::start(address, handler, extension_routes())?;
            debug!("Listening on {}", listening.socket);
        }
    }

    Ok(())
}

fn main() {
    use std::process::exit;

    let mut app = make_app();

    // use std::process:Termination when it graduates
    exit(match inner_main(&mut app) {
        Ok(_) => EXIT_SUCCESS,

        Err(e) => {
            eprintln!("{}: {}", get_program_name(), e);
            if !e.help_included() {
                print_help(&mut app);
            }

            e.exit_code()
        }
    });
}

fn make_app<'a, 'b>() -> App<'a, 'b> {
    App::new(format!("geckodriver {}", build::BuildInfo))
        .about("WebDriver implementation for Firefox")
        .arg(
            Arg::with_name("webdriver_host")
                .long("host")
                .takes_value(true)
                .value_name("HOST")
                .default_value("127.0.0.1")
                .help("Host IP to use for WebDriver server"),
        )
        .arg(
            Arg::with_name("webdriver_port")
                .short("p")
                .long("port")
                .takes_value(true)
                .value_name("PORT")
                .default_value("4444")
                .help("Port to use for WebDriver server"),
        )
        .arg(
            Arg::with_name("binary")
                .short("b")
                .long("binary")
                .takes_value(true)
                .value_name("BINARY")
                .help("Path to the Firefox binary"),
        )
        .arg(
            Arg::with_name("marionette_host")
                .long("marionette-host")
                .takes_value(true)
                .value_name("HOST")
                .default_value("127.0.0.1")
                .help("Host to use to connect to Gecko"),
        )
        .arg(
            Arg::with_name("marionette_port")
                .long("marionette-port")
                .takes_value(true)
                .value_name("PORT")
                .help("Port to use to connect to Gecko [default: system-allocated port]"),
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
                .help("Attach browser toolbox debugger for Firefox"),
        )
        .arg(
            Arg::with_name("verbosity")
                .multiple(true)
                .conflicts_with("log_level")
                .short("v")
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
            Arg::with_name("help")
                .short("h")
                .long("help")
                .help("Prints this message"),
        )
        .arg(
            Arg::with_name("version")
                .short("V")
                .long("version")
                .help("Prints version and copying information"),
        )
}

fn get_program_name() -> String {
    env::args().next().unwrap()
}

fn print_help(app: &mut App) {
    app.print_help().ok();
    println!();
}

fn print_version() {
    println!("geckodriver {}", build::BuildInfo);
    println!();
    println!("The source code of this program is available from");
    println!("testing/geckodriver in https://hg.mozilla.org/mozilla-central.");
    println!();
    println!("This program is subject to the terms of the Mozilla Public License 2.0.");
    println!("You can obtain a copy of the license at https://mozilla.org/MPL/2.0/.");
}

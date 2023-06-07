#![forbid(unsafe_code)]

extern crate chrono;
#[macro_use]
extern crate clap;
#[macro_use]
extern crate lazy_static;
extern crate hyper;
extern crate marionette as marionette_rs;
extern crate mozdevice;
extern crate mozprofile;
extern crate mozrunner;
extern crate mozversion;
extern crate regex;
extern crate serde;
#[macro_use]
extern crate serde_derive;
extern crate serde_json;
extern crate serde_yaml;
extern crate tempfile;
extern crate url;
extern crate uuid;
extern crate webdriver;
extern crate zip;

#[macro_use]
extern crate log;

use std::env;
use std::fmt;
use std::io;
use std::net::{IpAddr, SocketAddr, ToSocketAddrs};
use std::path::PathBuf;
use std::result;
use std::str::FromStr;

use clap::{Arg, ArgAction, Command};

macro_rules! try_opt {
    ($expr:expr, $err_type:expr, $err_msg:expr) => {{
        match $expr {
            Some(x) => x,
            None => return Err(WebDriverError::new($err_type, $err_msg)),
        }
    }};
}

mod android;
mod browser;
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
use mozdevice::AndroidStorageInput;
use url::{Host, Url};

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
        matches!(*self, FatalError::Parsing(_))
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
            Server(ref err) => format!("error: {}", err),
        };
        write!(f, "{}", s)
    }
}

macro_rules! usage {
    ($msg:expr) => {
        return Err(FatalError::Usage($msg.to_string()))
    };

    ($fmt:expr, $($arg:tt)+) => {
        return Err(FatalError::Usage(format!($fmt, $($arg)+)))
    };
}

type ProgramResult<T> = result::Result<T, FatalError>;

#[allow(clippy::large_enum_variant)]
enum Operation {
    Help,
    Version,
    Server {
        log_level: Option<Level>,
        log_truncate: bool,
        address: SocketAddr,
        allow_hosts: Vec<Host>,
        allow_origins: Vec<Url>,
        settings: MarionetteSettings,
        deprecated_storage_arg: bool,
    },
}

/// Get a socket address from the provided host and port
///
/// # Arguments
/// * `webdriver_host` - The hostname on which the server will listen
/// * `webdriver_port` - The port on which the server will listen
///
/// When the host and port resolve to multiple addresses, prefer
/// IPv4 addresses vs IPv6.
fn server_address(webdriver_host: &str, webdriver_port: u16) -> ProgramResult<SocketAddr> {
    let mut socket_addrs = match format!("{}:{}", webdriver_host, webdriver_port).to_socket_addrs()
    {
        Ok(addrs) => addrs.collect::<Vec<_>>(),
        Err(e) => usage!("{}: {}:{}", e, webdriver_host, webdriver_port),
    };
    if socket_addrs.is_empty() {
        usage!(
            "Unable to resolve host: {}:{}",
            webdriver_host,
            webdriver_port
        )
    }
    // Prefer ipv4 address
    socket_addrs.sort_by(|a, b| {
        let a_val = i32::from(!a.ip().is_ipv4());
        let b_val = i32::from(!b.ip().is_ipv4());
        a_val.partial_cmp(&b_val).expect("Comparison failed")
    });
    Ok(socket_addrs.remove(0))
}

/// Parse a given string into a Host
fn parse_hostname(webdriver_host: &str) -> Result<Host, url::ParseError> {
    let host_str = if let Ok(ip_addr) = IpAddr::from_str(webdriver_host) {
        // In this case we have an IP address as the host
        if ip_addr.is_ipv6() {
            // Convert to quoted form
            format!("[{}]", &webdriver_host)
        } else {
            webdriver_host.into()
        }
    } else {
        webdriver_host.into()
    };

    Host::parse(&host_str)
}

/// Get a list of default hostnames to allow
///
/// This only covers domain names, not IP addresses, since IP adresses
/// are always accepted.
fn get_default_allowed_hosts(ip: IpAddr) -> Vec<Host> {
    let localhost_is_loopback = ("localhost".to_string(), 80)
        .to_socket_addrs()
        .map(|addr_iter| {
            addr_iter
                .map(|addr| addr.ip())
                .filter(|ip| ip.is_loopback())
        })
        .iter()
        .len()
        > 0;
    if ip.is_loopback() && localhost_is_loopback {
        vec![Host::parse("localhost").unwrap()]
    } else {
        vec![]
    }
}

fn get_allowed_hosts(host: Host, allow_hosts: Option<clap::parser::ValuesRef<Host>>) -> Vec<Host> {
    allow_hosts
        .map(|hosts| hosts.cloned().collect())
        .unwrap_or_else(|| match host {
            Host::Domain(_) => {
                vec![host.clone()]
            }
            Host::Ipv4(ip) => get_default_allowed_hosts(IpAddr::V4(ip)),
            Host::Ipv6(ip) => get_default_allowed_hosts(IpAddr::V6(ip)),
        })
}

fn get_allowed_origins(allow_origins: Option<clap::parser::ValuesRef<Url>>) -> Vec<Url> {
    allow_origins.into_iter().flatten().cloned().collect()
}

fn parse_args(cmd: &mut Command) -> ProgramResult<Operation> {
    let args = cmd.try_get_matches_from_mut(env::args())?;

    if args.get_flag("help") {
        return Ok(Operation::Help);
    } else if args.get_flag("version") {
        return Ok(Operation::Version);
    }

    let log_level = if let Some(log_level) = args.get_one::<String>("log_level") {
        Level::from_str(log_level).ok()
    } else {
        Some(match args.get_count("verbosity") {
            0 => Level::Info,
            1 => Level::Debug,
            _ => Level::Trace,
        })
    };

    let webdriver_host = args.get_one::<String>("webdriver_host").unwrap();
    let webdriver_port = {
        let s = args.get_one::<String>("webdriver_port").unwrap();
        match u16::from_str(s) {
            Ok(n) => n,
            Err(e) => usage!("invalid --port: {}: {}", e, s),
        }
    };

    let android_storage = args
        .get_one::<String>("android_storage")
        .and_then(|arg| AndroidStorageInput::from_str(arg).ok())
        .unwrap_or(AndroidStorageInput::Auto);

    let binary = args.get_one::<String>("binary").map(PathBuf::from);

    let profile_root = args.get_one::<String>("profile_root").map(PathBuf::from);

    // Try to create a temporary directory on startup to check that the directory exists and is writable
    {
        let tmp_dir = if let Some(ref tmp_root) = profile_root {
            tempfile::tempdir_in(tmp_root)
        } else {
            tempfile::tempdir()
        };
        if tmp_dir.is_err() {
            usage!("Unable to write to temporary directory; consider --profile-root with a writeable directory")
        }
    }

    let marionette_host = args.get_one::<String>("marionette_host").unwrap();
    let marionette_port = match args.get_one::<String>("marionette_port") {
        Some(s) => match u16::from_str(s) {
            Ok(n) => Some(n),
            Err(e) => usage!("invalid --marionette-port: {}", e),
        },
        None => None,
    };

    // For Android the port on the device must be the same as the one on the
    // host. For now default to 9222, which is the default for --remote-debugging-port.
    let websocket_port = match args.get_one::<String>("websocket_port") {
        Some(s) => match u16::from_str(s) {
            Ok(n) => n,
            Err(e) => usage!("invalid --websocket-port: {}", e),
        },
        None => 9222,
    };

    let host = match parse_hostname(webdriver_host) {
        Ok(name) => name,
        Err(e) => usage!("invalid --host {}: {}", webdriver_host, e),
    };

    let allow_hosts = get_allowed_hosts(host, args.get_many("allow_hosts"));

    let allow_origins = get_allowed_origins(args.get_many("allow_origins"));

    let address = server_address(webdriver_host, webdriver_port)?;

    let settings = MarionetteSettings {
        binary,
        profile_root,
        connect_existing: args.get_flag("connect_existing"),
        host: marionette_host.into(),
        port: marionette_port,
        websocket_port,
        allow_hosts: allow_hosts.clone(),
        allow_origins: allow_origins.clone(),
        jsdebugger: args.get_flag("jsdebugger"),
        android_storage,
    };
    Ok(Operation::Server {
        log_level,
        log_truncate: !args.get_flag("log_no_truncate"),
        allow_hosts,
        allow_origins,
        address,
        settings,
        deprecated_storage_arg: args.contains_id("android_storage"),
    })
}

fn inner_main(cmd: &mut Command) -> ProgramResult<()> {
    match parse_args(cmd)? {
        Operation::Help => print_help(cmd),
        Operation::Version => print_version(),

        Operation::Server {
            log_level,
            log_truncate,
            address,
            allow_hosts,
            allow_origins,
            settings,
            deprecated_storage_arg,
        } => {
            if let Some(ref level) = log_level {
                logging::init_with_level(*level, log_truncate).unwrap();
            } else {
                logging::init(log_truncate).unwrap();
            }

            if deprecated_storage_arg {
                warn!("--android-storage argument is deprecated and will be removed soon.");
            };

            let handler = MarionetteHandler::new(settings);
            let listening = webdriver::server::start(
                address,
                allow_hosts,
                allow_origins,
                handler,
                extension_routes(),
            )?;
            info!("Listening on {}", listening.socket);
        }
    }

    Ok(())
}

fn main() {
    use std::process::exit;

    let mut cmd = make_command();

    // use std::process:Termination when it graduates
    exit(match inner_main(&mut cmd) {
        Ok(_) => EXIT_SUCCESS,

        Err(e) => {
            eprintln!("{}: {}", get_program_name(), e);
            if !e.help_included() {
                print_help(&mut cmd);
            }

            e.exit_code()
        }
    });
}

fn make_command() -> Command {
    Command::new(format!("geckodriver {}", build::build_info()))
        .disable_help_flag(true)
        .disable_version_flag(true)
        .about("WebDriver implementation for Firefox")
        .arg(
            Arg::new("allow_hosts")
                .long("allow-hosts")
                .num_args(1..)
                .value_parser(clap::builder::ValueParser::new(Host::parse))
                .value_name("ALLOW_HOSTS")
                .help("List of hostnames to allow. By default the value of --host is allowed, and in addition if that's a well known local address, other variations on well known local addresses are allowed. If --allow-hosts is provided only exactly those hosts are allowed."),
        )
        .arg(
            Arg::new("allow_origins")
                .long("allow-origins")
                .num_args(1..)
                .value_parser(clap::builder::ValueParser::new(Url::parse))
                .value_name("ALLOW_ORIGINS")
                .help("List of request origins to allow. These must be formatted as scheme://host:port. By default any request with an origin header is rejected. If --allow-origins is provided then only exactly those origins are allowed."),
        )
        .arg(
            Arg::new("android_storage")
                .long("android-storage")
                .value_parser(["auto", "app", "internal", "sdcard"])
                .value_name("ANDROID_STORAGE")
                .help("Selects storage location to be used for test data (deprecated)."),
        )
        .arg(
            Arg::new("binary")
                .short('b')
                .long("binary")
                .num_args(1)
                .value_name("BINARY")
                .help("Path to the Firefox binary"),
        )
        .arg(
            Arg::new("connect_existing")
                .long("connect-existing")
                .requires("marionette_port")
                .action(ArgAction::SetTrue)
                .help("Connect to an existing Firefox instance"),
        )
        .arg(
            Arg::new("help")
                .short('h')
                .long("help")
                .action(ArgAction::SetTrue)
                .help("Prints this message"),
        )
        .arg(
            Arg::new("webdriver_host")
                .long("host")
                .num_args(1)
                .value_name("HOST")
                .default_value("127.0.0.1")
                .help("Host IP to use for WebDriver server"),
        )
        .arg(
            Arg::new("jsdebugger")
                .long("jsdebugger")
                .action(ArgAction::SetTrue)
                .help("Attach browser toolbox debugger for Firefox"),
        )
        .arg(
            Arg::new("log_level")
                .long("log")
                .num_args(1)
                .value_name("LEVEL")
                .value_parser(["fatal", "error", "warn", "info", "config", "debug", "trace"])
                .help("Set Gecko log level"),
        )
        .arg(
            Arg::new("log_no_truncate")
                .long("log-no-truncate")
                .action(ArgAction::SetTrue)
                .help("Disable truncation of long log lines"),
        )
        .arg(
            Arg::new("marionette_host")
                .long("marionette-host")
                .num_args(1)
                .value_name("HOST")
                .default_value("127.0.0.1")
                .help("Host to use to connect to Gecko"),
        )
        .arg(
            Arg::new("marionette_port")
                .long("marionette-port")
                .num_args(1)
                .value_name("PORT")
                .help("Port to use to connect to Gecko [default: system-allocated port]"),
        )
        .arg(
            Arg::new("webdriver_port")
                .short('p')
                .long("port")
                .num_args(1)
                .value_name("PORT")
                .default_value("4444")
                .help("Port to use for WebDriver server"),
        )
        .arg(
            Arg::new("profile_root")
                .long("profile-root")
                .num_args(1)
                .value_name("PROFILE_ROOT")
                .help("Directory in which to create profiles. Defaults to the system temporary directory."),
        )
        .arg(
            Arg::new("verbosity")
                .conflicts_with("log_level")
                .short('v')
                .action(ArgAction::Count)
                .help("Log level verbosity (-v for debug and -vv for trace level)"),
        )
        .arg(
            Arg::new("version")
                .short('V')
                .long("version")
                .action(ArgAction::SetTrue)
                .help("Prints version and copying information"),
        )
        .arg(
            Arg::new("websocket_port")
                .long("websocket-port")
                .num_args(1)
                .value_name("PORT")
                .conflicts_with("connect_existing")
                .help("Port to use to connect to WebDriver BiDi [default: 9222]"),
        )
}

fn get_program_name() -> String {
    env::args().next().unwrap()
}

fn print_help(cmd: &mut Command) {
    cmd.print_help().ok();
    println!();
}

fn print_version() {
    println!("geckodriver {}", build::build_info());
    println!();
    println!("The source code of this program is available from");
    println!("testing/geckodriver in https://hg.mozilla.org/mozilla-central.");
    println!();
    println!("This program is subject to the terms of the Mozilla Public License 2.0.");
    println!("You can obtain a copy of the license at https://mozilla.org/MPL/2.0/.");
}

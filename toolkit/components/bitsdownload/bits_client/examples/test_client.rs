/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

extern crate bits_client;
extern crate comedy;
//extern crate ctrlc;
extern crate failure;
extern crate failure_derive;
extern crate guid_win;

use std::convert;
use std::env;
use std::ffi::{OsStr, OsString};
use std::process;
use std::str::FromStr;
use std::sync::{Arc, Mutex};

use failure::{AsFail, Fail};

use bits_client::bits_protocol::HResultMessage;
use bits_client::{BitsClient, BitsJobState, BitsMonitorClient, BitsProxyUsage, Guid, PipeError};

#[derive(Debug, Fail)]
enum MyError {
    #[fail(display = "{}", _0)]
    Msg(String),
    #[fail(display = "HResult")]
    HResult(#[fail(cause)] comedy::HResult),
    #[fail(display = "Win32Error")]
    Win32Error(#[fail(cause)] comedy::Win32Error),
    #[fail(display = "PipeError")]
    PipeError(#[fail(cause)] PipeError),
    #[fail(display = "HResultMessage")]
    HResultMessage(#[fail(cause)] HResultMessage),
}

impl convert::From<PipeError> for MyError {
    fn from(err: PipeError) -> MyError {
        MyError::PipeError(err)
    }
}

impl convert::From<comedy::HResult> for MyError {
    fn from(err: comedy::HResult) -> MyError {
        MyError::HResult(err)
    }
}

impl convert::From<comedy::Win32Error> for MyError {
    fn from(err: comedy::Win32Error) -> MyError {
        MyError::Win32Error(err)
    }
}

impl convert::From<HResultMessage> for MyError {
    fn from(err: HResultMessage) -> MyError {
        MyError::HResultMessage(err)
    }
}

macro_rules! bail {
    ($e:expr) => {
        return Err($crate::MyError::Msg($e.to_string()));
    };
    ($fmt:expr, $($arg:tt)*) => {
        return Err($crate::MyError::Msg(format!($fmt, $($arg)*)));
    };
}

type Result = std::result::Result<(), MyError>;

pub fn main() {
    if let Err(err) = entry() {
        eprintln!("{}", err);
        for cause in err.as_fail().iter_causes() {
            eprintln!("caused by {}", cause);
        }

        process::exit(1);
    } else {
        println!("OK");
    }
}

const EXE_NAME: &'static str = "test_client";

fn usage() -> String {
    format!(
        concat!(
            "Usage {0} <command> ",
            "[local-service] ",
            "[args...]\n",
            "Commands:\n",
            "  bits-start <URL> <local file>\n",
            "  bits-monitor <GUID>\n",
            "  bits-bg <GUID>\n",
            "  bits-fg <GUID>\n",
            "  bits-suspend <GUID>\n",
            "  bits-resume <GUID>\n",
            "  bits-complete <GUID>\n",
            "  bits-cancel <GUID> ...\n"
        ),
        EXE_NAME
    )
}

fn entry() -> Result {
    let args: Vec<_> = env::args_os().collect();

    let mut client = BitsClient::new(
        OsString::from("bits_client test"),
        OsString::from("C:\\ProgramData"),
    )?;

    if args.len() < 2 {
        eprintln!("{}", usage());
        bail!("not enough arguments");
    }

    let cmd = &*args[1].to_string_lossy();
    let cmd_args = &args[2..];

    match cmd {
        // command line client for testing
        "bits-start" if cmd_args.len() == 2 => bits_start(
            Arc::new(Mutex::new(client)),
            cmd_args[0].clone(),
            cmd_args[1].clone(),
            BitsProxyUsage::Preconfig,
        ),
        "bits-monitor" if cmd_args.len() == 1 => {
            bits_monitor(Arc::new(Mutex::new(client)), &cmd_args[0])
        }
        // TODO: some way of testing set update interval
        "bits-bg" if cmd_args.len() == 1 => bits_bg(&mut client, &cmd_args[0]),
        "bits-fg" if cmd_args.len() == 1 => bits_fg(&mut client, &cmd_args[0]),
        "bits-suspend" if cmd_args.len() == 1 => bits_suspend(&mut client, &cmd_args[0]),
        "bits-resume" if cmd_args.len() == 1 => bits_resume(&mut client, &cmd_args[0]),
        "bits-complete" if cmd_args.len() == 1 => bits_complete(&mut client, &cmd_args[0]),
        "bits-cancel" if cmd_args.len() >= 1 => {
            for guid in cmd_args {
                bits_cancel(&mut client, guid)?;
            }
            Ok(())
        }
        _ => {
            eprintln!("{}", usage());
            bail!("usage error");
        }
    }
}

fn bits_start(
    client: Arc<Mutex<BitsClient>>,
    url: OsString,
    save_path: OsString,
    proxy_usage: BitsProxyUsage,
) -> Result {
    //let interval = 10 * 60 * 1000;
    let no_progress_timeout_secs = 60;
    let interval = 1000;

    let result = client.lock().unwrap().start_job(
        url,
        save_path,
        proxy_usage,
        no_progress_timeout_secs,
        interval,
    )?;

    match result {
        Ok((r, monitor_client)) => {
            println!("start success, guid = {}", r.guid);
            client
                .lock()
                .unwrap()
                .set_update_interval(r.guid.clone(), interval)?
                .unwrap();
            monitor_loop(client, monitor_client, r.guid.clone(), interval)?;
            Ok(())
        }
        Err(e) => {
            let _ = e.clone();
            bail!("error from server {}", e)
        }
    }
}

fn bits_monitor(client: Arc<Mutex<BitsClient>>, guid: &OsStr) -> Result {
    let guid = Guid::from_str(&guid.to_string_lossy())?;
    let result = client.lock().unwrap().monitor_job(guid.clone(), 1000)?;
    match result {
        Ok(monitor_client) => {
            println!("monitor success");
            monitor_loop(client, monitor_client, guid, 1000)?;
            Ok(())
        }
        Err(e) => bail!("error from server {}", e),
    }
}

fn _check_client_send()
where
    BitsClient: Send,
{
}
fn _check_monitor_send()
where
    BitsMonitorClient: Send,
{
}

fn monitor_loop(
    _client: Arc<Mutex<BitsClient>>,
    mut monitor_client: BitsMonitorClient,
    _guid: Guid,
    wait_millis: u32,
) -> Result {
    /*
    // Commented out to avoid vendoring ctrlc.
    // This could also possibly be done with `exclude` in the mozilla-central `Cargo.toml`.
    let client_for_handler = _client.clone();
    ctrlc::set_handler(move || {
        eprintln!("Ctrl-C!");
        let _ = client_for_handler.lock().unwrap().stop_update(_guid.clone());
    })
    .expect("Error setting Ctrl-C handler");
    */

    loop {
        let status = monitor_client.get_status(wait_millis * 10)??;

        println!("{:?} {:?}", BitsJobState::from(status.state), status);

        //println!("{}", job.get_first_file()?.get_remote_name()?.into_string().unwrap());
        let transfer_completion_time = if let Some(ft) = status.times.transfer_completion {
            format!("Some({})", ft.to_system_time_utc()?)
        } else {
            String::from("None")
        };
        println!(
            "creation: {}, modification: {}, transfer completion: {}",
            status.times.creation.to_system_time_utc()?,
            status.times.modification.to_system_time_utc()?,
            transfer_completion_time
        );

        match BitsJobState::from(status.state) {
            BitsJobState::Connecting
            | BitsJobState::Transferring
            | BitsJobState::TransientError => {}
            _ => break,
        }
    }
    println!("monitor loop ending");
    println!("sleeping...");
    std::thread::sleep(std::time::Duration::from_secs(1));

    Ok(())
}

fn bits_bg(client: &mut BitsClient, guid: &OsStr) -> Result {
    bits_set_priority(client, guid, false)
}

fn bits_fg(client: &mut BitsClient, guid: &OsStr) -> Result {
    bits_set_priority(client, guid, true)
}

fn bits_set_priority(client: &mut BitsClient, guid: &OsStr, foreground: bool) -> Result {
    let guid = Guid::from_str(&guid.to_string_lossy())?;
    match client.set_job_priority(guid, foreground)? {
        Ok(()) => Ok(()),
        Err(e) => bail!("error from server {}", e),
    }
}

fn bits_suspend(client: &mut BitsClient, guid: &OsStr) -> Result {
    let guid = Guid::from_str(&guid.to_string_lossy())?;
    match client.suspend_job(guid)? {
        Ok(()) => Ok(()),
        Err(e) => bail!("error from server {}", e),
    }
}

fn bits_resume(client: &mut BitsClient, guid: &OsStr) -> Result {
    let guid = Guid::from_str(&guid.to_string_lossy())?;
    match client.resume_job(guid)? {
        Ok(()) => Ok(()),
        Err(e) => bail!("error from server {}", e),
    }
}

fn bits_complete(client: &mut BitsClient, guid: &OsStr) -> Result {
    let guid = Guid::from_str(&guid.to_string_lossy())?;
    match client.complete_job(guid)? {
        Ok(()) => Ok(()),
        Err(e) => bail!("error from server {}", e),
    }
}

fn bits_cancel(client: &mut BitsClient, guid: &OsStr) -> Result {
    let guid = Guid::from_str(&guid.to_string_lossy())?;
    match client.cancel_job(guid)? {
        Ok(()) => Ok(()),
        Err(e) => bail!("error from server {}", e),
    }
}

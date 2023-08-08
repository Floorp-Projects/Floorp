/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use authenticator::{
    authenticatorservice::AuthenticatorService, errors::AuthenticatorError,
    statecallback::StateCallback, InteractiveRequest, Pin, ResetResult, StatusUpdate,
};
use getopts::Options;
use log::debug;
use std::{env, io, thread};
use std::{
    io::Write,
    sync::mpsc::{channel, Receiver, RecvError},
};

fn print_usage(program: &str, opts: Options) {
    let brief = format!("Usage: {program} [options]");
    print!("{}", opts.usage(&brief));
}

fn interactive_status_callback(status_rx: Receiver<StatusUpdate>) {
    let mut num_of_devices = 0;
    loop {
        match status_rx.recv() {
            Ok(StatusUpdate::InteractiveManagement((tx, dev_info, auth_info))) => {
                debug!(
                    "STATUS: interactive management: {:#}, {:#?}",
                    dev_info, auth_info
                );
                println!("Device info {:#}", dev_info);
                let mut change_pin = false;
                if let Some(info) = auth_info {
                    println!("Authenticator Info {:#?}", info);
                    println!();
                    println!("What do you wish to do?");

                    let mut choices = vec!["0", "1"];
                    println!("(0) Quit");
                    println!("(1) Reset token");
                    match info.options.client_pin {
                        None => {}
                        Some(true) => {
                            println!("(2) Change PIN");
                            choices.push("2");
                            change_pin = true;
                        }
                        Some(false) => {
                            println!("(2) Set PIN");
                            choices.push("2");
                        }
                    }

                    let mut input = String::new();
                    while !choices.contains(&input.trim()) {
                        input.clear();
                        print!("Your choice: ");
                        io::stdout()
                            .lock()
                            .flush()
                            .expect("Failed to flush stdout!");
                        io::stdin()
                            .read_line(&mut input)
                            .expect("error: unable to read user input");
                    }
                    input = input.trim().to_string();

                    match input.as_str() {
                        "0" => {
                            return;
                        }
                        "1" => {
                            tx.send(InteractiveRequest::Reset)
                                .expect("Failed to send Reset request.");
                        }
                        "2" => {
                            let raw_new_pin = rpassword::prompt_password_stderr("Enter new PIN: ")
                                .expect("Failed to read PIN");
                            let new_pin = Pin::new(&raw_new_pin);
                            if change_pin {
                                let raw_curr_pin =
                                    rpassword::prompt_password_stderr("Enter current PIN: ")
                                        .expect("Failed to read PIN");
                                let curr_pin = Pin::new(&raw_curr_pin);
                                tx.send(InteractiveRequest::ChangePIN(curr_pin, new_pin))
                                    .expect("Failed to send PIN-change request");
                            } else {
                                tx.send(InteractiveRequest::SetPIN(new_pin))
                                    .expect("Failed to send PIN-set request");
                            }
                            return;
                        }
                        _ => {
                            panic!("Can't happen");
                        }
                    }
                } else {
                    println!("Device only supports CTAP1 and can't be managed.");
                }
            }
            Ok(StatusUpdate::DeviceAvailable { dev_info }) => {
                num_of_devices += 1;
                debug!(
                    "STATUS: New device #{} available: {}",
                    num_of_devices, dev_info
                );
            }
            Ok(StatusUpdate::DeviceUnavailable { dev_info }) => {
                num_of_devices -= 1;
                if num_of_devices <= 0 {
                    println!("No more devices left. Please plug in a device!");
                }
                debug!("STATUS: Device became unavailable: {}", dev_info)
            }
            Ok(StatusUpdate::Success { dev_info }) => {
                println!("STATUS: success using device: {}", dev_info);
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                println!("STATUS: Please select a device by touching one of them.");
            }
            Ok(StatusUpdate::DeviceSelected(_dev_info)) => {}
            Ok(StatusUpdate::PresenceRequired) => {
                println!("STATUS: waiting for user presence");
            }
            Ok(StatusUpdate::PinUvError(..)) => {
                println!("STATUS: Pin Error!");
            }
            Err(RecvError) => {
                println!("STATUS: end");
                return;
            }
        }
    }
}

fn main() {
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    let program = args[0].clone();

    let mut opts = Options::new();
    opts.optflag("x", "no-u2f-usb-hid", "do not enable u2f-usb-hid platforms");
    opts.optflag("h", "help", "print this help menu").optopt(
        "t",
        "timeout",
        "timeout in seconds",
        "SEC",
    );
    opts.optflag("h", "help", "print this help menu");
    let matches = match opts.parse(&args[1..]) {
        Ok(m) => m,
        Err(f) => panic!("{}", f.to_string()),
    };
    if matches.opt_present("help") {
        print_usage(&program, opts);
        return;
    }

    let mut manager =
        AuthenticatorService::new().expect("The auth service should initialize safely");

    if !matches.opt_present("no-u2f-usb-hid") {
        manager.add_u2f_usb_hid_platform_transports();
    }

    let timeout_ms = match matches.opt_get_default::<u64>("timeout", 120) {
        Ok(timeout_s) => {
            println!("Using {}s as the timeout", &timeout_s);
            timeout_s * 1_000
        }
        Err(e) => {
            println!("{e}");
            print_usage(&program, opts);
            return;
        }
    };

    let (status_tx, status_rx) = channel::<StatusUpdate>();
    thread::spawn(move || interactive_status_callback(status_rx));

    let (manage_tx, manage_rx) = channel();
    let state_callback =
        StateCallback::<Result<ResetResult, AuthenticatorError>>::new(Box::new(move |rv| {
            manage_tx.send(rv).unwrap();
        }));

    match manager.manage(timeout_ms, status_tx, state_callback) {
        Ok(_) => {
            debug!("Started management")
        }
        Err(e) => {
            println!("Error! Failed to start interactive management: {:?}", e)
        }
    }
    let manage_result = manage_rx
        .recv()
        .expect("Problem receiving, unable to continue");
    match manage_result {
        Ok(_) => println!("Success!"),
        Err(e) => println!("Error! {:?}", e),
    };
    println!("Done");
}

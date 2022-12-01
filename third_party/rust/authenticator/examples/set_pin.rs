/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use authenticator::{
    authenticatorservice::{AuthenticatorService, CtapVersion},
    statecallback::StateCallback,
    Pin, PinError, StatusUpdate,
};
use getopts::Options;
use std::sync::mpsc::{channel, RecvError};
use std::{env, thread};

fn print_usage(program: &str, opts: Options) {
    let brief = format!("Usage: {} [options]", program);
    print!("{}", opts.usage(&brief));
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

    let mut manager = AuthenticatorService::new(CtapVersion::CTAP2)
        .expect("The auth service should initialize safely");

    if !matches.opt_present("no-u2f-usb-hid") {
        manager.add_u2f_usb_hid_platform_transports();
    }

    let timeout_ms = match matches.opt_get_default::<u64>("timeout", 25) {
        Ok(timeout_s) => {
            println!("Using {}s as the timeout", &timeout_s);
            timeout_s * 1_000
        }
        Err(e) => {
            println!("{}", e);
            print_usage(&program, opts);
            return;
        }
    };

    let new_pin = rpassword::prompt_password_stderr("Enter new PIN: ").expect("Failed to read PIN");
    let repeat_new_pin =
        rpassword::prompt_password_stderr("Enter it again: ").expect("Failed to read PIN");
    if new_pin != repeat_new_pin {
        println!("PINs did not match!");
        return;
    }

    let (status_tx, status_rx) = channel::<StatusUpdate>();
    thread::spawn(move || loop {
        match status_rx.recv() {
            Ok(StatusUpdate::DeviceAvailable { dev_info }) => {
                println!("STATUS: device available: {}", dev_info)
            }
            Ok(StatusUpdate::DeviceUnavailable { dev_info }) => {
                println!("STATUS: device unavailable: {}", dev_info)
            }
            Ok(StatusUpdate::Success { dev_info }) => {
                println!("STATUS: success using device: {}", dev_info);
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                println!("STATUS: Please select a device by touching one of them.");
            }
            Ok(StatusUpdate::DeviceSelected(dev_info)) => {
                println!("STATUS: Continuing with device: {}", dev_info);
            }
            Ok(StatusUpdate::PinError(error, sender)) => match error {
                PinError::PinRequired => {
                    let raw_pin = rpassword::prompt_password_stderr("Enter current PIN: ")
                        .expect("Failed to read PIN");
                    sender.send(Pin::new(&raw_pin)).expect("Failed to send PIN");
                    continue;
                }
                PinError::InvalidPin(attempts) => {
                    println!(
                        "Wrong PIN! {}",
                        attempts.map_or(format!("Try again."), |a| format!(
                            "You have {} attempts left.",
                            a
                        ))
                    );
                    let raw_pin = rpassword::prompt_password_stderr("Enter current PIN: ")
                        .expect("Failed to read PIN");
                    sender.send(Pin::new(&raw_pin)).expect("Failed to send PIN");
                    continue;
                }
                PinError::PinAuthBlocked => {
                    panic!("Too many failed attempts in one row. Your device has been temporarily blocked. Please unplug it and plug in again.")
                }
                PinError::PinBlocked => {
                    panic!("Too many failed attempts. Your device has been blocked. Reset it.")
                }
                e => {
                    panic!("Unexpected error: {:?}", e)
                }
            },
            Err(RecvError) => {
                println!("STATUS: end");
                return;
            }
        }
    });

    let (reset_tx, reset_rx) = channel();
    let rs_tx = reset_tx.clone();
    let callback = StateCallback::new(Box::new(move |rv| {
        let _ = rs_tx.send(rv);
    }));

    if let Err(e) = manager.set_pin(timeout_ms, Pin::new(&new_pin), status_tx.clone(), callback) {
        panic!("Couldn't call set_pin: {:?}", e);
    };

    let reset_result = reset_rx
        .recv()
        .expect("Problem receiving, unable to continue");
    match reset_result {
        Ok(()) => {
            println!("PIN successfully set!");
        }
        Err(e) => panic!("Setting PIN failed: {:?}", e),
    };
}

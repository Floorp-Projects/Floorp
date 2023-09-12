/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use authenticator::{
    authenticatorservice::AuthenticatorService,
    ctap2::commands::StatusCode,
    errors::{AuthenticatorError, CommandError, HIDError},
    statecallback::StateCallback,
    StatusUpdate,
};
use getopts::Options;
use std::env;
use std::sync::mpsc::{channel, RecvError};

fn print_usage(program: &str, opts: Options) {
    let brief = format!("Usage: {program} [options]");
    print!("{}", opts.usage(&brief));
}

fn main() {
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    let program = args[0].clone();

    let mut opts = Options::new();
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
    manager.add_u2f_usb_hid_platform_transports();

    let timeout_ms = match matches.opt_get_default::<u64>("timeout", 25) {
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

    println!(
        "NOTE: Please unplug all devices, type in 'yes' and plug in the device that should be reset."
    );
    loop {
        let mut s = String::new();
        println!("ATTENTION: Resetting a device will wipe all credentials! Do you wish to continue? [yes/N]");
        std::io::stdin()
            .read_line(&mut s)
            .expect("Did not enter a correct string");
        let trimmed = s.trim();
        if trimmed.is_empty() || trimmed == "N" || trimmed == "n" {
            println!("Exiting without reset.");
            return;
        }
        if trimmed == "y" {
            println!("Please type in the whole word 'yes'");
            continue;
        }
        if trimmed == "yes" {
            break;
        }
    }

    let (status_tx, status_rx) = channel::<StatusUpdate>();
    let (reset_tx, reset_rx) = channel();
    let rs_tx = reset_tx;
    let callback = StateCallback::new(Box::new(move |rv| {
        let _ = rs_tx.send(rv);
    }));

    if let Err(e) = manager.reset(timeout_ms, status_tx, callback) {
        panic!("Couldn't register: {:?}", e);
    };

    loop {
        match status_rx.recv() {
            Ok(StatusUpdate::SelectDeviceNotice) => {
                println!("ERROR: Please unplug all other tokens that should not be reset!");
                // Needed to give the tokens enough time to start blinking
                // otherwise we may cancel pre-maturely and this binary will hang
                std::thread::sleep(std::time::Duration::from_millis(200));
                manager.cancel().unwrap();
                return;
            }
            Ok(StatusUpdate::PresenceRequired) => {
                println!("STATUS: waiting for user presence");
                break;
            }
            Ok(StatusUpdate::PinUvError(..)) => panic!("Reset should never ask for a PIN!"),
            Ok(_) => { /* Ignore all other updates */ }
            Err(RecvError) => {
                println!("RecvError");
                return;
            }
        }
    }

    let reset_result = reset_rx
        .recv()
        .expect("Problem receiving, unable to continue");
    match reset_result {
        Ok(()) => {
            println!("Token successfully reset!");
        }
        Err(AuthenticatorError::HIDError(HIDError::Command(CommandError::StatusCode(
            StatusCode::NotAllowed,
            _,
        )))) => {
            println!("Resetting is only allowed within the first 10 seconds after powering up.");
            println!("Please unplug your device, plug it back in and try again.");
        }
        Err(e) => panic!("Reset failed: {:?}", e),
    };
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use authenticator::{
    authenticatorservice::AuthenticatorService, statecallback::StateCallback,
    AuthenticatorTransports, KeyHandle, RegisterFlags, SignFlags, StatusUpdate,
};
use getopts::Options;
use sha2::{Digest, Sha256};
use std::sync::mpsc::{channel, RecvError};
use std::{env, io, thread};

fn u2f_get_key_handle_from_register_response(register_response: &[u8]) -> io::Result<Vec<u8>> {
    if register_response[0] != 0x05 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Reserved byte not set correctly",
        ));
    }

    let key_handle_len = register_response[66] as usize;
    let mut public_key = register_response.to_owned();
    let mut key_handle = public_key.split_off(67);
    let _attestation = key_handle.split_off(key_handle_len);

    Ok(key_handle)
}

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
    #[cfg(feature = "webdriver")]
    opts.optflag("w", "webdriver", "enable WebDriver virtual bus");

    opts.optflag("h", "help", "print this help menu").optopt(
        "t",
        "timeout",
        "timeout in seconds",
        "SEC",
    );

    opts.optflag("h", "help", "print this help menu");
    let matches = match opts.parse(&args[1..]) {
        Ok(m) => m,
        Err(f) => panic!(f.to_string()),
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

    #[cfg(feature = "webdriver")]
    {
        if matches.opt_present("webdriver") {
            manager.add_webdriver_virtual_bus();
        }
    }

    let timeout_ms = match matches.opt_get_default::<u64>("timeout", 15) {
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

    println!("Asking a security key to register now...");
    let challenge_str = format!(
        "{}{}",
        r#"{"challenge": "1vQ9mxionq0ngCnjD-wTsv1zUSrGRtFqG2xP09SbZ70","#,
        r#" "version": "U2F_V2", "appId": "http://demo.yubico.com"}"#
    );
    let mut challenge = Sha256::default();
    challenge.input(challenge_str.as_bytes());
    let chall_bytes = challenge.result().to_vec();

    let mut application = Sha256::default();
    application.input(b"http://demo.yubico.com");
    let app_bytes = application.result().to_vec();

    let flags = RegisterFlags::empty();

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
            Err(RecvError) => {
                println!("STATUS: end");
                return;
            }
        }
    });

    let (register_tx, register_rx) = channel();
    let callback = StateCallback::new(Box::new(move |rv| {
        register_tx.send(rv).unwrap();
    }));

    manager
        .register(
            flags,
            timeout_ms,
            chall_bytes.clone(),
            app_bytes.clone(),
            vec![],
            status_tx.clone(),
            callback,
        )
        .expect("Couldn't register");

    let register_result = register_rx
        .recv()
        .expect("Problem receiving, unable to continue");
    let (register_data, device_info) = register_result.expect("Registration failed");

    println!("Register result: {}", base64::encode(&register_data));
    println!("Device info: {}", &device_info);
    println!("Asking a security key to sign now, with the data from the register...");
    let credential = u2f_get_key_handle_from_register_response(&register_data).unwrap();
    let key_handle = KeyHandle {
        credential,
        transports: AuthenticatorTransports::empty(),
    };

    let flags = SignFlags::empty();
    let (sign_tx, sign_rx) = channel();

    let callback = StateCallback::new(Box::new(move |rv| {
        sign_tx.send(rv).unwrap();
    }));

    if let Err(e) = manager.sign(
        flags,
        timeout_ms,
        chall_bytes,
        vec![app_bytes],
        vec![key_handle],
        status_tx,
        callback,
    ) {
        panic!("Couldn't register: {:?}", e);
    }

    let sign_result = sign_rx
        .recv()
        .expect("Problem receiving, unable to continue");
    let (_, handle_used, sign_data, device_info) = sign_result.expect("Sign failed");

    println!("Sign result: {}", base64::encode(&sign_data));
    println!("Key handle used: {}", base64::encode(&handle_used));
    println!("Device info: {}", &device_info);
    println!("Done.");
}

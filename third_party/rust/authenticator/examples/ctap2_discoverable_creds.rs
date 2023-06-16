/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use authenticator::{
    authenticatorservice::{AuthenticatorService, RegisterArgs, SignArgs},
    ctap2::server::{
        PublicKeyCredentialDescriptor, PublicKeyCredentialParameters, RelyingParty,
        ResidentKeyRequirement, Transport, User, UserVerificationRequirement,
    },
    statecallback::StateCallback,
    COSEAlgorithm, Pin, RegisterResult, SignResult, StatusPinUv, StatusUpdate,
};
use getopts::Options;
use sha2::{Digest, Sha256};
use std::sync::mpsc::{channel, RecvError};
use std::{env, thread};

fn print_usage(program: &str, opts: Options) {
    println!("------------------------------------------------------------------------");
    println!("This program registers 3x the same origin with different users and");
    println!("requests 'discoverable credentials' for them.");
    println!("After that, we try to log in to that origin and list all credentials found.");
    println!("------------------------------------------------------------------------");
    let brief = format!("Usage: {program} [options]");
    print!("{}", opts.usage(&brief));
}

fn register_user(manager: &mut AuthenticatorService, username: &str, timeout_ms: u64) {
    println!();
    println!("*********************************************************************");
    println!("Asking a security key to register now with user: {username}");
    println!("*********************************************************************");

    println!("Asking a security key to register now...");
    let challenge_str = format!(
        "{}{}{}{}",
        r#"{"challenge": "1vQ9mxionq0ngCnjD-wTsv1zUSrGRtFqG2xP09SbZ70","#,
        r#" "version": "U2F_V2", "appId": "http://example.com", "username": ""#,
        username,
        r#""}"#
    );
    let mut challenge = Sha256::new();
    challenge.update(challenge_str.as_bytes());
    let chall_bytes = challenge.finalize().into();

    let (status_tx, status_rx) = channel::<StatusUpdate>();
    thread::spawn(move || loop {
        match status_rx.recv() {
            Ok(StatusUpdate::InteractiveManagement(..)) => {
                panic!("STATUS: This can't happen when doing non-interactive usage");
            }
            Ok(StatusUpdate::DeviceAvailable { dev_info }) => {
                println!("STATUS: device available: {dev_info}")
            }
            Ok(StatusUpdate::DeviceUnavailable { dev_info }) => {
                println!("STATUS: device unavailable: {dev_info}")
            }
            Ok(StatusUpdate::Success { dev_info }) => {
                println!("STATUS: success using device: {dev_info}");
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                println!("STATUS: Please select a device by touching one of them.");
            }
            Ok(StatusUpdate::DeviceSelected(dev_info)) => {
                println!("STATUS: Continuing with device: {dev_info}");
            }
            Ok(StatusUpdate::PresenceRequired) => {
                println!("STATUS: waiting for user presence");
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinRequired(sender))) => {
                let raw_pin =
                    rpassword::prompt_password_stderr("Enter PIN: ").expect("Failed to read PIN");
                sender.send(Pin::new(&raw_pin)).expect("Failed to send PIN");
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::InvalidPin(sender, attempts))) => {
                println!(
                    "Wrong PIN! {}",
                    attempts.map_or("Try again.".to_string(), |a| format!(
                        "You have {a} attempts left."
                    ))
                );
                let raw_pin =
                    rpassword::prompt_password_stderr("Enter PIN: ").expect("Failed to read PIN");
                sender.send(Pin::new(&raw_pin)).expect("Failed to send PIN");
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinAuthBlocked)) => {
                panic!("Too many failed attempts in one row. Your device has been temporarily blocked. Please unplug it and plug in again.")
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinBlocked)) => {
                panic!("Too many failed attempts. Your device has been blocked. Reset it.")
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::InvalidUv(attempts))) => {
                println!(
                    "Wrong UV! {}",
                    attempts.map_or("Try again.".to_string(), |a| format!(
                        "You have {a} attempts left."
                    ))
                );
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::UvBlocked)) => {
                println!("Too many failed UV-attempts.");
                continue;
            }
            Ok(StatusUpdate::PinUvError(e)) => {
                panic!("Unexpected error: {:?}", e)
            }
            Err(RecvError) => {
                println!("STATUS: end");
                return;
            }
        }
    });

    let user = User {
        id: username.as_bytes().to_vec(),
        icon: None,
        name: Some(username.to_string()),
        display_name: None,
    };
    let origin = "https://example.com".to_string();
    let ctap_args = RegisterArgs {
        client_data_hash: chall_bytes,
        relying_party: RelyingParty {
            id: "example.com".to_string(),
            name: None,
            icon: None,
        },
        origin,
        user,
        pub_cred_params: vec![
            PublicKeyCredentialParameters {
                alg: COSEAlgorithm::ES256,
            },
            PublicKeyCredentialParameters {
                alg: COSEAlgorithm::RS256,
            },
        ],
        exclude_list: vec![PublicKeyCredentialDescriptor {
            id: vec![],
            transports: vec![Transport::USB, Transport::NFC],
        }],
        user_verification_req: UserVerificationRequirement::Required,
        resident_key_req: ResidentKeyRequirement::Required,
        extensions: Default::default(),
        pin: None,
        use_ctap1_fallback: false,
    };

    let attestation_object;
    loop {
        let (register_tx, register_rx) = channel();
        let callback = StateCallback::new(Box::new(move |rv| {
            register_tx.send(rv).unwrap();
        }));

        if let Err(e) = manager.register(timeout_ms, ctap_args, status_tx, callback) {
            panic!("Couldn't register: {:?}", e);
        };

        let register_result = register_rx
            .recv()
            .expect("Problem receiving, unable to continue");
        match register_result {
            Ok(RegisterResult::CTAP1(_, _)) => panic!("Requested CTAP2, but got CTAP1 results!"),
            Ok(RegisterResult::CTAP2(a)) => {
                println!("Ok!");
                attestation_object = a;
                break;
            }
            Err(e) => panic!("Registration failed: {:?}", e),
        };
    }

    println!("Register result: {:?}", &attestation_object);
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

    let timeout_ms = match matches.opt_get_default::<u64>("timeout", 15) {
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

    for username in &["A. User", "A. Nother", "Dr. Who"] {
        register_user(&mut manager, username, timeout_ms)
    }

    println!();
    println!("*********************************************************************");
    println!("Asking a security key to sign now, with the data from the register...");
    println!("*********************************************************************");

    // Discovering creds:
    let allow_list = Vec::new();
    let origin = "https://example.com".to_string();
    let challenge_str = format!(
        "{}{}",
        r#"{"challenge": "1vQ9mxionq0ngCnjD-wTsv1zUSrGRtFqG2xP09SbZ70","#,
        r#" "version": "U2F_V2", "appId": "http://example.com" "#,
    );

    let (status_tx, status_rx) = channel::<StatusUpdate>();
    thread::spawn(move || loop {
        match status_rx.recv() {
            Ok(StatusUpdate::InteractiveManagement(..)) => {
                panic!("STATUS: This can't happen when doing non-interactive usage");
            }
            Ok(StatusUpdate::DeviceAvailable { dev_info }) => {
                println!("STATUS: device available: {dev_info}")
            }
            Ok(StatusUpdate::DeviceUnavailable { dev_info }) => {
                println!("STATUS: device unavailable: {dev_info}")
            }
            Ok(StatusUpdate::Success { dev_info }) => {
                println!("STATUS: success using device: {dev_info}");
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                println!("STATUS: Please select a device by touching one of them.");
            }
            Ok(StatusUpdate::DeviceSelected(dev_info)) => {
                println!("STATUS: Continuing with device: {dev_info}");
            }
            Ok(StatusUpdate::PresenceRequired) => {
                println!("STATUS: waiting for user presence");
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinRequired(sender))) => {
                let raw_pin =
                    rpassword::prompt_password_stderr("Enter PIN: ").expect("Failed to read PIN");
                sender.send(Pin::new(&raw_pin)).expect("Failed to send PIN");
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::InvalidPin(sender, attempts))) => {
                println!(
                    "Wrong PIN! {}",
                    attempts.map_or("Try again.".to_string(), |a| format!(
                        "You have {a} attempts left."
                    ))
                );
                let raw_pin =
                    rpassword::prompt_password_stderr("Enter PIN: ").expect("Failed to read PIN");
                sender.send(Pin::new(&raw_pin)).expect("Failed to send PIN");
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinAuthBlocked)) => {
                panic!("Too many failed attempts in one row. Your device has been temporarily blocked. Please unplug it and plug in again.")
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinBlocked)) => {
                panic!("Too many failed attempts. Your device has been blocked. Reset it.")
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::InvalidUv(attempts))) => {
                println!(
                    "Wrong UV! {}",
                    attempts.map_or("Try again.".to_string(), |a| format!(
                        "You have {a} attempts left."
                    ))
                );
                continue;
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::UvBlocked)) => {
                println!("Too many failed UV-attempts.");
                continue;
            }
            Ok(StatusUpdate::PinUvError(e)) => {
                panic!("Unexpected error: {:?}", e)
            }
            Err(RecvError) => {
                println!("STATUS: end");
                return;
            }
        }
    });

    let mut challenge = Sha256::new();
    challenge.update(challenge_str.as_bytes());
    let chall_bytes = challenge.finalize().into();
    let ctap_args = SignArgs {
        client_data_hash: chall_bytes,
        origin,
        relying_party_id: "example.com".to_string(),
        allow_list,
        user_verification_req: UserVerificationRequirement::Required,
        user_presence_req: true,
        extensions: Default::default(),
        pin: None,
        alternate_rp_id: None,
        use_ctap1_fallback: false,
    };

    loop {
        let (sign_tx, sign_rx) = channel();

        let callback = StateCallback::new(Box::new(move |rv| {
            sign_tx.send(rv).unwrap();
        }));

        if let Err(e) = manager.sign(timeout_ms, ctap_args, status_tx, callback) {
            panic!("Couldn't sign: {:?}", e);
        }

        let sign_result = sign_rx
            .recv()
            .expect("Problem receiving, unable to continue");

        match sign_result {
            Ok(SignResult::CTAP1(..)) => panic!("Requested CTAP2, but got CTAP1 sign results!"),
            Ok(SignResult::CTAP2(assertion_object)) => {
                println!("Assertion Object: {assertion_object:?}");
                println!("-----------------------------------------------------------------");
                println!("Found credentials:");
                println!(
                    "{:?}",
                    assertion_object
                        .0
                        .iter()
                        .map(|x| x.user.clone().unwrap().name.unwrap()) // Unwrapping here, as these shouldn't fail
                        .collect::<Vec<_>>()
                );
                println!("-----------------------------------------------------------------");
                println!("Done.");
                break;
            }
            Err(e) => panic!("Signing failed: {:?}", e),
        }
    }
}

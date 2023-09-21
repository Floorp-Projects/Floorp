/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use authenticator::{
    authenticatorservice::{AuthenticatorService, RegisterArgs, SignArgs},
    crypto::COSEAlgorithm,
    ctap2::server::{
        AuthenticationExtensionsClientInputs, CredentialProtectionPolicy,
        PublicKeyCredentialDescriptor, PublicKeyCredentialParameters,
        PublicKeyCredentialUserEntity, RelyingParty, ResidentKeyRequirement, Transport,
        UserVerificationRequirement,
    },
    statecallback::StateCallback,
    Pin, StatusPinUv, StatusUpdate,
};
use getopts::Options;
use rand::{thread_rng, RngCore};
use std::sync::mpsc::{channel, RecvError};
use std::{env, thread};

fn print_usage(program: &str, opts: Options) {
    let brief = format!("Usage: {program} [options]");
    print!("{}", opts.usage(&brief));
}

fn main() {
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    let program = args[0].clone();

    let rp_id = "example.com".to_string();
    let app_id = "https://fido.example.com/myApp".to_string();

    let mut opts = Options::new();
    opts.optflag("h", "help", "print this help menu").optopt(
        "t",
        "timeout",
        "timeout in seconds",
        "SEC",
    );
    opts.optflag(
        "a",
        "app_id",
        &format!("Using App ID {app_id} from origin 'https://{rp_id}'"),
    );
    opts.optflag("s", "hmac_secret", "With hmac-secret");
    opts.optflag("h", "help", "print this help menu");
    opts.optflag("f", "fallback", "Use CTAP1 fallback implementation");
    let matches = match opts.parse(&args[1..]) {
        Ok(m) => m,
        Err(f) => panic!("{}", f.to_string()),
    };
    if matches.opt_present("help") {
        print_usage(&program, opts);
        return;
    }

    let using_app_id = matches.opt_present("app_id");

    let mut manager =
        AuthenticatorService::new().expect("The auth service should initialize safely");
    manager.add_u2f_usb_hid_platform_transports();

    let fallback = matches.opt_present("fallback");

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

    println!("Asking a security key to register now...");
    let mut chall_bytes = [0u8; 32];
    thread_rng().fill_bytes(&mut chall_bytes);

    let (status_tx, status_rx) = channel::<StatusUpdate>();
    thread::spawn(move || loop {
        match status_rx.recv() {
            Ok(StatusUpdate::InteractiveManagement(..)) => {
                panic!("STATUS: This can't happen when doing non-interactive usage");
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                println!("STATUS: Please select a device by touching one of them.");
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
            Ok(StatusUpdate::SelectResultNotice(_, _)) => {
                panic!("Unexpected select device notice")
            }
            Err(RecvError) => {
                println!("STATUS: end");
                return;
            }
        }
    });

    let user = PublicKeyCredentialUserEntity {
        id: "user_id".as_bytes().to_vec(),
        name: Some("A. User".to_string()),
        display_name: None,
    };
    // If we're testing AppID support, then register with an RP ID that isn't valid for the origin.
    let relying_party = RelyingParty {
        id: if using_app_id {
            app_id.clone()
        } else {
            rp_id.clone()
        },
        name: None,
    };
    let ctap_args = RegisterArgs {
        client_data_hash: chall_bytes,
        relying_party,
        origin: format!("https://{rp_id}"),
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
            id: vec![
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
                0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
                0x1c, 0x1d, 0x1e, 0x1f,
            ],
            transports: vec![Transport::USB, Transport::NFC],
        }],
        user_verification_req: UserVerificationRequirement::Preferred,
        resident_key_req: ResidentKeyRequirement::Discouraged,
        extensions: AuthenticationExtensionsClientInputs {
            cred_props: Some(true),
            hmac_create_secret: Some(matches.opt_present("hmac_secret")),
            min_pin_length: Some(true),
            credential_protection_policy: Some(
                CredentialProtectionPolicy::UserVerificationOptionalWithCredentialIDList,
            ),
            ..Default::default()
        },
        pin: None,
        use_ctap1_fallback: fallback,
    };

    let attestation_object;
    loop {
        let (register_tx, register_rx) = channel();
        let callback = StateCallback::new(Box::new(move |rv| {
            register_tx.send(rv).unwrap();
        }));

        if let Err(e) = manager.register(timeout_ms, ctap_args, status_tx.clone(), callback) {
            panic!("Couldn't register: {:?}", e);
        };

        let register_result = register_rx
            .recv()
            .expect("Problem receiving, unable to continue");
        match register_result {
            Ok(a) => {
                println!("Ok!");
                attestation_object = a;
                break;
            }
            Err(e) => panic!("Registration failed: {:?}", e),
        };
    }

    println!("Register result: {:?}", &attestation_object);

    println!();
    println!("*********************************************************************");
    println!("Asking a security key to sign now, with the data from the register...");
    println!("*********************************************************************");

    let allow_list;
    if let Some(cred_data) = attestation_object.att_obj.auth_data.credential_data {
        allow_list = vec![PublicKeyCredentialDescriptor {
            id: cred_data.credential_id,
            transports: vec![Transport::USB],
        }];
    } else {
        allow_list = Vec::new();
    }

    let ctap_args = SignArgs {
        client_data_hash: chall_bytes,
        origin: format!("https://{rp_id}"),
        relying_party_id: rp_id,
        allow_list,
        user_verification_req: UserVerificationRequirement::Preferred,
        user_presence_req: true,
        extensions: AuthenticationExtensionsClientInputs {
            app_id: using_app_id.then(|| app_id.clone()),
            ..Default::default()
        },
        pin: None,
        use_ctap1_fallback: fallback,
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
            Ok(assertion_object) => {
                println!("Assertion Object: {assertion_object:?}");
                if using_app_id {
                    println!(
                        "Used AppID: {}",
                        assertion_object
                            .extensions
                            .app_id
                            .expect("app id extension should be set")
                    );
                }
                println!("Done.");
                break;
            }
            Err(e) => panic!("Signing failed: {:?}", e),
        }
    }
}

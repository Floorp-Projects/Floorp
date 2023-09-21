/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use authenticator::{
    authenticatorservice::AuthenticatorService,
    ctap2::commands::{
        authenticator_config::{AuthConfigCommand, AuthConfigResult, SetMinPINLength},
        bio_enrollment::EnrollmentInfo,
        credential_management::CredentialList,
        PinUvAuthResult,
    },
    errors::AuthenticatorError,
    statecallback::StateCallback,
    AuthenticatorInfo, BioEnrollmentCmd, BioEnrollmentResult, CredManagementCmd,
    CredentialManagementResult, InteractiveRequest, InteractiveUpdate, ManageResult, Pin,
    StatusPinUv, StatusUpdate,
};
use getopts::Options;
use log::debug;
use std::{env, io, sync::mpsc::Sender, thread};
use std::{
    fmt::Display,
    io::Write,
    sync::mpsc::{channel, Receiver, RecvError},
};

#[derive(Debug, PartialEq, Clone)]
enum PinOperation {
    Set,
    Change,
}

#[derive(Debug, Clone, PartialEq)]
enum ConfigureOperation {
    ToggleAlwaysUV,
    EnableEnterpriseAttestation,
    SetMinPINLength,
}

impl ConfigureOperation {
    fn ask_user(info: &AuthenticatorInfo) -> Self {
        let sub_level = Self::parse_possible_operations(info);
        println!();
        println!("What do you wish to do?");
        let choice = ask_user_choice(&sub_level);
        sub_level[choice].clone()
    }

    fn parse_possible_operations(info: &AuthenticatorInfo) -> Vec<Self> {
        let mut configure_ops = vec![];
        if info.options.authnr_cfg == Some(true) && info.options.always_uv.is_some() {
            configure_ops.push(ConfigureOperation::ToggleAlwaysUV);
        }
        if info.options.authnr_cfg == Some(true) && info.options.set_min_pin_length.is_some() {
            configure_ops.push(ConfigureOperation::SetMinPINLength);
        }
        if info.options.ep.is_some() {
            configure_ops.push(ConfigureOperation::EnableEnterpriseAttestation);
        }
        configure_ops
    }
}

impl Display for ConfigureOperation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ConfigureOperation::ToggleAlwaysUV => write!(f, "Toggle option 'Always UV'"),
            ConfigureOperation::EnableEnterpriseAttestation => {
                write!(f, "Enable Enterprise attestation")
            }
            ConfigureOperation::SetMinPINLength => write!(f, "Set min. PIN length"),
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
enum CredentialsOperation {
    List,
    Delete,
    UpdateUser,
}

impl CredentialsOperation {
    fn ask_user(info: &AuthenticatorInfo, creds: &CredentialList) -> Self {
        let sub_level = Self::parse_possible_operations(info, creds);
        println!();
        println!("What do you wish to do?");
        let choice = ask_user_choice(&sub_level);
        sub_level[choice].clone()
    }

    fn parse_possible_operations(info: &AuthenticatorInfo, creds: &CredentialList) -> Vec<Self> {
        let mut credentials_ops = vec![];
        if info.options.cred_mgmt == Some(true)
            || info.options.credential_mgmt_preview == Some(true)
        {
            credentials_ops.push(CredentialsOperation::List);
        }
        if creds.existing_resident_credentials_count > 0 {
            credentials_ops.push(CredentialsOperation::Delete);
            // FIDO_2_1_PRE devices do not (all?) support UpdateUser.
            // So we require devices to support full 2.1 for this.
            if info
                .versions
                .contains(&authenticator::ctap2::commands::get_info::AuthenticatorVersion::FIDO_2_1)
            {
                credentials_ops.push(CredentialsOperation::UpdateUser);
            }
        }
        credentials_ops
    }
}

impl Display for CredentialsOperation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CredentialsOperation::List => write!(f, "List credentials"),
            CredentialsOperation::Delete => write!(f, "Delete credentials"),
            CredentialsOperation::UpdateUser => write!(f, "Update user info"),
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
enum BioOperation {
    ShowInfo,
    Add,
    List,
    Delete,
    Rename,
}

impl BioOperation {
    fn ask_user(info: &AuthenticatorInfo) -> Self {
        let sub_level = Self::parse_possible_operations(info);
        println!();
        println!("What do you wish to do?");
        let choice = ask_user_choice(&sub_level);
        sub_level[choice].clone()
    }

    fn parse_possible_operations(info: &AuthenticatorInfo) -> Vec<Self> {
        let mut bio_ops = vec![];
        if info.options.bio_enroll.is_some()
            || info.options.user_verification_mgmt_preview.is_some()
        {
            bio_ops.extend([BioOperation::ShowInfo, BioOperation::Add]);
        }
        if info.options.bio_enroll == Some(true)
            || info.options.user_verification_mgmt_preview == Some(true)
        {
            bio_ops.extend([
                BioOperation::Delete,
                BioOperation::List,
                BioOperation::Rename,
            ]);
        }
        bio_ops
    }
}

impl Display for BioOperation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            BioOperation::ShowInfo => write!(f, "Show fingerprint sensor info"),
            BioOperation::List => write!(f, "List enrollments"),
            BioOperation::Add => write!(f, "Add enrollment"),
            BioOperation::Delete => write!(f, "Delete enrollment"),
            BioOperation::Rename => write!(f, "Rename enrollment"),
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
enum TopLevelOperation {
    Quit,
    Reset,
    ShowFullInfo,
    Pin(PinOperation),
    Configure,
    Credentials,
    Bio,
}

impl Display for TopLevelOperation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            TopLevelOperation::Quit => write!(f, "Quit"),
            TopLevelOperation::Reset => write!(f, "Reset"),
            TopLevelOperation::ShowFullInfo => write!(f, "Show full info"),
            TopLevelOperation::Pin(PinOperation::Change) => write!(f, "Change Pin"),
            TopLevelOperation::Pin(PinOperation::Set) => write!(f, "Set Pin"),
            TopLevelOperation::Configure => write!(f, "Configure Authenticator"),
            TopLevelOperation::Credentials => write!(f, "Manage Credentials"),
            TopLevelOperation::Bio => write!(f, "Manage BioEnrollments"),
        }
    }
}

impl TopLevelOperation {
    fn ask_user(info: &AuthenticatorInfo) -> TopLevelOperation {
        let top_level = Self::parse_possible_operations(info);
        println!();
        println!("What do you wish to do?");
        let choice = ask_user_choice(&top_level);
        top_level[choice].clone()
    }

    fn parse_possible_operations(info: &AuthenticatorInfo) -> Vec<TopLevelOperation> {
        let mut ops = vec![
            TopLevelOperation::Quit,
            TopLevelOperation::Reset,
            TopLevelOperation::ShowFullInfo,
        ];

        // PIN-related
        match info.options.client_pin {
            None => {}
            Some(true) => ops.push(TopLevelOperation::Pin(PinOperation::Change)),
            Some(false) => ops.push(TopLevelOperation::Pin(PinOperation::Set)),
        }

        // Authenticator-Configuration
        if info.options.authnr_cfg == Some(true)
            && (info.options.always_uv.is_some()
                || info.options.set_min_pin_length.is_some()
                || info.options.ep.is_some())
        {
            ops.push(TopLevelOperation::Configure);
        }

        // Credential Management
        if info.options.cred_mgmt == Some(true)
            || info.options.credential_mgmt_preview == Some(true)
        {
            ops.push(TopLevelOperation::Credentials);
        }

        // Bio Enrollment
        if info.options.bio_enroll.is_some()
            || info.options.user_verification_mgmt_preview.is_some()
        {
            ops.push(TopLevelOperation::Bio);
        }

        ops
    }
}

fn print_usage(program: &str, opts: Options) {
    let brief = format!("Usage: {program} [options]");
    print!("{}", opts.usage(&brief));
}

fn ask_user_choice<T: Display>(choices: &[T]) -> usize {
    for (idx, op) in choices.iter().enumerate() {
        println!("({idx}) {op}");
    }

    let mut input = String::new();
    loop {
        input.clear();
        print!("Your choice: ");
        io::stdout()
            .lock()
            .flush()
            .expect("Failed to flush stdout!");
        io::stdin()
            .read_line(&mut input)
            .expect("error: unable to read user input");
        if let Ok(idx) = input.trim().parse::<usize>() {
            if idx < choices.len() {
                // Add a newline in case of success for better separation of in/output
                println!();
                return idx;
            }
        }
    }
}

fn handle_authenticator_config(
    res: Option<AuthConfigResult>,
    puat: Option<PinUvAuthResult>,
    auth_info: &mut Option<AuthenticatorInfo>,
    tx: &Sender<InteractiveRequest>,
) {
    if let Some(AuthConfigResult::Success(info)) = res {
        println!("{:#?}", info.options);
        *auth_info = Some(info);
    }
    let choice = ConfigureOperation::ask_user(auth_info.as_ref().unwrap());
    match choice {
        ConfigureOperation::ToggleAlwaysUV => {
            tx.send(InteractiveRequest::ChangeConfig(
                AuthConfigCommand::ToggleAlwaysUv,
                puat,
            ))
            .expect("Failed to send ToggleAlwaysUV request.");
        }
        ConfigureOperation::EnableEnterpriseAttestation => {
            tx.send(InteractiveRequest::ChangeConfig(
                AuthConfigCommand::EnableEnterpriseAttestation,
                puat,
            ))
            .expect("Failed to send EnableEnterpriseAttestation request.");
        }
        ConfigureOperation::SetMinPINLength => {
            let mut length = String::new();
            while length.trim().parse::<u64>().is_err() {
                length.clear();
                print!("New minimum PIN length: ");
                io::stdout()
                    .lock()
                    .flush()
                    .expect("Failed to flush stdout!");
                io::stdin()
                    .read_line(&mut length)
                    .expect("error: unable to read user input");
            }
            let new_length = length.trim().parse::<u64>().unwrap();
            let cmd = SetMinPINLength {
                new_min_pin_length: Some(new_length),
                min_pin_length_rpids: None,
                force_change_pin: None,
            };

            tx.send(InteractiveRequest::ChangeConfig(
                AuthConfigCommand::SetMinPINLength(cmd),
                puat,
            ))
            .expect("Failed to send SetMinPINLength request.");
        }
    }
}

fn handle_credential_management(
    res: Option<CredentialManagementResult>,
    puat: Option<PinUvAuthResult>,
    auth_info: &mut Option<AuthenticatorInfo>,
    tx: &Sender<InteractiveRequest>,
) {
    match res {
        Some(CredentialManagementResult::CredentialList(credlist)) => {
            let mut creds = vec![];
            for rp in &credlist.credential_list {
                for cred in &rp.credentials {
                    creds.push((
                        rp.rp.name.clone(),
                        cred.user.clone(),
                        cred.credential_id.clone(),
                    ));
                }
            }
            let display_creds: Vec<_> = creds
                .iter()
                .map(|(rp, user, id)| format!("{:?} - {:?} - {:?}", rp, user, id))
                .collect();
            for (idx, op) in display_creds.iter().enumerate() {
                println!("({idx}) {op}");
            }

            loop {
                match CredentialsOperation::ask_user(auth_info.as_ref().unwrap(), &credlist) {
                    CredentialsOperation::List => {
                        let mut idx = 0;
                        for rp in credlist.credential_list.iter() {
                            for cred in &rp.credentials {
                                println!("({idx}) - {:?}: {:?}", rp.rp.name, cred);
                                idx += 1;
                            }
                        }
                        continue;
                    }
                    CredentialsOperation::Delete => {
                        let choice = ask_user_choice(&display_creds);
                        tx.send(InteractiveRequest::CredentialManagement(
                            CredManagementCmd::DeleteCredential(creds[choice].2.clone()),
                            puat,
                        ))
                        .expect("Failed to send DeleteCredentials request.");
                        break;
                    }
                    CredentialsOperation::UpdateUser => {
                        let choice = ask_user_choice(&display_creds);
                        // Updating username. Asking for the new one.
                        let mut input = String::new();
                        print!("New username: ");
                        io::stdout()
                            .lock()
                            .flush()
                            .expect("Failed to flush stdout!");
                        io::stdin()
                            .read_line(&mut input)
                            .expect("error: unable to read user input");
                        input = input.trim().to_string();
                        let name = if input.is_empty() { None } else { Some(input) };
                        let mut new_user = creds[choice].1.clone();
                        new_user.name = name;
                        tx.send(InteractiveRequest::CredentialManagement(
                            CredManagementCmd::UpdateUserInformation(
                                creds[choice].2.clone(),
                                new_user,
                            ),
                            puat,
                        ))
                        .expect("Failed to send UpdateUserinformation request.");
                        break;
                    }
                }
            }
        }
        None
        | Some(CredentialManagementResult::DeleteSucess)
        | Some(CredentialManagementResult::UpdateSuccess) => {
            tx.send(InteractiveRequest::CredentialManagement(
                CredManagementCmd::GetCredentials,
                puat,
            ))
            .expect("Failed to send GetCredentials request.");
        }
    }
}

fn ask_user_bio_options(
    biolist: Vec<EnrollmentInfo>,
    puat: Option<PinUvAuthResult>,
    auth_info: &mut Option<AuthenticatorInfo>,
    tx: &Sender<InteractiveRequest>,
) {
    let display_bios: Vec<_> = biolist
        .iter()
        .map(|x| format!("{:?}: {:?}", x.template_friendly_name, x.template_id))
        .collect();
    loop {
        match BioOperation::ask_user(auth_info.as_ref().unwrap()) {
            BioOperation::List => {
                for (idx, bio) in display_bios.iter().enumerate() {
                    println!("({idx}) - {bio}");
                }
                continue;
            }
            BioOperation::ShowInfo => {
                tx.send(InteractiveRequest::BioEnrollment(
                    BioEnrollmentCmd::GetFingerprintSensorInfo,
                    puat,
                ))
                .expect("Failed to send GetFingerprintSensorInfo request.");
                break;
            }
            BioOperation::Delete => {
                let choice = ask_user_choice(&display_bios);
                tx.send(InteractiveRequest::BioEnrollment(
                    BioEnrollmentCmd::DeleteEnrollment(biolist[choice].template_id.clone()),
                    puat,
                ))
                .expect("Failed to send GetEnrollments request.");
                break;
            }
            BioOperation::Rename => {
                let choice = ask_user_choice(&display_bios);
                let chosen_id = biolist[choice].template_id.clone();
                // Updating enrollment name. Asking for the new one.
                let mut input = String::new();
                print!("New name: ");
                io::stdout()
                    .lock()
                    .flush()
                    .expect("Failed to flush stdout!");
                io::stdin()
                    .read_line(&mut input)
                    .expect("error: unable to read user input");
                let name = input.trim().to_string();
                tx.send(InteractiveRequest::BioEnrollment(
                    BioEnrollmentCmd::ChangeName(chosen_id, name),
                    puat,
                ))
                .expect("Failed to send GetEnrollments request.");
                break;
            }
            BioOperation::Add => {
                let mut input = String::new();
                print!(
                "The name of the new bio enrollment (leave empty if you don't want to name it): "
            );
                io::stdout()
                    .lock()
                    .flush()
                    .expect("Failed to flush stdout!");
                io::stdin()
                    .read_line(&mut input)
                    .expect("error: unable to read user input");
                input = input.trim().to_string();
                let name = if input.is_empty() { None } else { Some(input) };
                tx.send(InteractiveRequest::BioEnrollment(
                    BioEnrollmentCmd::StartNewEnrollment(name),
                    puat,
                ))
                .expect("Failed to send StartNewEnrollment request.");
                break;
            }
        }
    }
}

fn handle_bio_enrollments(
    res: Option<BioEnrollmentResult>,
    puat: Option<PinUvAuthResult>,
    auth_info: &mut Option<AuthenticatorInfo>,
    tx: &Sender<InteractiveRequest>,
) {
    match res {
        Some(BioEnrollmentResult::EnrollmentList(biolist)) => {
            ask_user_bio_options(biolist, puat, auth_info, tx);
        }
        None => {
            if BioOperation::parse_possible_operations(auth_info.as_ref().unwrap())
                .contains(&BioOperation::List)
            {
                tx.send(InteractiveRequest::BioEnrollment(
                    BioEnrollmentCmd::GetEnrollments,
                    puat,
                ))
                .expect("Failed to send GetEnrollments request.");
            } else {
                ask_user_bio_options(vec![], puat, auth_info, tx);
            }
        }
        Some(BioEnrollmentResult::UpdateSuccess) => {
            tx.send(InteractiveRequest::BioEnrollment(
                BioEnrollmentCmd::GetEnrollments,
                puat,
            ))
            .expect("Failed to send GetEnrollments request.");
        }
        Some(BioEnrollmentResult::DeleteSuccess(info)) => {
            *auth_info = Some(info.clone());
            if BioOperation::parse_possible_operations(&info).contains(&BioOperation::List) {
                tx.send(InteractiveRequest::BioEnrollment(
                    BioEnrollmentCmd::GetEnrollments,
                    puat,
                ))
                .expect("Failed to send GetEnrollments request.");
            } else {
                ask_user_bio_options(vec![], puat, auth_info, tx);
            }
        }
        Some(BioEnrollmentResult::AddSuccess(info)) => {
            *auth_info = Some(info);
            tx.send(InteractiveRequest::BioEnrollment(
                BioEnrollmentCmd::GetEnrollments,
                puat,
            ))
            .expect("Failed to send GetEnrollments request.");
        }
        Some(BioEnrollmentResult::FingerprintSensorInfo(info)) => {
            println!("{info:#?}");
            if BioOperation::parse_possible_operations(auth_info.as_ref().unwrap())
                .contains(&BioOperation::List)
            {
                tx.send(InteractiveRequest::BioEnrollment(
                    BioEnrollmentCmd::GetEnrollments,
                    puat,
                ))
                .expect("Failed to send GetEnrollments request.");
            } else {
                ask_user_bio_options(vec![], puat, auth_info, tx);
            }
        }
        Some(BioEnrollmentResult::SampleStatus(last_sample_status, remaining_samples)) => {
            println!("Last sample status: {last_sample_status:?}, remaining samples: {remaining_samples:?}");
        }
    }
}

fn interactive_status_callback(status_rx: Receiver<StatusUpdate>) {
    let mut tx = None;
    let mut auth_info = None;
    loop {
        match status_rx.recv() {
            Ok(StatusUpdate::InteractiveManagement(InteractiveUpdate::StartManagement((
                tx_new,
                auth_info_new,
            )))) => {
                let info = match auth_info_new {
                    Some(info) => info,
                    None => {
                        println!("Device only supports CTAP1 and can't be managed.");
                        return;
                    }
                };
                auth_info = Some(info.clone());
                tx = Some(tx_new);
                match TopLevelOperation::ask_user(&info) {
                    TopLevelOperation::Quit => {
                        tx.as_ref()
                            .unwrap()
                            .send(InteractiveRequest::Quit)
                            .expect("Failed to send PIN-set request");
                    }
                    TopLevelOperation::Reset => tx
                        .as_ref()
                        .unwrap()
                        .send(InteractiveRequest::Reset)
                        .expect("Failed to send Reset request."),
                    TopLevelOperation::ShowFullInfo => {
                        println!("Authenticator Info {:#?}", info);
                        return;
                    }
                    TopLevelOperation::Pin(op) => {
                        let raw_new_pin = rpassword::prompt_password_stderr("Enter new PIN: ")
                            .expect("Failed to read PIN");
                        let new_pin = Pin::new(&raw_new_pin);
                        if op == PinOperation::Change {
                            let raw_curr_pin =
                                rpassword::prompt_password_stderr("Enter current PIN: ")
                                    .expect("Failed to read PIN");
                            let curr_pin = Pin::new(&raw_curr_pin);
                            tx.as_ref()
                                .unwrap()
                                .send(InteractiveRequest::ChangePIN(curr_pin, new_pin))
                                .expect("Failed to send PIN-change request");
                        } else {
                            tx.as_ref()
                                .unwrap()
                                .send(InteractiveRequest::SetPIN(new_pin))
                                .expect("Failed to send PIN-set request");
                        }
                    }
                    TopLevelOperation::Configure => handle_authenticator_config(
                        None,
                        None,
                        &mut auth_info,
                        tx.as_ref().unwrap(),
                    ),
                    TopLevelOperation::Credentials => handle_credential_management(
                        None,
                        None,
                        &mut auth_info,
                        tx.as_ref().unwrap(),
                    ),
                    TopLevelOperation::Bio => {
                        handle_bio_enrollments(None, None, &mut auth_info, tx.as_ref().unwrap())
                    }
                }
                continue;
            }
            Ok(StatusUpdate::InteractiveManagement(InteractiveUpdate::AuthConfigUpdate((
                cfg_result,
                puat_res,
            )))) => {
                handle_authenticator_config(
                    Some(cfg_result),
                    puat_res,
                    &mut auth_info,
                    tx.as_ref().unwrap(),
                );
                continue;
            }
            Ok(StatusUpdate::InteractiveManagement(
                InteractiveUpdate::CredentialManagementUpdate((cfg_result, puat_res)),
            )) => {
                handle_credential_management(
                    Some(cfg_result),
                    puat_res,
                    &mut auth_info,
                    tx.as_ref().unwrap(),
                );
                continue;
            }
            Ok(StatusUpdate::InteractiveManagement(InteractiveUpdate::BioEnrollmentUpdate((
                bio_res,
                puat_res,
            )))) => {
                handle_bio_enrollments(
                    Some(bio_res),
                    puat_res,
                    &mut auth_info,
                    tx.as_ref().unwrap(),
                );
                continue;
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                println!("STATUS: Please select a device by touching one of them.");
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
            Ok(StatusUpdate::PresenceRequired) => {
                println!("Please touch your device!");
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
    }
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
        StateCallback::<Result<ManageResult, AuthenticatorError>>::new(Box::new(move |rv| {
            manage_tx.send(rv).unwrap();
        }));

    match manager.manage(timeout_ms, status_tx, state_callback) {
        Ok(_) => {
            debug!("Started management");
        }
        Err(e) => {
            println!("Error! Failed to start interactive management: {:?}", e);
            return;
        }
    }
    let manage_result = manage_rx
        .recv()
        .expect("Problem receiving, unable to continue");
    match manage_result {
        Ok(r) => {
            println!("Success! Result = {r:?}");
        }
        Err(e) => {
            println!("Error! {:?}", e);
        }
    };
    println!("Done");
}

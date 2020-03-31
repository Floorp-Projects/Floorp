/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate cstr;

use glean_preview::metrics::PingType;
use glean_preview::{ClientInfoMetrics, Configuration};
use log::error;
use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::{nsAString, nsString};
use std::ffi::CString;
use std::fs::{self, File};
use std::io::{self, BufRead, BufReader, Error, Write};
use std::path::{Path, PathBuf};
use std::thread::JoinHandle;
use std::{thread, time};
use xpcom::interfaces::{nsIFile, nsIProcess};
use xpcom::RefPtr;

#[no_mangle]
pub unsafe extern "C" fn fog_init(data_dir: &nsAString, pingsender_path: &nsAString) -> nsresult {
    let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");

    let pingsender_path = pingsender_path.to_string();
    let data_dir = data_dir.to_string();
    if thread::Builder::new()
        .name("fogotype_init".to_owned())
        .spawn(move || {
            let cfg = Configuration {
                data_path: data_dir.clone(),
                application_id: "org.mozilla.fogotype".into(),
                upload_enabled,
                max_events: None,
                delay_ping_lifetime_io: false, // We will want this eventually.
                channel: Some("nightly".into()),
            };

            // TODO: Build our own ClientInfoMetrics instead of using unknown().
            if let Err(e) = glean_preview::initialize(cfg, ClientInfoMetrics::unknown()) {
                error!("Failed to init glean_preview due to {:?}", e);
                return;
            }

            let mut data_path = PathBuf::from(data_dir);
            data_path.push("pending_pings");

            // We ignore the returned JoinHandle for the nonce.
            // The detached thread will live until this process (the main process) dies.
            if let Err(e) = prototype_ping_init(data_path, pingsender_path) {
                error!("Failed to init fogtotype prototype ping due to {:?}", e);
            }
        })
        .is_err()
    {
        return NS_ERROR_FAILURE;
    }

    NS_OK
}

fn prototype_ping_init(
    ping_dir: PathBuf,
    pingsender_path: String,
) -> Result<JoinHandle<()>, Error> {
    thread::Builder::new()
        .name("fogotype_ping".to_owned())
        .spawn(move || {
            let prototype_ping = PingType::new("prototype", true, true);
            glean_preview::register_ping_type(&prototype_ping);

            let an_hour = time::Duration::from_secs(60 * 60);
            loop {
                thread::sleep(an_hour);
                let upload_enabled =
                    static_prefs::pref!("datareporting.healthreport.uploadEnabled");
                glean_preview::set_upload_enabled(upload_enabled);
                if !upload_enabled {
                    continue;
                }
                prototype_ping.submit();
                if let Err(e) = send_all_pings(&ping_dir, &pingsender_path) {
                    error!("Failed to send all pings due to {:?}", e);
                }
            }
        })
}

fn send_all_pings(
    ping_dir: &Path,
    pingsender_path: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    assert!(ping_dir.is_dir());
    // This will be a multi-step process:
    // 1. Ensure we have an (empty) subdirectory in ping_dir called "telemetry" we can work within.
    // 2. Split the endpoint out of the glean-format ping in ping_dir, writing the rest to telemetry/.
    // 3. Invoke pingsender{.exe} for the ping in telemetry to the endpoint from the glean-format ping file we most timely ripp'd.
    // 4. Return to 2 while pings remain in ping_dir

    let telemetry_dir = ping_dir.join("telemetry");
    let _ = fs::remove_dir_all(&telemetry_dir);
    fs::create_dir(&telemetry_dir)?;

    for entry in fs::read_dir(ping_dir)? {
        let entry = entry?;
        let path = entry.path();
        if !path.is_file() {
            continue;
        }
        // Do the things.
        let file = File::open(&path)?;
        let reader = BufReader::new(file);
        let lines: Vec<String> = reader.lines().filter_map(io::Result::ok).collect();

        // Sanity check: Glean SDK ping file format is two lines.
        // First line is the path of the ingestion endpoint.
        // Second line is the payload.
        if lines.len() != 2 {
            // Doesn't look like a Glean SDK-format ping. Get rid of it.
            fs::remove_file(path)?;
            continue;
        }

        let telemetry_ping_path =
            telemetry_dir.join(path.file_name().ok_or("ping dir file name invalid")?);
        let mut telemetry_ping_file = File::create(&telemetry_ping_path)?;
        write!(telemetry_ping_file, "{}", lines[1])?;

        fs::remove_file(path)?;

        let pingsender_file: RefPtr<nsIFile> =
            xpcom::create_instance(&cstr!("@mozilla.org/file/local;1"))
                .ok_or("couldn't create nsIFile")?;
        let process: RefPtr<nsIProcess> =
            xpcom::create_instance(&cstr!("@mozilla.org/process/util;1"))
                .ok_or("couldn't create nsIProcess")?;
        unsafe {
            pingsender_file
                .InitWithPath(&*nsString::from(pingsender_path) as &nsAString)
                .to_result()?;
            process.Init(&*pingsender_file).to_result()?;
            process.SetStartHidden(true).to_result()?;
            process.SetNoShell(true).to_result()?;
        };
        let server_url = CString::new(format!(
            "https://incoming.telemetry.mozilla.org{}",
            lines[0]
        ))?;
        let telemetry_ping_path_cstr = CString::new(
            telemetry_ping_path
                .to_str()
                .expect("non-unicode ping path character"),
        )?;
        let mut args = [server_url.as_ptr(), telemetry_ping_path_cstr.as_ptr()];
        let args_length = 2;
        // Block while running the process.
        // We should feel free to do this because we're on our own thread.
        // Also, if we run async, nsIProcess tries to get the ObserverService on this thread, and asserts.
        unsafe {
            process
                .Run(true /* blocking */, args.as_mut_ptr(), args_length)
                .to_result()?;
        };
    }
    Ok(())
}

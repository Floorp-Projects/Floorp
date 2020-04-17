/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use bits_client::{BitsClient, BitsJobState, BitsProxyUsage};
use std::{ffi::OsString, fs::create_dir_all, path::Path};

pub fn sync_download_url(
    download_dir: OsString,
    save_path: OsString,
    job_name: OsString,
    url: OsString,
) -> Result<(), String> {
    let immediate_download_dir = Path::new(&save_path)
        .parent()
        .ok_or("Couldn't get immediate download directory".to_string())?;
    create_dir_all(immediate_download_dir)
        .map_err(|e| format!("Couldn't create download directory: {}", e))?;

    let mut client = BitsClient::new(job_name, download_dir)
        .map_err(|e| format!("Error constructing BitsClient: {}", e))?;

    // Kill the transfer after 15 minutes of making no progress.
    let no_progress_timeout_sec: u32 = 15 * 60;
    // Transfer status updates are checked by polling. Set the polling interval to 10 minutes.
    // This is just for monitoring update progress, which we don't really care about right now.
    // Transfer completion (including errors) are propagated immediately, without waiting for the
    // monitor interval.
    let monitor_interval_ms: u32 = 10 * 60 * 1000;
    let (job, mut monitor_client) = client
        .start_job(
            url,
            OsString::from(save_path),
            BitsProxyUsage::Preconfig,
            no_progress_timeout_sec,
            monitor_interval_ms,
        )
        .map_err(|e| format!("BitsClient error when starting job: {}", e))?
        .map_err(|e| format!("BITS error when starting job: {}", e))?;

    loop {
        // The monitor timeout should be at least as long as the monitor interval so we don't time
        // out just waiting for the interval to expire.
        let monitor_timeout_ms = 15 * 60 * 1000;
        // get_status will cause the program to sleep until the monitor interval expires, or the
        // transfer completes.
        match monitor_client.get_status(monitor_timeout_ms) {
            Err(error) => {
                let _ = client.cancel_job(job.guid);
                return Err(format!("BitsMonitorClient Error: {}", error));
            }
            Ok(Err(error)) => {
                let _ = client.cancel_job(job.guid);
                return Err(format!("BITS error while getting status: {}", error));
            }
            Ok(Ok(job_status)) => match job_status.state {
                BitsJobState::Suspended => {
                    let _ = client.cancel_job(job.guid);
                    return Err("BITS job externally suspended".to_string());
                }
                BitsJobState::Cancelled => return Err("BITS job externally cancelled".to_string()),
                BitsJobState::Acknowledged => {
                    return Err("BITS job externally completed".to_string())
                }
                BitsJobState::Error => {
                    let _ = client.cancel_job(job.guid);
                    if let Some(error) = job_status.error {
                        return Err(format!("BITS error while transferring: {}", error));
                    }
                    return Err("Unknown BITS error".to_string());
                }
                BitsJobState::Other(state) => {
                    let _ = client.cancel_job(job.guid);
                    return Err(format!("BITS returned an unknown state: {}", state));
                }
                BitsJobState::Queued
                | BitsJobState::Connecting
                | BitsJobState::Transferring
                | BitsJobState::TransientError => {
                    // Continue waiting for transfer to complete
                }
                BitsJobState::Transferred => break,
            },
        }
    }
    client
        .complete_job(job.guid)
        .map_err(|e| format!("BitsClient error when completing job: {}", e))?
        .map_err(|e| format!("BITS error when completing job: {}", e))?;
    Ok(())
}

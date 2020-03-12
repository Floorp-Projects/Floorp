/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::cmd::Command;
use std::ffi::{OsStr, OsString};
use std::path::Path;

use comedy::HResult;
use log::warn;

use crate::cmd;
use crate::ole_utils::BString;
use crate::taskschd::{hr_is_not_found, TaskService};
use crate::try_to_bstring;

fn folder_name() -> Result<BString, HResult> {
    try_to_bstring!(crate::VENDOR)
}

pub fn register(
    exe: &Path,
    name: &OsStr,
    args: &[OsString],
    command: Command,
) -> Result<(), failure::Error> {
    let name = try_to_bstring!(name)?;
    let folder_name = folder_name()?;

    let local_service = match command {
        Command::RegisterTask | Command::UpdateTask => false,
        Command::RegisterTaskLocalService | Command::UpdateTaskLocalService => true,
        _ => unreachable!(),
    };
    let update_task = match command {
        Command::RegisterTask | Command::RegisterTaskLocalService => false,
        Command::UpdateTask | Command::UpdateTaskLocalService => true,
        _ => unreachable!(),
    };

    let mut service = TaskService::connect_local()?;

    // Get or create the folder
    let mut folder = service.get_folder(&folder_name).or_else(|e| {
        if hr_is_not_found(&e) {
            service
                .get_root_folder()
                .and_then(|mut root| root.create_folder(&folder_name))
        } else {
            Err(e)
        }
    })?;

    // When updating, we still delete and recreate the task.
    // The only part that is currently copied over is the start boundary of the
    // daily trigger, since that it otherwise set to 5 minutes before the registration; in this way
    // updates won't delay the task's next scheduled run time.

    // TODO: We may want to also track if the task was disabled.

    let start_time = if update_task {
        // Ignoring any failures, if we can't get the time for any reason we choose a new one.
        folder
            .get_task(&name)
            .ok()
            .and_then(|mut task| task.get_definition().ok())
            .and_then(|mut def| def.get_daily_triggers().ok())
            .and_then(|mut triggers| {
                // Currently we are only using 1 daily trigger.
                triggers
                    .get_mut(0)
                    .and_then(|trigger| trigger.get_StartBoundary().ok())
            })
    } else {
        None
    };

    folder.delete_task(&name).unwrap_or_else(|e| {
        // Don't even warn if the task didn't exist.
        if !hr_is_not_found(&e) {
            warn!("delete task failed: {}", e);
        }
    });

    let mut task_def = service.new_task_definition()?;

    {
        let mut task_args = vec![OsString::from(cmd::DO_TASK)];
        task_args.extend_from_slice(args);

        let mut action = task_def.add_exec_action()?;
        action.put_Path(exe)?;
        action.put_Arguments(task_args.as_slice())?;
        // TODO working directory?
    }

    {
        let mut settings = task_def.get_settings()?;
        settings.put_DisallowStartIfOnBatteries(false)?;
        settings.put_StopIfGoingOnBatteries(false)?;
        settings.put_StartWhenAvailable(true)?;
        settings.put_ExecutionTimeLimit(chrono::Duration::minutes(5))?;
    }

    {
        let mut info = task_def.get_registration_info()?;
        info.put_Author(&try_to_bstring!(crate::VENDOR)?)?;
        info.put_Description(&try_to_bstring!(crate::DESCRIPTION)?)?;
    }

    // A daily trigger starting 5 minutes ago.
    {
        let mut daily_trigger = task_def.add_daily_trigger()?;
        if let Some(ref start_time) = start_time {
            daily_trigger.put_StartBoundary_BString(start_time)?;
        } else {
            daily_trigger.put_StartBoundary(chrono::Utc::now() - chrono::Duration::minutes(5))?;
        }
        daily_trigger.put_DaysInterval(1)?;
        // TODO: 12-hourly trigger? logon trigger?
    }

    let service_account = if local_service {
        Some(try_to_bstring!("NT AUTHORITY\\LocalService")?)
    } else {
        None
    };

    let mut registered_task = task_def.create(&mut folder, &name, service_account.as_ref())?;

    if local_service {
        // SDDL seem to be the only way to set the security descriptor.
        // https://docs.microsoft.com/en-us/windows/win32/secauthz/security-descriptor-string-format
        // Setting just the DACL here allows us to avoid specifying the ownership information,
        // which I think is required if SDDL is provided on initial task registration.
        let sddl = try_to_bstring!(concat!(
            "D:(",   // DACL
            "A;",    // ace_type = Allow
            ";",     // ace_flags = none
            "GRGX;", // rights = Generic Read, Generic Execute
            ";;",    // object_guid, inherit_object_guid = none
            "BU)"    // account_sid = Built-in users
        ))?;

        registered_task.set_sd(&sddl)?;
    }

    Ok(())
}

pub fn unregister(name: &OsStr) -> Result<(), failure::Error> {
    let name = try_to_bstring!(name)?;
    let folder_name = folder_name()?;

    let mut service = TaskService::connect_local()?;
    let maybe_folder = service.get_folder(&folder_name);
    let mut folder = match maybe_folder {
        Err(e) => {
            if hr_is_not_found(&e) {
                // Just warn and exit if the folder didn't exist.
                warn!("failed to unregister: task folder didn't exist");
                return Ok(());
            } else {
                // Other errors are fatal.
                return Err(e.into());
            }
        }
        Ok(folder) => folder,
    };

    folder.delete_task(&name).or_else(|e| {
        if hr_is_not_found(&e) {
            // Only warn if the task didn't exist, still try to remove the folder below.
            warn!("failed to unregister task that didn't exist");
            Ok(())
        } else {
            // Other errors are fatal.
            Err(e)
        }
    })?;

    let count = folder.get_task_count(true).unwrap_or_else(|e| {
        warn!("failed getting task count: {}", e);
        1
    });

    if count == 0 {
        let result = service
            .get_root_folder()
            .and_then(|mut root| root.delete_folder(&folder_name));
        if let Err(e) = result {
            warn!("failed deleting folder: {}", e);
        }
    }

    Ok(())
}

pub fn run_on_demand(name: &OsStr) -> Result<(), failure::Error> {
    let name = try_to_bstring!(name)?;
    let folder_name = folder_name()?;

    let mut service = TaskService::connect_local()?;
    let task = service.get_folder(&folder_name)?.get_task(&name)?;

    task.run()?;

    Ok(())
}

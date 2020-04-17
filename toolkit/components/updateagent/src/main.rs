/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// This code is Windows-specific, don't build this module for another platform.
#![cfg(windows)]
// We want to use the "windows" subsystem to avoid popping up a console window. This also
// prevents Windows consoles from picking up output, however, so it is disabled when debugging.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod event_log;
mod ole_utils;
mod task_setup;
pub mod taskschd;
mod update_paths;
mod update_xml;

use std::env;
use std::ffi::OsString;
use std::process;

use comedy::com::ComApartmentScope;
use log::{debug, error, info};

// Used as the name of the task folder and author of the task
pub static VENDOR: &str = "Mozilla";
// Used as the description of the task and the event log application
pub static DESCRIPTION: &str = "Mozilla Update Agent";
static BITS_JOB_NAME_PREFIX: &str = "MozillaUpdate ";

fn main() {
    log::set_logger(&event_log::EventLogger).unwrap();
    log::set_max_level(log::LevelFilter::Info);

    // TODO: Appropriate threading and security settings will depend on the main work the task
    // will be doing.
    let _com = ComApartmentScope::init_mta().unwrap();

    process::exit(match fallible_main() {
        Ok(_) => {
            debug!("success");
            0
        }
        Err(e) => {
            error!("{}", e);
            1
        }
    });
}

/// Command types
pub mod cmd {
    // Create a new task, removing any old one.
    // `updateagent.exe register-task TaskName [TaskArgs ...]`
    pub static REGISTER_TASK: &str = "register-task";
    pub static REGISTER_TASK_LOCAL_SERVICE: &str = "register-task-local-service";

    // Create a new task, removing any old one, but copying its schedule as appropriate.
    pub static UPDATE_TASK: &str = "update-task";
    pub static UPDATE_TASK_LOCAL_SERVICE: &str = "update-task-local-service";

    // Remove the task.
    pub static UNREGISTER_TASK: &str = "unregister-task";

    // Request to be run immediately by Task Scheduler.
    pub static RUN_ON_DEMAND: &str = "run-on-demand";

    // The task is set up to execute this command, using the TaskArgs from registration.
    // `updateagent.exe do-task [TaskArgs ...]`
    pub static DO_TASK: &str = "do-task";

    #[derive(Clone)]
    pub enum Command {
        RegisterTask,
        RegisterTaskLocalService,
        UpdateTask,
        UpdateTaskLocalService,
        UnregisterTask,
        RunOnDemand,
        DoTask,
    }

    impl Command {
        pub fn parse(s: &str) -> Option<Command> {
            use Command::*;

            // Build a map to lookup the string. This only runs once so performance isn't critical.
            let lookup_map: std::collections::HashMap<_, _> = [
                (REGISTER_TASK, RegisterTask),
                (REGISTER_TASK_LOCAL_SERVICE, RegisterTaskLocalService),
                (UPDATE_TASK, UpdateTask),
                (UPDATE_TASK_LOCAL_SERVICE, UpdateTaskLocalService),
                (UNREGISTER_TASK, UnregisterTask),
                (RUN_ON_DEMAND, RunOnDemand),
                (DO_TASK, DoTask),
            ]
            .iter()
            .cloned()
            .collect();

            lookup_map.get(s).cloned()
        }
    }
}

use cmd::Command;

pub fn fallible_main() -> Result<(), String> {
    let args_os: Vec<_> = env::args_os().collect();

    let command = {
        let command_str = args_os
            .get(1)
            .ok_or_else(|| String::from("missing command"))?
            .to_string_lossy()
            .to_owned();

        Command::parse(&command_str)
            .ok_or_else(|| format!("unknown command \"{}\"", command_str))?
    };

    // all commands except DoTask take TaskName as args_os[2]
    let maybe_task_name = args_os
        .get(2)
        .ok_or_else(|| String::from("missing TaskName"));

    match command {
        Command::RegisterTask
        | Command::RegisterTaskLocalService
        | Command::UpdateTask
        | Command::UpdateTaskLocalService => {
            let exe = env::current_exe().map_err(|e| format!("get current exe failed: {}", e))?;
            let task_name = maybe_task_name?;

            task_setup::register(&*exe, task_name, &args_os[3..], command)
                .map_err(|e| format!("register failed: {}", e))
        }
        Command::UnregisterTask => {
            if args_os.len() != 3 {
                return Err("unregister-task takes only one argument: TaskName".into());
            }

            task_setup::unregister(maybe_task_name?)
                .map_err(|e| format!("unregister failed: {}", e))
        }
        Command::RunOnDemand => {
            if args_os.len() != 3 {
                return Err("run-on-demand takes only one argument: TaskName".into());
            }

            task_setup::run_on_demand(maybe_task_name?)
                .map_err(|e| format!("run on demand failed: {}", e))
        }
        Command::DoTask => task_action(&args_os[2..]),
    }
}

fn task_action(args: &[OsString]) -> Result<(), String> {
    info!("task_action({:?})", args);

    let download_dir = update_paths::get_update_directory()?;
    let xml_path = update_paths::get_download_xml()?;
    let mut job_name = OsString::from(BITS_JOB_NAME_PREFIX);
    job_name.push(update_paths::get_install_hash()?);

    update_xml::sync_download(download_dir, xml_path.clone(), job_name)?;
    let updates = update_xml::parse_file(&xml_path)?;
    info!("Got {} updates!", updates.len());

    // TODO: Process updates

    Ok(())
}

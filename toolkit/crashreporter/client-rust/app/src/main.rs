/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Use the WINDOWS windows subsystem. This prevents a console window from opening with the
// application.
#![cfg_attr(windows, windows_subsystem = "windows")]

use crate::std::sync::Arc;
use anyhow::Context;
use config::Config;

/// cc is short for Clone Capture, a shorthand way to clone a bunch of values before an expression
/// (particularly useful for closures).
///
/// It is defined here to allow it to be used in all submodules (textual scope lookup).
macro_rules! cc {
    ( ($($c:ident),*) $e:expr ) => {
        {
            $(let $c = $c.clone();)*
            $e
        }
    }
}

mod async_task;
mod config;
mod data;
mod lang;
mod logging;
mod logic;
mod net;
mod process;
mod settings;
mod std;
mod thread_bound;
mod ui;

fn main() {
    let log_target = logging::init();

    let mut config = Config::new();
    let config_result = config.read_from_environment();
    config.log_target = Some(log_target);

    let mut config = Arc::new(config);

    let result = config_result.and_then(|()| {
        let attempted_send = try_run(&mut config)?;
        if !attempted_send {
            // Exited without attempting to send the crash report; delete files.
            config.delete_files();
        }
        Ok(())
    });

    if let Err(message) = result {
        // TODO maybe errors should also delete files?
        log::error!("exiting with error: {message}");
        if !config.auto_submit {
            // Only show a dialog if auto_submit is disabled.
            ui::error_dialog(&config, message);
        }
        std::process::exit(1);
    }
}

fn try_run(config: &mut Arc<Config>) -> anyhow::Result<bool> {
    if config.dump_file.is_none() {
        if !config.auto_submit {
            Err(anyhow::anyhow!(config.string("crashreporter-information")))
        } else {
            Ok(false)
        }
    } else {
        // Run minidump-analyzer to gather stack traces.
        {
            let analyzer_path = config.sibling_program_path("minidump-analyzer");
            let mut cmd = crate::process::background_command(&analyzer_path);
            if config.dump_all_threads {
                cmd.arg("--full");
            }
            cmd.arg(config.dump_file());
            let output = cmd
                .output()
                .with_context(|| config.string("crashreporter-error-minidump-analyzer"))?;
            if !output.status.success() {
                log::warn!(
                    "minidump-analyzer failed to run ({});\n\nstderr: {}\n\nstdout: {}",
                    output.status,
                    String::from_utf8_lossy(&output.stderr),
                    String::from_utf8_lossy(&output.stdout),
                );
            }
        }

        let extra = {
            // Perform a few things which may change the config, then treat is as immutable.
            let config = Arc::get_mut(config).expect("unexpected config references");
            let extra = config.load_extra_file()?;
            config.move_crash_data_to_pending()?;
            extra
        };

        logic::ReportCrash::new(config.clone(), extra)?.run()
    }
}

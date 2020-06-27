/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! This provides a XPCOM service to send app services logs to the desktop

#[macro_use]
extern crate xpcom;

use golden_gate::log::LogSink;
use log;
use nserror::{nsresult, NS_OK};
use nsstring::nsAString;
use once_cell::sync::Lazy;
use std::{cmp, collections::HashMap, sync::RwLock};
use xpcom::{
    interfaces::{mozIAppServicesLogger, mozIServicesLogSink},
    RefPtr,
};

#[derive(xpcom)]
#[xpimplements(mozIAppServicesLogger)]
#[refcnt = "nonatomic"]
pub struct InitAppServicesLogger {}

pub static LOGGERS_BY_TARGET: Lazy<RwLock<HashMap<String, LogSink>>> = Lazy::new(|| {
    let h: HashMap<String, LogSink> = HashMap::new();
    let m = RwLock::new(h);
    m
});

impl AppServicesLogger {
    xpcom_method!(register => Register(target: *const nsAString, logger: *const mozIServicesLogSink));
    fn register(&self, target: &nsAString, logger: &mozIServicesLogSink) -> Result<(), nsresult> {
        let log_sink_logger = LogSink::with_logger(Some(logger))?;
        let max_level = cmp::max(log::max_level(), log_sink_logger.max_level);

        // Note: This will only work if the max_level is lower than the compile-time
        // max_level_* filter.
        log::set_max_level(max_level);

        LOGGERS_BY_TARGET
            .write()
            .unwrap()
            .insert(target.to_string(), log_sink_logger);
        Ok(())
    }

    pub fn is_app_services_logger_registered(target: String) -> bool {
        match LOGGERS_BY_TARGET.read() {
            Ok(loggers_by_target) => loggers_by_target.contains_key(&target),
            Err(_e) => false,
        }
    }
}

/// The constructor for an `AppServicesLogger` service. This uses C linkage so that it
/// can be called from C++. See `AppServicesLoggerComponents.h` for the C++
/// constructor that's passed to the component manager.
///
/// # Safety
///
/// This function is unsafe because it dereferences `result`.
#[no_mangle]
pub unsafe extern "C" fn NS_NewAppServicesLogger(
    result: *mut *const mozIAppServicesLogger,
) -> nsresult {
    let logger = AppServicesLogger::allocate(InitAppServicesLogger {});
    RefPtr::new(logger.coerce::<mozIAppServicesLogger>()).forget(&mut *result);
    NS_OK
}

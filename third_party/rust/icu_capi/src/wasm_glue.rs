// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::format;
use diplomat_runtime::{console_log, console_warn};
use log::{Level, LevelFilter, Metadata, Record};

struct ConsoleLogger;

impl log::Log for ConsoleLogger {
    fn enabled(&self, _: &Metadata) -> bool {
        true
    }

    fn log(&self, record: &Record) {
        if record.level() <= Level::Warn {
            console_warn(format!("[{}] {}", record.level(), record.args()).as_str());
        } else {
            console_log(format!("[{}] {}", record.level(), record.args()).as_str());
        }
    }

    fn flush(&self) {}
}

static LOGGER: ConsoleLogger = ConsoleLogger;

#[no_mangle]
pub unsafe extern "C" fn icu4x_init() {
    log::set_logger(&LOGGER)
        .map(|()| log::set_max_level(LevelFilter::Debug))
        .unwrap();
}

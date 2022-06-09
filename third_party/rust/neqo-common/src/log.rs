// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::io::Write;
use std::sync::Once;
use std::time::Instant;

#[macro_export]
macro_rules! do_log {
    (target: $target:expr, $lvl:expr, $($arg:tt)+) => ({
        let lvl = $lvl;
        if lvl <= ::log::max_level() {
            ::log::__private_api_log(
                ::log::__log_format_args!($($arg)+),
                lvl,
                &($target, ::log::__log_module_path!(), ::log::__log_file!(), ::log::__log_line!()),
                None,
            );
        }
    });
    ($lvl:expr, $($arg:tt)+) => ($crate::do_log!(target: ::log::__log_module_path!(), $lvl, $($arg)+))
}

#[macro_export]
macro_rules! log_subject {
    ($lvl:expr, $subject:expr) => {{
        if $lvl <= ::log::max_level() {
            format!("{}", $subject)
        } else {
            String::new()
        }
    }};
}

use env_logger::Builder;

static INIT_ONCE: Once = Once::new();

lazy_static! {
    static ref START_TIME: Instant = Instant::now();
}

pub fn init() {
    INIT_ONCE.call_once(|| {
        let mut builder = Builder::from_env("RUST_LOG");
        builder.format(|buf, record| {
            let elapsed = START_TIME.elapsed();
            writeln!(
                buf,
                "{}s{:3}ms {} {}",
                elapsed.as_secs(),
                elapsed.as_millis() % 1000,
                record.level(),
                record.args()
            )
        });
        if let Err(e) = builder.try_init() {
            do_log!(::log::Level::Info, "Logging initialization error {:?}", e);
        } else {
            do_log!(::log::Level::Info, "Logging initialized");
        }
    });
}

#[macro_export]
macro_rules! log_invoke {
    ($lvl:expr, $ctx:expr, $($arg:tt)*) => ( {
        ::neqo_common::log::init();
        ::neqo_common::do_log!($lvl, "[{}] {}", $ctx, format!($($arg)*));
    } )
}
#[macro_export]
macro_rules! qerror {
    ([$ctx:expr], $($arg:tt)*) => (::neqo_common::log_invoke!(::log::Level::Error, $ctx, $($arg)*););
    ($($arg:tt)*) => ( { ::neqo_common::log::init(); ::neqo_common::do_log!(::log::Level::Error, $($arg)*); } );
}
#[macro_export]
macro_rules! qwarn {
    ([$ctx:expr], $($arg:tt)*) => (::neqo_common::log_invoke!(::log::Level::Warn, $ctx, $($arg)*););
    ($($arg:tt)*) => ( { ::neqo_common::log::init(); ::neqo_common::do_log!(::log::Level::Warn, $($arg)*); } );
}
#[macro_export]
macro_rules! qinfo {
    ([$ctx:expr], $($arg:tt)*) => (::neqo_common::log_invoke!(::log::Level::Info, $ctx, $($arg)*););
    ($($arg:tt)*) => ( { ::neqo_common::log::init(); ::neqo_common::do_log!(::log::Level::Info, $($arg)*); } );
}
#[macro_export]
macro_rules! qdebug {
    ([$ctx:expr], $($arg:tt)*) => (::neqo_common::log_invoke!(::log::Level::Debug, $ctx, $($arg)*););
    ($($arg:tt)*) => ( { ::neqo_common::log::init(); ::neqo_common::do_log!(::log::Level::Debug, $($arg)*); } );
}
#[macro_export]
macro_rules! qtrace {
    ([$ctx:expr], $($arg:tt)*) => (::neqo_common::log_invoke!(::log::Level::Trace, $ctx, $($arg)*););
    ($($arg:tt)*) => ( { ::neqo_common::log::init(); ::neqo_common::do_log!(::log::Level::Trace, $($arg)*); } );
}

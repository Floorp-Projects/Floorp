/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use parking_lot::RwLock;
use std::sync::atomic::{AtomicU32, Ordering};

/// Counter for breadcrumb messages
///
/// We are currently seeing breadcrumbs that may indicate that the reporting is unreliable.  In
/// some reports, the breadcrumbs seem like they may be duplicated and/or out of order.  This
/// counter is a temporary measure to check out that theory.
static BREADCRUMB_COUNTER: AtomicU32 = AtomicU32::new(0);

fn get_breadcrumb_counter_value() -> u32 {
    // Notes:
    //   - fetch_add is specified to wrap around in case of overflow, which seems okay.
    //   - By itself, this does not guarentee that breadcrumb logs will be ordered the same way as
    //     the counter values.  If two threads are running at the same time, it's very possible
    //     that thread A gets the lower breadcrumb value, but thread B wins the race to report its
    //     breadcrumb. However, if we expect operations to be synchronized, like with places DB,
    //     then the breadcrumb counter values should always increase by 1.
    BREADCRUMB_COUNTER.fetch_add(1, Ordering::Relaxed)
}

/// Application error reporting trait
///
/// The application that's consuming application-services implements this via a UniFFI callback
/// interface, then calls `set_application_error_reporter()` to setup a global
/// ApplicationErrorReporter.
pub trait ApplicationErrorReporter: Sync + Send {
    /// Send an error report to a Sentry-like error reporting system
    ///
    /// type_name should be used to group errors together
    fn report_error(&self, type_name: String, message: String);
    /// Send a breadcrumb to a Sentry-like error reporting system
    fn report_breadcrumb(&self, message: String, module: String, line: u32, column: u32);
}

// ApplicationErrorReporter to use if the app doesn't set one
struct DefaultApplicationErrorReporter;
impl ApplicationErrorReporter for DefaultApplicationErrorReporter {
    fn report_error(&self, _type_name: String, _message: String) {}
    fn report_breadcrumb(&self, _message: String, _module: String, _line: u32, _column: u32) {}
}

lazy_static::lazy_static! {
    // RwLock rather than a Mutex, since we only expect to set this once.
    pub(crate) static ref APPLICATION_ERROR_REPORTER: RwLock<Box<dyn ApplicationErrorReporter>> = RwLock::new(Box::new(DefaultApplicationErrorReporter));
}

pub fn set_application_error_reporter(reporter: Box<dyn ApplicationErrorReporter>) {
    *APPLICATION_ERROR_REPORTER.write() = reporter;
}

pub fn report_error(type_name: String, message: String) {
    APPLICATION_ERROR_REPORTER
        .read()
        .report_error(type_name, message);
}

pub fn report_breadcrumb(message: String, module: String, line: u32, column: u32) {
    let message = format!("{} ({})", message, get_breadcrumb_counter_value());
    APPLICATION_ERROR_REPORTER
        .read()
        .report_breadcrumb(message, module, line, column);
}

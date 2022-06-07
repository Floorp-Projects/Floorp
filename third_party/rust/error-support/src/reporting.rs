/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use parking_lot::RwLock;

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
    APPLICATION_ERROR_REPORTER
        .read()
        .report_breadcrumb(message, module, line, column);
}

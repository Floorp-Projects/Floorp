/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! `Action` is an enum describing what BITS action is being processed. This is
//! used mostly for logging and error reporting reasons.
//! The values of `Action` describe actions that could be in progress for
//! BitsService or BitsRequest. When specifying a type, `ServiceAction` or
//! `RequestAction`, can be used to restrict the action type to one of the two
//! categories.
//! A value of type `ServiceAction` or `RequestAction` can easily be converted
//! to an `Action` using the `into()` method.

use std::convert::From;
use xpcom::interfaces::nsIBits;

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum Action {
    StartDownload,
    MonitorDownload,
    Complete,
    Cancel,
    SetMonitorInterval,
    SetPriority,
    SetNoProgressTimeout,
    Resume,
    Suspend,
}

impl Action {
    pub fn description(&self) -> &'static str {
        match self {
            Action::StartDownload => "starting download",
            Action::MonitorDownload => "monitoring download",
            Action::Complete => "completing download",
            Action::Cancel => "cancelling download",
            Action::SetMonitorInterval => "changing monitor interval",
            Action::SetPriority => "setting download priority",
            Action::SetNoProgressTimeout => "setting no progress timeout",
            Action::Resume => "resuming download",
            Action::Suspend => "suspending download",
        }
    }

    pub fn as_error_code(&self) -> i32 {
        let val = match self {
            Action::StartDownload => nsIBits::ERROR_ACTION_START_DOWNLOAD,
            Action::MonitorDownload => nsIBits::ERROR_ACTION_MONITOR_DOWNLOAD,
            Action::Complete => nsIBits::ERROR_ACTION_COMPLETE,
            Action::Cancel => nsIBits::ERROR_ACTION_CANCEL,
            Action::SetMonitorInterval => nsIBits::ERROR_ACTION_CHANGE_MONITOR_INTERVAL,
            Action::SetPriority => nsIBits::ERROR_ACTION_SET_PRIORITY,
            Action::SetNoProgressTimeout => nsIBits::ERROR_ACTION_SET_NO_PROGRESS_TIMEOUT,
            Action::Resume => nsIBits::ERROR_ACTION_RESUME,
            Action::Suspend => nsIBits::ERROR_ACTION_SUSPEND,
        };
        val as i32
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum ServiceAction {
    StartDownload,
    MonitorDownload,
}

impl From<ServiceAction> for Action {
    fn from(action: ServiceAction) -> Action {
        match action {
            ServiceAction::StartDownload => Action::StartDownload,
            ServiceAction::MonitorDownload => Action::MonitorDownload,
        }
    }
}

impl ServiceAction {
    pub fn as_error_code(&self) -> i32 {
        Action::as_error_code(&(self.clone()).into())
    }

    pub fn description(&self) -> &'static str {
        Action::description(&(self.clone()).into())
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum RequestAction {
    Complete,
    Cancel,
    SetMonitorInterval,
    SetPriority,
    SetNoProgressTimeout,
    Resume,
    Suspend,
}

impl From<RequestAction> for Action {
    fn from(action: RequestAction) -> Action {
        match action {
            RequestAction::Complete => Action::Complete,
            RequestAction::Cancel => Action::Cancel,
            RequestAction::SetMonitorInterval => Action::SetMonitorInterval,
            RequestAction::SetPriority => Action::SetPriority,
            RequestAction::SetNoProgressTimeout => Action::SetNoProgressTimeout,
            RequestAction::Resume => Action::Resume,
            RequestAction::Suspend => Action::Suspend,
        }
    }
}

impl RequestAction {
    pub fn as_error_code(&self) -> i32 {
        Action::as_error_code(&(self.clone()).into())
    }

    pub fn description(&self) -> &'static str {
        Action::description(&(self.clone()).into())
    }
}

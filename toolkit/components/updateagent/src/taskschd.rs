/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A partial type-safe interface for Windows Task Scheduler 2.0
//!
//! This provides structs thinly wrapping the taskschd interfaces, with methods implemented as
//! they've been needed for the update agent.
//!
//! If it turns out that much more flexibility is needed in task definitions, it may be worth
//! generating an XML string and using `ITaskFolder::RegisterTask` or
//! `ITaskDefinition::put_XmlText`, rather than adding more and more boilerplate here.
//!
//! See https://docs.microsoft.com/windows/win32/taskschd/task-scheduler-start-page for
//! Microsoft's documentation.

use std::ffi::{OsStr, OsString};
use std::os::windows::ffi::{OsStrExt, OsStringExt};
use std::path::Path;
use std::ptr;

use comedy::com::{create_instance_inproc_server, ComRef};
use comedy::error::{HResult, Win32Error};
use comedy::{com_call, com_call_getter};
use failure::Fail;

use crate::ole_utils::{empty_variant, BString, IntoVariantBool, OptionBstringExt};
use crate::try_to_bstring;

use winapi::shared::{
    ntdef::{LONG, SHORT},
    winerror::{
        ERROR_ALREADY_EXISTS, ERROR_BAD_ARGUMENTS, ERROR_FILE_NOT_FOUND, E_ACCESSDENIED,
        SCHED_E_SERVICE_NOT_RUNNING,
    },
};

use winapi::um::taskschd::{
    self, IDailyTrigger, IExecAction, IRegisteredTask, IRegistrationInfo, IRunningTask,
    ITaskDefinition, ITaskFolder, ITaskService, ITaskSettings, ITrigger, ITriggerCollection,
};

/// Check if the `HResult` represents the win32 `ERROR_FILE_NOT_FOUND`, returned by
/// several task scheduler methods.
pub fn hr_is_not_found(hr: &HResult) -> bool {
    hr.code() == HResult::from(Win32Error::new(ERROR_FILE_NOT_FOUND)).code()
}

/// Check if the `HResult` represents the win32 `ERROR_ALREADY_EXISTS`, returned by
/// several task scheduler methods.
pub fn hr_is_already_exists(hr: &HResult) -> bool {
    hr.code() == HResult::from(Win32Error::new(ERROR_ALREADY_EXISTS)).code()
}

// These macros simplify wrapping `put_*` property methods. They are each slightly different;
// I found it significantly more confusing to try to combine them.

/// put a bool, converting to `VARIANT_BOOL`
macro_rules! bool_putter {
    ($interface:ident :: $method:ident) => {
        #[allow(non_snake_case)]
        pub fn $method(&mut self, v: bool) -> Result<(), HResult> {
            let v = v.into_variant_bool();
            unsafe {
                com_call!(self.0, $interface::$method(v))?;
            }
            Ok(())
        }
    };
}

/// put a value that is already available as a `BString`
macro_rules! bstring_putter {
    ($interface:ident :: $method:ident) => {
        #[allow(non_snake_case)]
        pub fn $method(&mut self, v: &BString) -> Result<(), HResult> {
            unsafe {
                com_call!(self.0, $interface::$method(v.as_raw_ptr()))?;
            }
            Ok(())
        }
    };
}

/// put a `chrono::DateTime` value
macro_rules! datetime_putter {
    ($interface:ident :: $method:ident) => {
        #[allow(non_snake_case)]
        pub fn $method(&mut self, v: chrono::DateTime<chrono::Utc>) -> Result<(), HResult> {
            let v = try_to_bstring!(v.to_rfc3339_opts(chrono::SecondsFormat::Secs, true))?;
            unsafe {
                com_call!(self.0, $interface::$method(v.as_raw_ptr()))?;
            }
            Ok(())
        }
    };
}

/// put a value of type `$ty`, which implements `AsRef<OsStr>`
macro_rules! to_os_str_putter {
    ($interface:ident :: $method:ident, $ty:ty) => {
        #[allow(non_snake_case)]
        pub fn $method(&mut self, v: $ty) -> Result<(), HResult> {
            let v = try_to_bstring!(v)?;
            unsafe {
                com_call!(self.0, $interface::$method(v.as_raw_ptr()))?;
            }
            Ok(())
        }
    };
}

/// put a value of type `$ty`, which implements `ToString`
macro_rules! to_string_putter {
    ($interface:ident :: $method:ident, $ty:ty) => {
        #[allow(non_snake_case)]
        pub fn $method(&mut self, v: $ty) -> Result<(), HResult> {
            let v = try_to_bstring!(v.to_string())?;
            unsafe {
                com_call!(self.0, $interface::$method(v.as_raw_ptr()))?;
            }
            Ok(())
        }
    };
}

pub struct TaskService(ComRef<ITaskService>);

impl TaskService {
    pub fn connect_local() -> Result<TaskService, ConnectTaskServiceError> {
        use self::ConnectTaskServiceError::*;

        let task_service = create_instance_inproc_server::<taskschd::TaskScheduler, ITaskService>()
            .map_err(CreateInstanceFailed)?;

        // Connect to local service with no credentials.
        unsafe {
            com_call!(
                task_service,
                ITaskService::Connect(
                    empty_variant(),
                    empty_variant(),
                    empty_variant(),
                    empty_variant()
                )
            )
        }
        .map_err(|hr| match hr.code() {
            E_ACCESSDENIED => AccessDenied(hr),
            SCHED_E_SERVICE_NOT_RUNNING => ServiceNotRunning(hr),
            _ => ConnectFailed(hr),
        })?;

        Ok(TaskService(task_service))
    }

    pub fn get_root_folder(&mut self) -> Result<TaskFolder, HResult> {
        self.get_folder(&try_to_bstring!("\\")?)
    }

    pub fn get_folder(&mut self, path: &BString) -> Result<TaskFolder, HResult> {
        unsafe {
            com_call_getter!(
                |folder| self.0,
                ITaskService::GetFolder(path.as_raw_ptr(), folder)
            )
        }
        .map(TaskFolder)
    }

    pub fn new_task_definition(&mut self) -> Result<TaskDefinition, HResult> {
        unsafe {
            com_call_getter!(
                |task_def| self.0,
                ITaskService::NewTask(
                    0, // flags (reserved)
                    task_def,
                )
            )
        }
        .map(TaskDefinition)
    }
}

#[derive(Clone, Debug, Fail)]
pub enum ConnectTaskServiceError {
    #[fail(display = "{}", _0)]
    CreateInstanceFailed(#[fail(cause)] HResult),
    #[fail(display = "Access is denied to connect to the Task Scheduler service")]
    AccessDenied(#[fail(cause)] HResult),
    #[fail(display = "The Task Scheduler service is not running")]
    ServiceNotRunning(#[fail(cause)] HResult),
    #[fail(display = "{}", _0)]
    ConnectFailed(#[fail(cause)] HResult),
}

pub struct TaskFolder(ComRef<ITaskFolder>);

impl TaskFolder {
    pub fn get_task(&mut self, task_name: &BString) -> Result<RegisteredTask, HResult> {
        unsafe {
            com_call_getter!(
                |task| self.0,
                ITaskFolder::GetTask(task_name.as_raw_ptr(), task)
            )
        }
        .map(RegisteredTask)
    }

    pub fn get_task_count(&mut self, include_hidden: bool) -> Result<LONG, HResult> {
        use self::taskschd::IRegisteredTaskCollection;

        let flags = if include_hidden {
            taskschd::TASK_ENUM_HIDDEN
        } else {
            0
        };

        unsafe {
            let tasks = com_call_getter!(|t| self.0, ITaskFolder::GetTasks(flags as LONG, t))?;

            let mut count = 0;
            com_call!(tasks, IRegisteredTaskCollection::get_Count(&mut count))?;

            Ok(count)
        }
    }

    pub fn create_folder(&mut self, path: &BString) -> Result<TaskFolder, HResult> {
        let sddl = empty_variant();
        unsafe {
            com_call_getter!(
                |folder| self.0,
                ITaskFolder::CreateFolder(path.as_raw_ptr(), sddl, folder)
            )
        }
        .map(TaskFolder)
    }

    pub fn delete_folder(&mut self, path: &BString) -> Result<(), HResult> {
        unsafe {
            com_call!(
                self.0,
                ITaskFolder::DeleteFolder(
                    path.as_raw_ptr(),
                    0, // flags (reserved)
                )
            )?;
        }

        Ok(())
    }

    pub fn delete_task(&mut self, task_name: &BString) -> Result<(), HResult> {
        unsafe {
            com_call!(
                self.0,
                ITaskFolder::DeleteTask(
                    task_name.as_raw_ptr(),
                    0, // flags (reserved)
                )
            )?;
        }

        Ok(())
    }
}

pub struct TaskDefinition(ComRef<ITaskDefinition>);

impl TaskDefinition {
    pub fn get_settings(&mut self) -> Result<TaskSettings, HResult> {
        unsafe { com_call_getter!(|s| self.0, ITaskDefinition::get_Settings(s)) }.map(TaskSettings)
    }

    pub fn get_registration_info(&mut self) -> Result<RegistrationInfo, HResult> {
        unsafe { com_call_getter!(|ri| self.0, ITaskDefinition::get_RegistrationInfo(ri)) }
            .map(RegistrationInfo)
    }

    unsafe fn add_action<T: winapi::Interface>(
        &mut self,
        action_type: taskschd::TASK_ACTION_TYPE,
    ) -> Result<ComRef<T>, HResult> {
        use self::taskschd::IActionCollection;

        let actions = com_call_getter!(|ac| self.0, ITaskDefinition::get_Actions(ac))?;
        let action = com_call_getter!(|a| actions, IActionCollection::Create(action_type, a))?;
        action.cast()
    }

    pub fn add_exec_action(&mut self) -> Result<ExecAction, HResult> {
        unsafe { self.add_action(taskschd::TASK_ACTION_EXEC) }.map(ExecAction)
    }

    unsafe fn add_trigger<T: winapi::Interface>(
        &mut self,
        trigger_type: taskschd::TASK_TRIGGER_TYPE2,
    ) -> Result<ComRef<T>, HResult> {
        let triggers = com_call_getter!(|tc| self.0, ITaskDefinition::get_Triggers(tc))?;
        let trigger = com_call_getter!(|t| triggers, ITriggerCollection::Create(trigger_type, t))?;
        trigger.cast()
    }

    pub fn add_daily_trigger(&mut self) -> Result<DailyTrigger, HResult> {
        unsafe { self.add_trigger(taskschd::TASK_TRIGGER_DAILY) }.map(DailyTrigger)
    }

    pub fn get_daily_triggers(&mut self) -> Result<Vec<DailyTrigger>, HResult> {
        let mut found_triggers = Vec::new();

        unsafe {
            let triggers = com_call_getter!(|tc| self.0, ITaskDefinition::get_Triggers(tc))?;
            let mut count = 0;
            com_call!(triggers, ITriggerCollection::get_Count(&mut count))?;

            // Item indexes start at 1
            for i in 1..=count {
                let trigger = com_call_getter!(|t| triggers, ITriggerCollection::get_Item(i, t))?;

                let mut trigger_type = 0;
                com_call!(trigger, ITrigger::get_Type(&mut trigger_type))?;

                if trigger_type == taskschd::TASK_TRIGGER_DAILY {
                    found_triggers.push(DailyTrigger(trigger.cast()?))
                }
            }
        }

        Ok(found_triggers)
    }

    pub fn create(
        &mut self,
        folder: &mut TaskFolder,
        task_name: &BString,
        service_account: Option<&BString>,
    ) -> Result<RegisteredTask, HResult> {
        self.register_impl(folder, task_name, service_account, taskschd::TASK_CREATE)
    }

    fn register_impl(
        &mut self,
        folder: &mut TaskFolder,
        task_name: &BString,
        service_account: Option<&BString>,
        creation_flags: taskschd::TASK_CREATION,
    ) -> Result<RegisteredTask, HResult> {
        let task_definition = self.0.as_raw_ptr();

        let password = empty_variant();

        let logon_type = if service_account.is_some() {
            taskschd::TASK_LOGON_SERVICE_ACCOUNT
        } else {
            taskschd::TASK_LOGON_INTERACTIVE_TOKEN
        };

        let sddl = empty_variant();

        let registered_task = unsafe {
            com_call_getter!(
                |rt| folder.0,
                ITaskFolder::RegisterTaskDefinition(
                    task_name.as_raw_ptr(),
                    task_definition,
                    creation_flags as LONG,
                    service_account.as_raw_variant(),
                    password,
                    logon_type,
                    sddl,
                    rt,
                )
            )?
        };

        Ok(RegisteredTask(registered_task))
    }

    pub fn get_xml(task_definition: &ComRef<ITaskDefinition>) -> Result<OsString, String> {
        unsafe {
            let mut xml = ptr::null_mut();
            com_call!(task_definition, ITaskDefinition::get_XmlText(&mut xml))
                .map_err(|e| format!("{}", e))?;

            Ok(OsString::from_wide(
                BString::from_raw(xml)
                    .ok_or_else(|| "null xml".to_string())?
                    .as_ref(),
            ))
        }
    }
}

pub struct TaskSettings(ComRef<ITaskSettings>);

impl TaskSettings {
    bool_putter!(ITaskSettings::put_AllowDemandStart);
    bool_putter!(ITaskSettings::put_DisallowStartIfOnBatteries);
    to_string_putter!(ITaskSettings::put_ExecutionTimeLimit, chrono::Duration);
    bool_putter!(ITaskSettings::put_Hidden);

    #[allow(non_snake_case)]
    pub fn put_MultipleInstances(&mut self, v: InstancesPolicy) -> Result<(), HResult> {
        unsafe {
            com_call!(self.0, ITaskSettings::put_MultipleInstances(v as u32))?;
        }
        Ok(())
    }

    bool_putter!(ITaskSettings::put_RunOnlyIfIdle);
    bool_putter!(ITaskSettings::put_RunOnlyIfNetworkAvailable);
    bool_putter!(ITaskSettings::put_StartWhenAvailable);
    bool_putter!(ITaskSettings::put_StopIfGoingOnBatteries);
    bool_putter!(ITaskSettings::put_Enabled);
    bool_putter!(ITaskSettings::put_WakeToRun);
}

pub struct RegistrationInfo(ComRef<IRegistrationInfo>);

impl RegistrationInfo {
    bstring_putter!(IRegistrationInfo::put_Author);
    bstring_putter!(IRegistrationInfo::put_Description);
}

#[derive(Clone, Copy, Debug)]
#[repr(u32)]
pub enum InstancesPolicy {
    Parallel = taskschd::TASK_INSTANCES_PARALLEL,
    Queue = taskschd::TASK_INSTANCES_QUEUE,
    IgnoreNew = taskschd::TASK_INSTANCES_IGNORE_NEW,
    StopExisting = taskschd::TASK_INSTANCES_STOP_EXISTING,
}

pub struct DailyTrigger(ComRef<IDailyTrigger>);

impl DailyTrigger {
    datetime_putter!(IDailyTrigger::put_StartBoundary);

    // I'd like to have this only use the type-safe DateTime, but when copying it seems less
    // error-prone to use the string directly rather than try to parse it and then convert it back
    // to string.
    #[allow(non_snake_case)]
    pub fn put_StartBoundary_BString(&mut self, v: &BString) -> Result<(), HResult> {
        unsafe {
            com_call!(self.0, IDailyTrigger::put_StartBoundary(v.as_raw_ptr()))?;
        }
        Ok(())
    }

    #[allow(non_snake_case)]
    pub fn get_StartBoundary(&mut self) -> Result<BString, HResult> {
        unsafe {
            let mut start_boundary = ptr::null_mut();
            let hr = com_call!(
                self.0,
                IDailyTrigger::get_StartBoundary(&mut start_boundary)
            )?;
            BString::from_raw(start_boundary).ok_or_else(|| HResult::new(hr))
        }
    }

    #[allow(non_snake_case)]
    pub fn put_DaysInterval(&mut self, v: SHORT) -> Result<(), HResult> {
        unsafe {
            com_call!(self.0, IDailyTrigger::put_DaysInterval(v))?;
        }
        Ok(())
    }
}

pub struct ExecAction(ComRef<IExecAction>);

impl ExecAction {
    to_os_str_putter!(IExecAction::put_Path, &Path);
    to_os_str_putter!(IExecAction::put_WorkingDirectory, &Path);

    #[allow(non_snake_case)]
    pub fn put_Arguments(&mut self, args: &[OsString]) -> Result<(), HResult> {
        // based on `make_command_line()` from libstd
        // https://github.com/rust-lang/rust/blob/37ff5d388f8c004ca248adb635f1cc84d347eda0/src/libstd/sys/windows/process.rs#L457

        let mut s = Vec::new();

        fn append_arg(cmd: &mut Vec<u16>, arg: &OsStr) -> Result<(), HResult> {
            cmd.push('"' as u16);

            let mut backslashes: usize = 0;
            for x in arg.encode_wide() {
                if x == 0 {
                    return Err(HResult::from(Win32Error::new(ERROR_BAD_ARGUMENTS))
                        .file_line(file!(), line!()));
                }

                if x == '\\' as u16 {
                    backslashes += 1;
                } else {
                    if x == '"' as u16 {
                        // Add n+1 backslashes for a total of 2n+1 before internal '"'.
                        cmd.extend((0..=backslashes).map(|_| '\\' as u16));
                    }
                    backslashes = 0;
                }
                cmd.push(x);
            }

            // Add n backslashes for a total of 2n before ending '"'.
            cmd.extend((0..backslashes).map(|_| '\\' as u16));
            cmd.push('"' as u16);

            Ok(())
        }

        for arg in args {
            if !s.is_empty() {
                s.push(' ' as u16);
            }

            // always quote args
            append_arg(&mut s, arg.as_ref())?;
        }

        let args = BString::from_slice(s).map_err(|e| e.file_line(file!(), line!()))?;

        unsafe {
            com_call!(self.0, IExecAction::put_Arguments(args.as_raw_ptr()))?;
        }
        Ok(())
    }
}

pub struct RegisteredTask(ComRef<IRegisteredTask>);

impl RegisteredTask {
    pub fn set_sd(&mut self, sddl: &BString) -> Result<(), HResult> {
        unsafe {
            com_call!(
                self.0,
                IRegisteredTask::SetSecurityDescriptor(
                    sddl.as_raw_ptr(),
                    0, // flags (none)
                )
            )?;
        }
        Ok(())
    }

    pub fn get_definition(&mut self) -> Result<TaskDefinition, HResult> {
        unsafe { com_call_getter!(|tc| self.0, IRegisteredTask::get_Definition(tc)) }
            .map(TaskDefinition)
    }

    pub fn run(&self) -> Result<(), HResult> {
        self.run_impl(Option::<&OsStr>::None)?;
        Ok(())
    }

    fn run_impl(&self, param: Option<impl AsRef<OsStr>>) -> Result<ComRef<IRunningTask>, HResult> {
        // Running with parameters isn't currently exposed.
        // param can also be an array of strings, but that is not supported here
        let param = if let Some(p) = param {
            Some(try_to_bstring!(p)?)
        } else {
            None
        };

        unsafe {
            com_call_getter!(
                |rt| self.0,
                IRegisteredTask::Run(param.as_ref().as_raw_variant(), rt)
            )
        }
    }
}

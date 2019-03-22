// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

//! A safe interface for BITS
//!
//! The primary entry point into BITS is the
//! [`BackgroundCopyManager`](struct.BackgroundCopyManager.html) struct.
//!
//! Functionality is only provided by this crate on an as-needed basis for
//! [bits_client](../bits_client/index.html), so there are vast swathes of the BITS API
//! unsupported.

extern crate comedy;
extern crate filetime_win;
extern crate guid_win;
extern crate winapi;

#[cfg(feature = "status_serde")]
extern crate serde;
#[cfg(feature = "status_serde")]
extern crate serde_derive;

mod callback;
pub mod status;
mod wide;

use std::ffi::{OsStr, OsString};
use std::mem;
use std::os::windows::ffi::OsStringExt;
use std::ptr;
use std::result;

use comedy::com::{create_instance_local_server, CoTaskMem, ComRef, INIT_MTA};
use comedy::error::{HResult, ResultExt};
use comedy::{com_call, com_call_getter, com_call_taskmem_getter};
use filetime_win::FileTime;
use guid_win::Guid;
use winapi::shared::minwindef::DWORD;
use winapi::shared::ntdef::{HRESULT, LANGIDFROMLCID, ULONG};
use winapi::shared::winerror::S_FALSE;
use winapi::um::bits::{
    IBackgroundCopyError, IBackgroundCopyFile, IBackgroundCopyJob, IBackgroundCopyManager,
    IEnumBackgroundCopyFiles, IEnumBackgroundCopyJobs, BG_JOB_PRIORITY, BG_JOB_PRIORITY_FOREGROUND,
    BG_JOB_PRIORITY_HIGH, BG_JOB_PRIORITY_LOW, BG_JOB_PRIORITY_NORMAL, BG_JOB_PROXY_USAGE,
    BG_JOB_PROXY_USAGE_AUTODETECT, BG_JOB_PROXY_USAGE_NO_PROXY, BG_JOB_PROXY_USAGE_PRECONFIG,
    BG_JOB_STATE_ERROR, BG_JOB_STATE_TRANSIENT_ERROR, BG_JOB_TYPE_DOWNLOAD, BG_NOTIFY_DISABLE,
    BG_NOTIFY_JOB_ERROR, BG_NOTIFY_JOB_MODIFICATION, BG_NOTIFY_JOB_TRANSFERRED, BG_SIZE_UNKNOWN,
};
use winapi::um::bits2_5::{IBackgroundCopyJobHttpOptions, BG_HTTP_REDIRECT_POLICY_ALLOW_REPORT};
use winapi::um::bitsmsg::BG_E_NOT_FOUND;
use winapi::um::unknwnbase::IUnknown;
use winapi::um::winnls::GetThreadLocale;

pub use winapi::um::bits::{BG_ERROR_CONTEXT, BG_JOB_STATE};
pub use winapi::um::bitsmsg::{BG_S_PARTIAL_COMPLETE, BG_S_UNABLE_TO_DELETE_FILES};

pub use status::{
    BitsErrorContext, BitsJobError, BitsJobProgress, BitsJobState, BitsJobStatus, BitsJobTimes,
};
use wide::ToWideNull;

pub use winapi::shared::winerror::E_FAIL;

#[repr(u32)]
#[derive(Copy, Clone, Debug)]
pub enum BitsJobPriority {
    Foreground = BG_JOB_PRIORITY_FOREGROUND,
    High = BG_JOB_PRIORITY_HIGH,
    /// Default
    Normal = BG_JOB_PRIORITY_NORMAL,
    Low = BG_JOB_PRIORITY_LOW,
}

#[repr(u32)]
#[derive(Copy, Clone, Debug)]
pub enum BitsProxyUsage {
    /// Directly access the network.
    NoProxy = BG_JOB_PROXY_USAGE_NO_PROXY,
    /// Use Internet Explorer proxy settings. This is the default.
    Preconfig = BG_JOB_PROXY_USAGE_PRECONFIG,
    /// Attempt to auto-detect the connection's proxy settings.
    AutoDetect = BG_JOB_PROXY_USAGE_AUTODETECT,
}

type Result<T> = result::Result<T, HResult>;

pub struct BackgroundCopyManager(ComRef<IBackgroundCopyManager>);

impl BackgroundCopyManager {
    /// Get access to the local BITS service.
    ///
    /// # COM Initialization and Threading Model #
    ///
    /// This method uses a thread local variable to initialize COM with a multithreaded apartment
    /// model for this thread, and leaves it this way until the thread local is dropped.
    /// If the thread was in a single-threaded apartment, `connect()` will fail gracefully.
    ///
    /// # Safety #
    ///
    /// If there are mismatched `CoUninitialize` calls on this thread which lead to COM shutting
    /// down before this thread ends, unsafe behavior may result.
    pub fn connect() -> Result<BackgroundCopyManager> {
        INIT_MTA.with(|com| {
            if let Err(e) = com {
                return Err(e.clone());
            }
            Ok(())
        })?;

        // Assuming no mismatched CoUninitialize calls, methods do not have to check for
        // successfully initialized COM once the object is constructed: `BackgroundCopyManager`
        // is not `Send` or `Sync` so it must be used on the thread it was constructed on,
        // which has now successfully inited MTA for the lifetime of thread local `INIT_MTA`.
        // This also holds for any functions using pointers only derived from these methods, like
        // the `BitsJob` methods.

        Ok(BackgroundCopyManager(create_instance_local_server::<
            winapi::um::bits::BackgroundCopyManager,
            IBackgroundCopyManager,
        >()?))
    }

    /// Create a new download job with the given name.
    pub fn create_job(&self, display_name: &OsStr) -> Result<BitsJob> {
        unsafe {
            let mut guid = mem::zeroed();
            Ok(BitsJob(com_call_getter!(
                |job| self.0,
                IBackgroundCopyManager::CreateJob(
                    display_name.to_wide_null().as_ptr(),
                    BG_JOB_TYPE_DOWNLOAD,
                    &mut guid,
                    job,
                )
            )?))
        }
    }

    /// Cancel all jobs with the given name.
    ///
    /// This only attempts to cancel jobs owned by the current user.
    /// No errors are returned for jobs that failed to cancel.
    pub fn cancel_jobs_by_name(&self, match_name: &OsStr) -> Result<()> {
        let jobs =
            unsafe { com_call_getter!(|jobs| self.0, IBackgroundCopyManager::EnumJobs(0, jobs))? };

        loop {
            let result = unsafe {
                com_call_getter!(
                    |job| jobs,
                    IEnumBackgroundCopyJobs::Next(1, job, ptr::null_mut())
                )
            };
            match result {
                Ok(job) => {
                    if job_name_eq(&job, match_name)? {
                        unsafe {
                            let _ = com_call!(job, IBackgroundCopyJob::Cancel());
                        }
                    }
                }
                Err(e) => {
                    if e.code() == S_FALSE {
                        // Ran out of jobs to enumerate
                        return Ok(());
                    } else {
                        return Err(e);
                    }
                }
            }
        }
    }

    /// Get the job with the given GUID.
    ///
    /// Returns Err if the job was not found.
    pub fn get_job_by_guid(&self, guid: &Guid) -> Result<BitsJob> {
        unsafe { com_call_getter!(|job| self.0, IBackgroundCopyManager::GetJob(&guid.0, job)) }
            .map(BitsJob)
    }

    /// Try to find a job with a given GUID.
    ///
    /// Returns Ok(None) if the job was not found but there was no other error.
    pub fn find_job_by_guid(&self, guid: &Guid) -> Result<Option<BitsJob>> {
        Ok(self
            .get_job_by_guid(guid)
            .map(Some)
            .allow_err(BG_E_NOT_FOUND as i32, None)?)
    }

    /// Try to find a job with a given GUID and name.
    ///
    /// Returns Ok(None) if the job was not found, or if it had the wrong name, as long as there
    /// was no other error.
    pub fn find_job_by_guid_and_name(
        &self,
        guid: &Guid,
        match_name: &OsStr,
    ) -> Result<Option<BitsJob>> {
        Ok(match self.find_job_by_guid(guid)? {
            None => None,
            Some(BitsJob(ref job)) if !job_name_eq(job, match_name)? => None,
            result => result,
        })
    }

    /// Translate a BITS `HRESULT` to a textual description.
    ///
    /// This uses the current thread's locale to look up the message associated with a BITS
    /// error. It should only be used for `HRESULT`s returned from BITS COM interfaces.
    pub fn get_error_description(&self, hr: HRESULT) -> Result<String> {
        unsafe {
            let language_id = DWORD::from(LANGIDFROMLCID(GetThreadLocale()));

            Ok(taskmem_into_lossy_string(com_call_taskmem_getter!(
                |desc| self.0,
                IBackgroundCopyManager::GetErrorDescription(hr, language_id, desc)
            )?))
        }
    }
}

unsafe fn taskmem_into_lossy_string(taskmem: CoTaskMem<u16>) -> String {
    OsString::from_wide(taskmem.as_slice_until_null())
        .to_string_lossy()
        .into_owned()
}

fn job_name_eq(job: &ComRef<IBackgroundCopyJob>, match_name: &OsStr) -> Result<bool> {
    let job_name = unsafe {
        OsString::from_wide(
            com_call_taskmem_getter!(|name| job, IBackgroundCopyJob::GetDisplayName(name))?
                .as_slice_until_null(),
        )
    };

    Ok(job_name == match_name)
}

pub struct BitsJob(ComRef<IBackgroundCopyJob>);

impl BitsJob {
    /// Get the job's GUID.
    pub fn guid(&self) -> Result<Guid> {
        // TODO: cache on create or retrieved by GUID?
        unsafe {
            let mut guid = mem::zeroed();
            com_call!(self.0, IBackgroundCopyJob::GetId(&mut guid))?;
            Ok(Guid(guid))
        }
    }

    /// Add a file to the job.
    pub fn add_file(&mut self, remote_url: &OsStr, local_file: &OsStr) -> Result<()> {
        unsafe {
            com_call!(
                self.0,
                IBackgroundCopyJob::AddFile(
                    remote_url.to_wide_null().as_ptr(),
                    local_file.to_wide_null().as_ptr(),
                )
            )
        }?;
        Ok(())
    }

    /// Get the first file in the job.
    ///
    /// This is provided for collecting the redirected remote name of single file jobs.
    pub fn get_first_file(&mut self) -> Result<BitsFile> {
        let files = unsafe { com_call_getter!(|e| self.0, IBackgroundCopyJob::EnumFiles(e))? };

        let file = unsafe {
            com_call_getter!(
                |file| files,
                IEnumBackgroundCopyFiles::Next(1, file, ptr::null_mut())
            )?
        };

        Ok(BitsFile(file))
    }

    /// Set the job's description string.
    ///
    /// This is different from the display name set when creating the job.
    pub fn set_description(&mut self, description: &OsStr) -> Result<()> {
        unsafe {
            com_call!(
                self.0,
                IBackgroundCopyJob::SetDescription(description.to_wide_null().as_ptr())
            )
        }?;
        Ok(())
    }

    /// Change the job's proxy usage setting.
    ///
    /// The default is `BitsProxyUsage::Preconfig`.
    pub fn set_proxy_usage(&mut self, usage: BitsProxyUsage) -> Result<()> {
        use BitsProxyUsage::*;

        match usage {
            Preconfig | NoProxy | AutoDetect => {
                unsafe {
                    com_call!(
                        self.0,
                        IBackgroundCopyJob::SetProxySettings(
                            usage as BG_JOB_PROXY_USAGE,
                            ptr::null_mut(),
                            ptr::null_mut(),
                        )
                    )
                }?;
                Ok(())
            }
        }
    }

    /// Change the job's priority.
    ///
    /// The default is `BitsJobPriority::Normal`.
    pub fn set_priority(&mut self, priority: BitsJobPriority) -> Result<()> {
        unsafe {
            com_call!(
                self.0,
                IBackgroundCopyJob::SetPriority(priority as BG_JOB_PRIORITY)
            )
        }?;
        Ok(())
    }

    pub fn set_minimum_retry_delay(&mut self, seconds: ULONG) -> Result<()> {
        unsafe { com_call!(self.0, IBackgroundCopyJob::SetMinimumRetryDelay(seconds)) }?;
        Ok(())
    }

    /// Enable HTTP redirect reporting.
    ///
    /// The default setting is to allow HTTP redirects, but to not report them in any way. With
    /// this setting enabled, the remote name of a file will be updated to reflect the redirect.
    ///
    /// # Compatibility #
    ///
    /// First available in Windows Vista.
    pub fn set_redirect_report(&mut self) -> Result<()> {
        unsafe {
            com_call!(
                self.0.cast()?,
                IBackgroundCopyJobHttpOptions::SetSecurityFlags(
                    BG_HTTP_REDIRECT_POLICY_ALLOW_REPORT
                )
            )
        }?;

        Ok(())
    }

    /// Resume the job. This must be done at least once to initially enqueue the job.
    pub fn resume(&mut self) -> Result<()> {
        unsafe { com_call!(self.0, IBackgroundCopyJob::Resume()) }?;
        Ok(())
    }

    pub fn suspend(&mut self) -> Result<()> {
        unsafe { com_call!(self.0, IBackgroundCopyJob::Suspend()) }?;
        Ok(())
    }

    /// Complete the job, moving the local files to their final names.
    ///
    /// Has two interesting success `HRESULT`s: `BG_S_PARTIAL_COMPLETE` and
    /// `BG_S_UNABLE_TO_DELETE_FILES`.
    pub fn complete(&mut self) -> Result<HRESULT> {
        unsafe { com_call!(self.0, IBackgroundCopyJob::Complete()) }
    }

    /// Cancel the job, deleting any temporary files.
    ///
    /// Has an interesting success `HRESULT`: `BG_S_UNABLE_TO_DELETE_FILES`.
    pub fn cancel(&mut self) -> Result<HRESULT> {
        unsafe { com_call!(self.0, IBackgroundCopyJob::Cancel()) }
    }

    /// Set the notification callbacks to use with this job.
    ///
    /// This will replace any previously set callbacks.
    pub fn register_callbacks(
        &mut self,
        transferred_cb: Option<Box<callback::TransferredCallback>>,
        error_cb: Option<Box<callback::ErrorCallback>>,
        modification_cb: Option<Box<callback::ModificationCallback>>,
    ) -> Result<()> {
        let mut flags = 0;
        if transferred_cb.is_some() {
            flags |= BG_NOTIFY_JOB_TRANSFERRED;
        }
        if error_cb.is_some() {
            flags |= BG_NOTIFY_JOB_ERROR;
        }
        if modification_cb.is_some() {
            flags |= BG_NOTIFY_JOB_MODIFICATION;
        }

        callback::BackgroundCopyCallback::register(
            self,
            transferred_cb,
            error_cb,
            modification_cb,
        )?;

        unsafe { com_call!(self.0, IBackgroundCopyJob::SetNotifyFlags(flags)) }?;

        Ok(())
    }

    fn _clear_callbacks(&mut self) -> Result<()> {
        unsafe {
            com_call!(
                self.0,
                IBackgroundCopyJob::SetNotifyFlags(BG_NOTIFY_DISABLE)
            )?;

            self.set_notify_interface(ptr::null_mut() as *mut IUnknown)
        }
    }

    /// Collect the current status of the job, including errors.
    pub fn get_status(&self) -> Result<BitsJobStatus> {
        let mut state = 0;
        let mut progress = unsafe { mem::zeroed() };
        let mut error_count = 0;
        let mut times = unsafe { mem::zeroed() };

        unsafe {
            com_call!(self.0, IBackgroundCopyJob::GetState(&mut state))?;
            com_call!(self.0, IBackgroundCopyJob::GetProgress(&mut progress))?;
            com_call!(self.0, IBackgroundCopyJob::GetErrorCount(&mut error_count))?;
            com_call!(self.0, IBackgroundCopyJob::GetTimes(&mut times))?;
        }

        Ok(BitsJobStatus {
            state: BitsJobState::from(state),
            progress: BitsJobProgress {
                total_bytes: if progress.BytesTotal == BG_SIZE_UNKNOWN {
                    None
                } else {
                    Some(progress.BytesTotal)
                },
                transferred_bytes: progress.BytesTransferred,
                total_files: progress.FilesTotal,
                transferred_files: progress.FilesTransferred,
            },
            error_count,
            error: if state == BG_JOB_STATE_ERROR || state == BG_JOB_STATE_TRANSIENT_ERROR {
                let error_obj =
                    unsafe { com_call_getter!(|e| self.0, IBackgroundCopyJob::GetError(e)) }?;

                Some(BitsJob::get_error(error_obj)?)
            } else {
                None
            },
            times: BitsJobTimes {
                creation: FileTime(times.CreationTime),
                modification: FileTime(times.ModificationTime),
                transfer_completion: if times.TransferCompletionTime.dwLowDateTime == 0
                    && times.TransferCompletionTime.dwHighDateTime == 0
                {
                    None
                } else {
                    Some(FileTime(times.TransferCompletionTime))
                },
            },
        })
    }

    fn get_error(error_obj: ComRef<IBackgroundCopyError>) -> Result<BitsJobError> {
        let mut context = 0;
        let mut hresult = 0;
        unsafe {
            com_call!(
                error_obj,
                IBackgroundCopyError::GetError(&mut context, &mut hresult)
            )?;

            let language_id = DWORD::from(LANGIDFROMLCID(GetThreadLocale()));

            Ok(BitsJobError {
                context: BitsErrorContext::from(context),
                context_str: taskmem_into_lossy_string(com_call_taskmem_getter!(
                    |desc| error_obj,
                    IBackgroundCopyError::GetErrorContextDescription(language_id, desc)
                )?),
                error: hresult,
                error_str: taskmem_into_lossy_string(com_call_taskmem_getter!(
                    |desc| error_obj,
                    IBackgroundCopyError::GetErrorDescription(language_id, desc)
                )?),
            })
        }
    }

    unsafe fn set_notify_interface(&self, interface: *mut IUnknown) -> Result<()> {
        com_call!(self.0, IBackgroundCopyJob::SetNotifyInterface(interface))?;
        Ok(())
    }
}

pub struct BitsFile(ComRef<IBackgroundCopyFile>);

/// A single file in a BITS job.
///
/// This is provided for collecting the redirected remote name.
impl BitsFile {
    /// Get the remote name from which the file is being downloaded.
    ///
    /// If [`BitsJob::set_redirect_report()`](struct.BitsJob.html#method.set_redirect_report)
    /// hasn't been called on the job, this won't be
    /// updated as HTTP redirects are processed.
    pub fn get_remote_name(&self) -> Result<OsString> {
        unsafe {
            Ok(OsString::from_wide(
                com_call_taskmem_getter!(|name| self.0, IBackgroundCopyFile::GetRemoteName(name))?
                    .as_slice_until_null(),
            ))
        }
    }
}

#[cfg(test)]
mod test {
    use super::BackgroundCopyManager;
    use std::ffi::OsString;
    use std::mem;

    #[test]
    #[ignore]
    fn test_find_job() {
        let bcm = BackgroundCopyManager::connect().unwrap();
        let name = OsString::from("bits test job");
        let wrong_name = OsString::from("bits test jobbo");

        let mut job = bcm.create_job(&name).unwrap();
        let guid = job.guid().unwrap();

        assert_eq!(
            bcm.find_job_by_guid(&guid)
                .unwrap()
                .unwrap()
                .guid()
                .unwrap(),
            guid
        );
        assert_eq!(
            bcm.find_job_by_guid_and_name(&guid, &name)
                .unwrap()
                .unwrap()
                .guid()
                .unwrap(),
            guid
        );
        assert!(bcm
            .find_job_by_guid_and_name(&guid, &wrong_name)
            .unwrap()
            .is_none());

        job.cancel().unwrap();
        mem::drop(job);

        assert!(bcm.find_job_by_guid(&guid).unwrap().is_none());
        assert!(bcm
            .find_job_by_guid_and_name(&guid, &name)
            .unwrap()
            .is_none());
    }
}

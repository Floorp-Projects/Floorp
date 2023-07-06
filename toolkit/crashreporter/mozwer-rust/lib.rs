/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

extern crate ini;
extern crate winapi;

use ini::Ini;
use libc::time;
use serde::Serialize;
use serde_json::ser::to_writer;
use std::convert::TryInto;
use std::ffi::OsString;
use std::fs::{read_to_string, File};
use std::io::{BufRead, BufReader, Write};
use std::mem::{size_of, zeroed};
use std::os::windows::ffi::{OsStrExt, OsStringExt};
use std::os::windows::io::AsRawHandle;
use std::path::{Path, PathBuf};
use std::ptr::{addr_of_mut, null, null_mut};
use std::slice::from_raw_parts;
use uuid::Uuid;
use winapi::shared::basetsd::{SIZE_T, ULONG_PTR};
use winapi::shared::minwindef::{
    BOOL, BYTE, DWORD, FALSE, FILETIME, LPVOID, MAX_PATH, PBOOL, PDWORD, PULONG, TRUE, ULONG, WORD,
};
use winapi::shared::ntdef::{NTSTATUS, STRING, UNICODE_STRING};
use winapi::shared::ntstatus::STATUS_SUCCESS;
use winapi::shared::winerror::{E_UNEXPECTED, S_OK};
use winapi::um::combaseapi::CoTaskMemFree;
use winapi::um::handleapi::CloseHandle;
use winapi::um::knownfolders::FOLDERID_RoamingAppData;
use winapi::um::memoryapi::{ReadProcessMemory, WriteProcessMemory};
use winapi::um::minwinbase::{EXCEPTION_BREAKPOINT, LPTHREAD_START_ROUTINE};
use winapi::um::processthreadsapi::{
    CreateProcessW, CreateRemoteThread, GetProcessId, GetProcessTimes, GetThreadContext,
    GetThreadId, OpenProcess, OpenThread, TerminateProcess, PROCESS_INFORMATION, STARTUPINFOW,
};
use winapi::um::psapi::K32GetModuleFileNameExW;
use winapi::um::shlobj::SHGetKnownFolderPath;
use winapi::um::synchapi::WaitForSingleObject;
use winapi::um::winbase::{
    VerifyVersionInfoW, CREATE_NO_WINDOW, CREATE_UNICODE_ENVIRONMENT, NORMAL_PRIORITY_CLASS,
    WAIT_OBJECT_0,
};
use winapi::um::winnt::{
    VerSetConditionMask, CONTEXT, DWORDLONG, EXCEPTION_POINTERS, EXCEPTION_RECORD, HANDLE, HRESULT,
    LIST_ENTRY, LPOSVERSIONINFOEXW, OSVERSIONINFOEXW, PCWSTR, PEXCEPTION_POINTERS,
    PROCESS_ALL_ACCESS, PVOID, PWSTR, THREAD_GET_CONTEXT, VER_GREATER_EQUAL, VER_MAJORVERSION,
    VER_MINORVERSION, VER_SERVICEPACKMAJOR, VER_SERVICEPACKMINOR,
};
use winapi::STRUCT;

/* The following struct must be kept in sync with the identically named one in
 * nsExceptionHandler.h. There is one copy of this structure for every child
 * process and they are all stored within the main process'. WER will use it to
 * communicate with the main process when a child process is encountered. */
#[repr(C)]
struct WindowsErrorReportingData {
    wer_notify_proc: LPTHREAD_START_ROUTINE,
    child_pid: DWORD,
    minidump_name: [u8; 40],
    oom_allocation_size: usize,
}

/* The following struct must be kept in sync with the identically named one in
 * nsExceptionHandler.h. A copy of this is stored in every process and a pointer
 * to it is passed to the runtime exception module. We will read it to gather
 * information about the crashed process. */
#[repr(C)]
struct InProcessWindowsErrorReportingData {
    process_type: u32,
    oom_allocation_size_ptr: *mut usize,
}

// This value comes from GeckoProcessTypes.h
static MAIN_PROCESS_TYPE: u32 = 0;

#[no_mangle]
pub extern "C" fn OutOfProcessExceptionEventCallback(
    context: PVOID,
    exception_information: PWER_RUNTIME_EXCEPTION_INFORMATION,
    b_ownership_claimed: PBOOL,
    _wsz_event_name: PWSTR,
    _pch_size: PDWORD,
    _dw_signature_count: PDWORD,
) -> HRESULT {
    let result = out_of_process_exception_event_callback(context, exception_information);

    match result {
        Ok(_) => {
            unsafe {
                // Inform WER that we claim ownership of this crash
                *b_ownership_claimed = TRUE;
                // Make sure that the process shuts down
                TerminateProcess((*exception_information).hProcess, 1);
            }
            S_OK
        }
        Err(_) => E_UNEXPECTED,
    }
}

#[no_mangle]
pub extern "C" fn OutOfProcessExceptionEventSignatureCallback(
    _context: PVOID,
    _exception_information: PWER_RUNTIME_EXCEPTION_INFORMATION,
    _w_index: DWORD,
    _wsz_name: PWSTR,
    _ch_name: PDWORD,
    _wsz_value: PWSTR,
    _ch_value: PDWORD,
) -> HRESULT {
    S_OK
}

#[no_mangle]
pub extern "C" fn OutOfProcessExceptionEventDebuggerLaunchCallback(
    _context: PVOID,
    _exception_information: PWER_RUNTIME_EXCEPTION_INFORMATION,
    b_is_custom_debugger: PBOOL,
    _wsz_debugger_launch: PWSTR,
    _ch_debugger_launch: PDWORD,
    _b_is_debugger_autolaunch: PBOOL,
) -> HRESULT {
    unsafe {
        *b_is_custom_debugger = FALSE;
    }

    S_OK
}

type Result<T> = std::result::Result<T, ()>;

fn out_of_process_exception_event_callback(
    context: PVOID,
    exception_information: PWER_RUNTIME_EXCEPTION_INFORMATION,
) -> Result<()> {
    let exception_information = unsafe { &mut *exception_information };
    let is_fatal = exception_information.bIsFatal.to_bool();
    let mut is_ui_hang = false;
    if !is_fatal {
        'hang: {
            // Check whether this error is a hang. A hang always results in an EXCEPTION_BREAKPOINT.
            // Hangs may have an hThread/context that is unrelated to the hanging thread, so we get
            // it by searching for process windows that are hung.
            if exception_information.exceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT {
                if let Ok(thread_id) = find_hung_window_thread(exception_information.hProcess) {
                    // In the case of a hang, change the crashing thread to be the one that created
                    // the hung window.
                    //
                    // This is all best-effort, so don't return errors (just fall through to the
                    // Ok return).
                    let thread_handle = unsafe { OpenThread(THREAD_GET_CONTEXT, FALSE, thread_id) };
                    if thread_handle != std::ptr::null_mut()
                        && unsafe {
                            GetThreadContext(thread_handle, &mut exception_information.context)
                        }
                        .to_bool()
                    {
                        exception_information.hThread = thread_handle;
                        break 'hang;
                    }
                }
            }

            // A non-fatal but non-hang exception should not do anything else.
            return Ok(());
        }
        is_ui_hang = true;
    }

    let process = exception_information.hProcess;
    let application_info = ApplicationInformation::from_process(process)?;
    let wer_data = read_from_process::<InProcessWindowsErrorReportingData>(
        process,
        context as *mut InProcessWindowsErrorReportingData,
    )?;
    let startup_time = get_startup_time(process)?;
    let oom_allocation_size = get_oom_allocation_size(process, &wer_data);
    let crash_report = CrashReport::new(
        &application_info,
        startup_time,
        oom_allocation_size,
        is_ui_hang,
    );
    crash_report.write_minidump(exception_information)?;
    if wer_data.process_type == MAIN_PROCESS_TYPE {
        handle_main_process_crash(crash_report, process, &application_info)
    } else {
        handle_child_process_crash(crash_report, process)
    }
}

/// Find whether the given process has a hung window, and return the thread id related to the
/// window.
fn find_hung_window_thread(process: HANDLE) -> Result<DWORD> {
    use winapi::shared::{minwindef::LPARAM, windef::HWND};
    use winapi::um::winuser::{EnumWindows, GetWindowThreadProcessId, IsHungAppWindow};

    let process_id = get_process_id(process)?;

    struct WindowSearch {
        process_id: DWORD,
        ui_thread_id: Option<DWORD>,
    }

    let mut search = WindowSearch {
        process_id,
        ui_thread_id: None,
    };

    unsafe extern "system" fn enum_window_callback(wnd: HWND, data: LPARAM) -> BOOL {
        let data = &mut *(data as *mut WindowSearch);
        let mut window_proc_id = DWORD::default();
        let thread_id = GetWindowThreadProcessId(wnd, &mut window_proc_id);
        if thread_id != 0 && window_proc_id == data.process_id && IsHungAppWindow(wnd).to_bool() {
            data.ui_thread_id = Some(thread_id);
            FALSE
        } else {
            TRUE
        }
    }

    // Disregard the return value, we are trying for best-effort service (it's okay if ui_thread_id
    // is never set).
    unsafe { EnumWindows(Some(enum_window_callback), &mut search as *mut _ as LPARAM) };

    search.ui_thread_id.ok_or(())
}

fn handle_main_process_crash(
    crash_report: CrashReport,
    process: HANDLE,
    application_information: &ApplicationInformation,
) -> Result<()> {
    crash_report.write_extra_file()?;
    crash_report.write_event_file()?;

    let mut environment = read_environment_block(process)?;
    launch_crash_reporter_client(
        &application_information.install_path,
        &mut environment,
        &crash_report,
    );

    Ok(())
}

fn handle_child_process_crash(crash_report: CrashReport, child_process: HANDLE) -> Result<()> {
    let command_line = read_command_line(child_process)?;
    let (parent_pid, data_ptr) = parse_child_data(&command_line)?;

    let parent_process = get_process_handle(parent_pid)?;
    let mut wer_data: WindowsErrorReportingData = read_from_process(parent_process, data_ptr)?;
    wer_data.child_pid = get_process_id(child_process)?;
    wer_data.minidump_name = crash_report.get_minidump_name();
    wer_data.oom_allocation_size = crash_report.oom_allocation_size;
    let wer_notify_proc = wer_data.wer_notify_proc;
    write_to_process(parent_process, wer_data, data_ptr)?;
    notify_main_process(parent_process, wer_notify_proc, data_ptr)
}

fn read_from_process<T>(process: HANDLE, data_ptr: *mut T) -> Result<T> {
    let mut data: T = unsafe { zeroed() };
    unsafe {
        ReadProcessMemory(
            process,
            data_ptr as *mut _,
            addr_of_mut!(data) as *mut _,
            size_of::<T>() as SIZE_T,
            null_mut(),
        )
    }
    .if_true(data)
}

fn read_array_from_process<T: Clone + Default>(
    process: HANDLE,
    data_ptr: LPVOID,
    count: usize,
) -> Result<Vec<T>> {
    let mut array = vec![Default::default(); count];
    let size = size_of::<T>() as SIZE_T;
    let size = size.checked_mul(count).ok_or(())?;
    unsafe {
        ReadProcessMemory(
            process,
            data_ptr,
            array.as_mut_ptr() as *mut _,
            size,
            null_mut(),
        )
    }
    .if_true(array)
}

fn write_to_process<T>(process: HANDLE, mut data: T, data_ptr: *mut T) -> Result<()> {
    unsafe {
        WriteProcessMemory(
            process,
            data_ptr as LPVOID,
            addr_of_mut!(data) as *mut _,
            size_of::<T>() as SIZE_T,
            null_mut(),
        )
    }
    .success()
}

fn notify_main_process(
    process: HANDLE,
    wer_notify_proc: LPTHREAD_START_ROUTINE,
    data_ptr: *mut WindowsErrorReportingData,
) -> Result<()> {
    let thread = unsafe {
        CreateRemoteThread(
            process,
            null_mut(),
            0,
            wer_notify_proc,
            data_ptr as LPVOID,
            0,
            null_mut(),
        )
    };

    if thread == null_mut() {
        return Err(());
    }

    // Don't wait forever as we want the process to get killed eventually
    let res = unsafe { WaitForSingleObject(thread, 5000) };
    if res != WAIT_OBJECT_0 {
        return Err(());
    }

    unsafe { CloseHandle(thread) }.success()
}

fn get_startup_time(process: HANDLE) -> Result<u64> {
    let mut create_time: FILETIME = Default::default();
    let mut exit_time: FILETIME = Default::default();
    let mut kernel_time: FILETIME = Default::default();
    let mut user_time: FILETIME = Default::default();
    unsafe {
        if GetProcessTimes(
            process,
            &mut create_time as *mut _,
            &mut exit_time as *mut _,
            &mut kernel_time as *mut _,
            &mut user_time as *mut _,
        ) == 0
        {
            return Err(());
        }
    }
    let start_time_in_ticks =
        ((create_time.dwHighDateTime as u64) << 32) + create_time.dwLowDateTime as u64;
    let windows_tick: u64 = 10000000;
    let sec_to_unix_epoch = 11644473600;
    Ok((start_time_in_ticks / windows_tick) - sec_to_unix_epoch)
}

fn get_oom_allocation_size(
    process: HANDLE,
    wer_data: &InProcessWindowsErrorReportingData,
) -> usize {
    read_from_process(process, wer_data.oom_allocation_size_ptr).unwrap_or(0)
}

fn parse_child_data(command_line: &str) -> Result<(DWORD, *mut WindowsErrorReportingData)> {
    let mut itr = command_line.rsplit(' ');
    let address = itr.nth(1).ok_or(())?;
    let address = usize::from_str_radix(address, 16).map_err(|_err| (()))?;
    let address = address as *mut WindowsErrorReportingData;
    let parent_pid = itr.nth(2).ok_or(())?;
    let parent_pid = u32::from_str_radix(parent_pid, 10).map_err(|_err| (()))?;

    Ok((parent_pid, address))
}

fn get_process_id(process: HANDLE) -> Result<DWORD> {
    match unsafe { GetProcessId(process) } {
        0 => Err(()),
        pid => Ok(pid),
    }
}

fn get_process_handle(pid: DWORD) -> Result<HANDLE> {
    let handle = unsafe { OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid) };
    if handle != null_mut() {
        Ok(handle)
    } else {
        Err(())
    }
}

fn launch_crash_reporter_client(
    install_path: &Path,
    environment: &mut Vec<u16>,
    crash_report: &CrashReport,
) {
    // Prepare the command line
    let client_path = install_path.join("crashreporter.exe");

    let mut cmd_line = OsString::from("\"");
    cmd_line.push(client_path);
    cmd_line.push("\" \"");
    cmd_line.push(crash_report.get_minidump_path());
    cmd_line.push("\"\0");
    let mut cmd_line: Vec<u16> = cmd_line.encode_wide().collect();

    let mut pi: PROCESS_INFORMATION = Default::default();
    let mut si = STARTUPINFOW {
        cb: size_of::<STARTUPINFOW>().try_into().unwrap(),
        ..Default::default()
    };

    unsafe {
        if CreateProcessW(
            null_mut(),
            cmd_line.as_mut_ptr(),
            null_mut(),
            null_mut(),
            FALSE,
            NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
            environment.as_mut_ptr() as *mut _,
            null_mut(),
            &mut si,
            &mut pi,
        ) != 0
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}

#[derive(Debug)]
struct ApplicationData {
    vendor: Option<String>,
    name: String,
    version: String,
    build_id: String,
    product_id: String,
    server_url: String,
}

impl ApplicationData {
    fn load_from_disk(install_path: &Path) -> Result<ApplicationData> {
        let ini_path = ApplicationData::get_path(install_path);
        let conf = Ini::load_from_file(ini_path).map_err(|_e| ())?;

        // Parse the "App" section
        let app_section = conf.section(Some("App")).ok_or(())?;
        let vendor = app_section.get("Vendor").map(|s| s.to_owned());
        let name = app_section.get("Name").ok_or(())?.to_owned();
        let version = app_section.get("Version").ok_or(())?.to_owned();
        let build_id = app_section.get("BuildID").ok_or(())?.to_owned();
        let product_id = app_section.get("ID").ok_or(())?.to_owned();

        // Parse the "Crash Reporter" section
        let crash_reporter_section = conf.section(Some("Crash Reporter")).ok_or(())?;
        let server_url = crash_reporter_section
            .get("ServerURL")
            .ok_or(())?
            .to_owned();

        // InstallTime<build_id>

        Ok(ApplicationData {
            vendor,
            name,
            version,
            build_id,
            product_id,
            server_url,
        })
    }

    fn get_path(install_path: &Path) -> PathBuf {
        install_path.join("application.ini")
    }
}

#[derive(Serialize)]
#[allow(non_snake_case)]
struct Annotations {
    BuildID: String,
    CrashTime: String,
    InstallTime: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    Hang: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    OOMAllocationSize: Option<String>,
    ProductID: String,
    ProductName: String,
    ReleaseChannel: String,
    ServerURL: String,
    StartupTime: String,
    UptimeTS: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    Vendor: Option<String>,
    Version: String,
    WindowsErrorReporting: String,
}

impl Annotations {
    fn from_application_data(
        application_data: &ApplicationData,
        release_channel: String,
        install_time: String,
        crash_time: u64,
        startup_time: u64,
        oom_allocation_size: usize,
        ui_hang: bool,
    ) -> Annotations {
        let oom_allocation_size = if oom_allocation_size != 0 {
            Some(oom_allocation_size.to_string())
        } else {
            None
        };
        Annotations {
            BuildID: application_data.build_id.clone(),
            CrashTime: crash_time.to_string(),
            InstallTime: install_time,
            Hang: ui_hang.then(|| "ui".to_string()),
            OOMAllocationSize: oom_allocation_size,
            ProductID: application_data.product_id.clone(),
            ProductName: application_data.name.clone(),
            ReleaseChannel: release_channel,
            ServerURL: application_data.server_url.clone(),
            StartupTime: startup_time.to_string(),
            UptimeTS: (crash_time - startup_time).to_string() + ".0",
            Vendor: application_data.vendor.clone(),
            Version: application_data.version.clone(),
            WindowsErrorReporting: "1".to_string(),
        }
    }
}

/// Encapsulates the information about the application that crashed. This includes the install path as well as version information
struct ApplicationInformation {
    install_path: PathBuf,
    application_data: ApplicationData,
    release_channel: String,
    crash_reports_dir: PathBuf,
    install_time: String,
}

impl ApplicationInformation {
    fn from_process(process: HANDLE) -> Result<ApplicationInformation> {
        let mut install_path = ApplicationInformation::get_application_path(process)?;
        install_path.pop();
        let application_data = ApplicationData::load_from_disk(install_path.as_ref())?;
        let release_channel = ApplicationInformation::get_release_channel(install_path.as_ref())?;
        let crash_reports_dir = ApplicationInformation::get_crash_reports_dir(&application_data)?;
        let install_time = ApplicationInformation::get_install_time(
            &crash_reports_dir,
            &application_data.build_id,
        )?;

        Ok(ApplicationInformation {
            install_path,
            application_data,
            release_channel,
            crash_reports_dir,
            install_time,
        })
    }

    fn get_application_path(process: HANDLE) -> Result<PathBuf> {
        let mut path: [u16; MAX_PATH + 1] = [0; MAX_PATH + 1];
        unsafe {
            let res = K32GetModuleFileNameExW(
                process,
                null_mut(),
                (&mut path).as_mut_ptr(),
                (MAX_PATH + 1) as DWORD,
            );

            if res == 0 {
                return Err(());
            }

            let application_path = PathBuf::from(OsString::from_wide(&path[0..res as usize]));
            Ok(application_path)
        }
    }

    fn get_release_channel(install_path: &Path) -> Result<String> {
        let channel_prefs =
            File::open(install_path.join("defaults/pref/channel-prefs.js")).map_err(|_e| ())?;
        let lines = BufReader::new(channel_prefs).lines();
        let line = lines
            .filter_map(std::result::Result::ok)
            .find(|line| line.contains("app.update.channel"))
            .ok_or(())?;
        line.split("\"").nth(3).map(|s| s.to_string()).ok_or(())
    }

    fn get_crash_reports_dir(application_data: &ApplicationData) -> Result<PathBuf> {
        let mut psz_path: PWSTR = null_mut();
        unsafe {
            let res = SHGetKnownFolderPath(
                &FOLDERID_RoamingAppData as *const _,
                0,
                null_mut(),
                &mut psz_path as *mut _,
            );

            if res == S_OK {
                let mut len = 0;
                while psz_path.offset(len).read() != 0 {
                    len += 1;
                }
                let str = OsString::from_wide(from_raw_parts(psz_path, len as usize));
                CoTaskMemFree(psz_path as _);
                let mut path = PathBuf::from(str);
                if let Some(vendor) = &application_data.vendor {
                    path.push(vendor);
                }
                path.push(&application_data.name);
                path.push("Crash Reports");
                Ok(path)
            } else {
                Err(())
            }
        }
    }

    fn get_install_time(crash_reports_path: &Path, build_id: &str) -> Result<String> {
        let file_name = "InstallTime".to_owned() + build_id;
        let file_path = crash_reports_path.join(file_name);
        read_to_string(file_path).map_err(|_e| ())
    }
}

struct CrashReport {
    uuid: String,
    crash_reports_path: PathBuf,
    release_channel: String,
    annotations: Annotations,
    crash_time: u64,
    oom_allocation_size: usize,
}

impl CrashReport {
    fn new(
        application_information: &ApplicationInformation,
        startup_time: u64,
        oom_allocation_size: usize,
        ui_hang: bool,
    ) -> CrashReport {
        let uuid = Uuid::new_v4()
            .as_hyphenated()
            .encode_lower(&mut Uuid::encode_buffer())
            .to_owned();
        let crash_reports_path = application_information.crash_reports_dir.clone();
        let crash_time: u64 = unsafe { time(null_mut()) as u64 };
        let annotations = Annotations::from_application_data(
            &application_information.application_data,
            application_information.release_channel.clone(),
            application_information.install_time.clone(),
            crash_time,
            startup_time,
            oom_allocation_size,
            ui_hang,
        );
        CrashReport {
            uuid,
            crash_reports_path,
            release_channel: application_information.release_channel.clone(),
            annotations,
            crash_time,
            oom_allocation_size,
        }
    }

    fn is_nightly(&self) -> bool {
        self.release_channel == "nightly" || self.release_channel == "default"
    }

    fn get_minidump_type(&self) -> MINIDUMP_TYPE {
        let mut minidump_type = MiniDumpWithFullMemoryInfo | MiniDumpWithUnloadedModules;
        if self.is_nightly() {
            // This is Nightly only because this doubles the size of minidumps based
            // on the experimental data.
            minidump_type = minidump_type | MiniDumpWithProcessThreadData;

            // dbghelp.dll on Win7 can't handle overlapping memory regions so we only
            // enable this feature on Win8 or later.
            if is_windows8_or_later() {
                // This allows us to examine heap objects referenced from stack objects
                // at the cost of further doubling the size of minidumps.
                minidump_type = minidump_type | MiniDumpWithIndirectlyReferencedMemory
            }
        }
        minidump_type
    }

    fn get_pending_path(&self) -> PathBuf {
        self.crash_reports_path.join("pending")
    }

    fn get_events_path(&self) -> PathBuf {
        self.crash_reports_path.join("events")
    }

    fn get_minidump_path(&self) -> PathBuf {
        self.get_pending_path().join(self.uuid.to_string() + ".dmp")
    }

    fn get_minidump_name(&self) -> [u8; 40] {
        let bytes = (self.uuid.to_string() + ".dmp").into_bytes();
        bytes[0..40].try_into().unwrap()
    }

    fn get_extra_file_path(&self) -> PathBuf {
        self.get_pending_path()
            .join(self.uuid.to_string() + ".extra")
    }

    fn get_event_file_path(&self) -> PathBuf {
        self.get_events_path().join(self.uuid.to_string())
    }

    fn write_minidump(
        &self,
        exception_information: PWER_RUNTIME_EXCEPTION_INFORMATION,
    ) -> Result<()> {
        let minidump_path = self.get_minidump_path();
        let minidump_file = File::create(minidump_path).map_err(|_e| ())?;
        let minidump_type: MINIDUMP_TYPE = self.get_minidump_type();

        unsafe {
            let mut exception_pointers = EXCEPTION_POINTERS {
                ExceptionRecord: &mut ((*exception_information).exceptionRecord),
                ContextRecord: &mut ((*exception_information).context),
            };

            let mut exception = MINIDUMP_EXCEPTION_INFORMATION {
                ThreadId: GetThreadId((*exception_information).hThread),
                ExceptionPointers: &mut exception_pointers,
                ClientPointers: FALSE,
            };

            MiniDumpWriteDump(
                (*exception_information).hProcess,
                get_process_id((*exception_information).hProcess)?,
                minidump_file.as_raw_handle() as _,
                minidump_type,
                &mut exception,
                /* userStream */ null(),
                /* callback */ null(),
            )
            .success()
        }
    }

    fn write_extra_file(&self) -> Result<()> {
        let extra_file = File::create(self.get_extra_file_path()).map_err(|_e| ())?;
        to_writer(extra_file, &self.annotations).map_err(|_e| ())
    }

    fn write_event_file(&self) -> Result<()> {
        let mut event_file = File::create(self.get_event_file_path()).map_err(|_e| ())?;
        writeln!(event_file, "crash.main.3").map_err(|_e| ())?;
        writeln!(event_file, "{}", self.crash_time).map_err(|_e| ())?;
        writeln!(event_file, "{}", self.uuid).map_err(|_e| ())?;
        to_writer(event_file, &self.annotations).map_err(|_e| ())
    }
}

fn is_windows8_or_later() -> bool {
    let mut info = OSVERSIONINFOEXW {
        dwOSVersionInfoSize: size_of::<OSVERSIONINFOEXW>().try_into().unwrap(),
        dwMajorVersion: 6,
        dwMinorVersion: 2,
        ..Default::default()
    };

    unsafe {
        let mut mask: DWORDLONG = 0;
        mask = VerSetConditionMask(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
        mask = VerSetConditionMask(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
        mask = VerSetConditionMask(mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
        mask = VerSetConditionMask(mask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);

        VerifyVersionInfoW(
            &mut info as LPOSVERSIONINFOEXW,
            VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
            mask,
        )
        .to_bool()
    }
}

trait WinBool: Sized {
    fn to_bool(self) -> bool;
    fn if_true<T>(self, value: T) -> Result<T> {
        if self.to_bool() {
            Ok(value)
        } else {
            Err(())
        }
    }
    fn success(self) -> Result<()> {
        self.if_true(())
    }
}

impl WinBool for BOOL {
    fn to_bool(self) -> bool {
        match self {
            FALSE => false,
            _ => true,
        }
    }
}

fn read_environment_block(process: HANDLE) -> Result<Vec<u16>> {
    let upp = read_user_process_parameters(process)?;

    // Read the environment
    let buffer = upp.Environment;
    let length = upp.EnvironmentSize;
    let count = length as usize / 2;
    read_array_from_process::<u16>(process, buffer, count)
}

fn read_command_line(process: HANDLE) -> Result<String> {
    let upp = read_user_process_parameters(process)?;

    // Read the command-line
    let buffer = upp.CommandLine.Buffer;
    let length = upp.CommandLine.Length;
    let count = (length as usize) / 2;
    let command_line = read_array_from_process::<u16>(process, buffer as *mut _, count)?;
    String::from_utf16(&command_line).map_err(|_err| ())
}

fn read_user_process_parameters(process: HANDLE) -> Result<RTL_USER_PROCESS_PARAMETERS> {
    let mut pbi: PROCESS_BASIC_INFORMATION = unsafe { zeroed() };
    let mut length: ULONG = 0;
    let result = unsafe {
        NtQueryInformationProcess(
            process,
            ProcessBasicInformation,
            &mut pbi as *mut _ as _,
            size_of::<PROCESS_BASIC_INFORMATION>().try_into().unwrap(),
            &mut length,
        )
    };

    if result != STATUS_SUCCESS {
        return Err(());
    }

    // Read the process environment block
    let peb: PEB = read_from_process(process, pbi.PebBaseAddress)?;

    // Read the user process parameters
    read_from_process::<RTL_USER_PROCESS_PARAMETERS>(process, peb.ProcessParameters)
}

/******************************************************************************
 * The stuff below should be migrated to the winapi crate, see bug 1696414    *
 ******************************************************************************/

// we can't use winapi's ENUM macro directly because it doesn't support
// attributes, so let's define this one here until we migrate this code
macro_rules! ENUM {
    {enum $name:ident { $($variant:ident = $value:expr,)+ }} => {
        #[allow(non_camel_case_types)] pub type $name = u32;
        $(#[allow(non_upper_case_globals)] pub const $variant: $name = $value;)+
    };
}

// winapi doesn't export the FN macro, so we duplicate it here
macro_rules! FN {
    (stdcall $func:ident($($t:ty,)*) -> $ret:ty) => (
        #[allow(non_camel_case_types)] pub type $func = Option<unsafe extern "system" fn($($t,)*) -> $ret>;
    );
    (stdcall $func:ident($($p:ident: $t:ty,)*) -> $ret:ty) => (
        #[allow(non_camel_case_types)] pub type $func = Option<unsafe extern "system" fn($($p: $t,)*) -> $ret>;
    );
}

// From um/WerApi.h

STRUCT! {#[allow(non_snake_case)] struct WER_RUNTIME_EXCEPTION_INFORMATION
{
    dwSize: DWORD,
    hProcess: HANDLE,
    hThread: HANDLE,
    exceptionRecord: EXCEPTION_RECORD,
    context: CONTEXT,
    pwszReportId: PCWSTR,
    bIsFatal: BOOL,
    dwReserved: DWORD,
}}

#[allow(non_camel_case_types)]
pub type PWER_RUNTIME_EXCEPTION_INFORMATION = *mut WER_RUNTIME_EXCEPTION_INFORMATION;

// From minidumpapiset.hProcess

STRUCT! {#[allow(non_snake_case)] #[repr(packed(4))] struct MINIDUMP_EXCEPTION_INFORMATION {
    ThreadId: DWORD,
    ExceptionPointers: PEXCEPTION_POINTERS,
    ClientPointers: BOOL,
}}

#[allow(non_camel_case_types)]
pub type PMINIDUMP_EXCEPTION_INFORMATION = *mut MINIDUMP_EXCEPTION_INFORMATION;

ENUM! { enum MINIDUMP_TYPE {
    MiniDumpNormal                         = 0x00000000,
    MiniDumpWithDataSegs                   = 0x00000001,
    MiniDumpWithFullMemory                 = 0x00000002,
    MiniDumpWithHandleData                 = 0x00000004,
    MiniDumpFilterMemory                   = 0x00000008,
    MiniDumpScanMemory                     = 0x00000010,
    MiniDumpWithUnloadedModules            = 0x00000020,
    MiniDumpWithIndirectlyReferencedMemory = 0x00000040,
    MiniDumpFilterModulePaths              = 0x00000080,
    MiniDumpWithProcessThreadData          = 0x00000100,
    MiniDumpWithPrivateReadWriteMemory     = 0x00000200,
    MiniDumpWithoutOptionalData            = 0x00000400,
    MiniDumpWithFullMemoryInfo             = 0x00000800,
    MiniDumpWithThreadInfo                 = 0x00001000,
    MiniDumpWithCodeSegs                   = 0x00002000,
    MiniDumpWithoutAuxiliaryState          = 0x00004000,
    MiniDumpWithFullAuxiliaryState         = 0x00008000,
    MiniDumpWithPrivateWriteCopyMemory     = 0x00010000,
    MiniDumpIgnoreInaccessibleMemory       = 0x00020000,
    MiniDumpWithTokenInformation           = 0x00040000,
    MiniDumpWithModuleHeaders              = 0x00080000,
    MiniDumpFilterTriage                   = 0x00100000,
    MiniDumpWithAvxXStateContext           = 0x00200000,
    MiniDumpWithIptTrace                   = 0x00400000,
    MiniDumpScanInaccessiblePartialPages   = 0x00800000,
    MiniDumpValidTypeFlags                 = 0x00ffffff,
}}

// We don't actually need the following three structs so we use placeholders
STRUCT! {#[allow(non_snake_case)] struct MINIDUMP_CALLBACK_INPUT {
    dummy: u32,
}}

#[allow(non_camel_case_types)]
pub type PMINIDUMP_CALLBACK_INPUT = *const MINIDUMP_CALLBACK_INPUT;

STRUCT! {#[allow(non_snake_case)] struct MINIDUMP_USER_STREAM_INFORMATION {
    dummy: u32,
}}

#[allow(non_camel_case_types)]
pub type PMINIDUMP_USER_STREAM_INFORMATION = *const MINIDUMP_USER_STREAM_INFORMATION;

STRUCT! {#[allow(non_snake_case)] struct MINIDUMP_CALLBACK_OUTPUT {
    dummy: u32,
}}

#[allow(non_camel_case_types)]
pub type PMINIDUMP_CALLBACK_OUTPUT = *const MINIDUMP_CALLBACK_OUTPUT;

// MiniDumpWriteDump() function and structs
FN! {stdcall MINIDUMP_CALLBACK_ROUTINE(
CallbackParam: PVOID,
CallbackInput: PMINIDUMP_CALLBACK_INPUT,
CallbackOutput: PMINIDUMP_CALLBACK_OUTPUT,
) -> BOOL}

STRUCT! {#[allow(non_snake_case)] #[repr(packed(4))] struct MINIDUMP_CALLBACK_INFORMATION {
    CallbackRoutine: MINIDUMP_CALLBACK_ROUTINE,
    CallbackParam: PVOID,
}}

#[allow(non_camel_case_types)]
pub type PMINIDUMP_CALLBACK_INFORMATION = *const MINIDUMP_CALLBACK_INFORMATION;

extern "system" {
    pub fn MiniDumpWriteDump(
        hProcess: HANDLE,
        ProcessId: DWORD,
        hFile: HANDLE,
        DumpType: MINIDUMP_TYPE,
        Exceptionparam: PMINIDUMP_EXCEPTION_INFORMATION,
        UserStreamParam: PMINIDUMP_USER_STREAM_INFORMATION,
        CallbackParam: PMINIDUMP_CALLBACK_INFORMATION,
    ) -> BOOL;
}

// From um/winternl.h

STRUCT! {#[allow(non_snake_case)] struct PEB_LDR_DATA {
    Reserved1: [BYTE; 8],
    Reserved2: [PVOID; 3],
    InMemoryOrderModuleList: LIST_ENTRY,
}}

#[allow(non_camel_case_types)]
pub type PPEB_LDR_DATA = *mut PEB_LDR_DATA;

STRUCT! {#[allow(non_snake_case)] struct RTL_DRIVE_LETTER_CURDIR {
    Flags: WORD,
    Length: WORD,
    TimeStamp: ULONG,
    DosPath: STRING,
}}

STRUCT! {#[allow(non_snake_case)] struct RTL_USER_PROCESS_PARAMETERS {
    Reserved1: [BYTE; 16],
    Reserved2: [PVOID; 10],
    ImagePathName: UNICODE_STRING,
    CommandLine: UNICODE_STRING,
    // Everything below this point is undocumented
    Environment: PVOID,
    StartingX: ULONG,
    StartingY: ULONG,
    CountX: ULONG,
    CountY: ULONG,
    CountCharsX: ULONG,
    CountCharsY: ULONG,
    FillAttribute: ULONG,
    WindowFlags: ULONG,
    ShowWindowFlags: ULONG,
    WindowTitle: UNICODE_STRING,
    DesktopInfo: UNICODE_STRING,
    ShellInfo: UNICODE_STRING,
    RuntimeData: UNICODE_STRING,
    CurrentDirectores: [RTL_DRIVE_LETTER_CURDIR; 32],
    EnvironmentSize: ULONG,
}}

#[allow(non_camel_case_types)]
pub type PRTL_USER_PROCESS_PARAMETERS = *mut RTL_USER_PROCESS_PARAMETERS;

FN! {stdcall PPS_POST_PROCESS_INIT_ROUTINE() -> ()}

STRUCT! {#[allow(non_snake_case)] struct PEB {
    Reserved1: [BYTE; 2],
    BeingDebugged: BYTE,
    Reserved2: [BYTE; 1],
    Reserved3: [PVOID; 2],
    Ldr: PPEB_LDR_DATA,
    ProcessParameters: PRTL_USER_PROCESS_PARAMETERS,
    Reserved4: [PVOID; 3],
    AtlThunkSListPtr: PVOID,
    Reserved5: PVOID,
    Reserved6: ULONG,
    Reserved7: PVOID,
    Reserved8: ULONG,
    AtlThunkSListPtr32: ULONG,
    Reserved9: [PVOID; 45],
    Reserved10: [BYTE; 96],
    PostProcessInitRoutine: PPS_POST_PROCESS_INIT_ROUTINE,
    Reserved11: [BYTE; 128],
    Reserved12: [PVOID; 1],
    SessionId: ULONG,
}}

#[allow(non_camel_case_types)]
pub type PPEB = *mut PEB;

STRUCT! {#[allow(non_snake_case)] struct PROCESS_BASIC_INFORMATION {
    Reserved1: PVOID,
    PebBaseAddress: PPEB,
    Reserved2: [PVOID; 2],
    UniqueProcessId: ULONG_PTR,
    Reserved3: PVOID,
}}

ENUM! {enum PROCESSINFOCLASS {
    ProcessBasicInformation = 0,
    ProcessDebugPort = 7,
    ProcessWow64Information = 26,
    ProcessImageFileName = 27,
    ProcessBreakOnTermination = 29,
}}

extern "system" {
    pub fn NtQueryInformationProcess(
        ProcessHandle: HANDLE,
        ProcessInformationClass: PROCESSINFOCLASS,
        ProcessInformation: PVOID,
        ProcessInformationLength: ULONG,
        ReturnLength: PULONG,
    ) -> NTSTATUS;
}

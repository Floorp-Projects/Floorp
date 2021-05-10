use minidump::*;
use minidump_common::format::{GUID, MINIDUMP_STREAM_TYPE::*};
use minidump_writer_linux::app_memory::AppMemory;
#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
use minidump_writer_linux::crash_context::fpstate_t;
use minidump_writer_linux::crash_context::CrashContext;
use minidump_writer_linux::errors::*;
use minidump_writer_linux::linux_ptrace_dumper::LinuxPtraceDumper;
use minidump_writer_linux::maps_reader::{MappingEntry, MappingInfo, SystemMappingInfo};
use minidump_writer_linux::minidump_writer::MinidumpWriter;
use minidump_writer_linux::thread_info::Pid;
use nix::errno::Errno;
use nix::sys::signal::Signal;
use std::convert::TryInto;
use std::io::{BufRead, BufReader};
use std::os::unix::process::ExitStatusExt;
use std::process::{Command, Stdio};
use std::str::FromStr;

mod common;
use common::*;

#[derive(Debug, PartialEq)]
enum Context {
    With,
    Without,
}

#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
fn get_ucontext() -> Result<libc::ucontext_t> {
    let mut context = std::mem::MaybeUninit::<libc::ucontext_t>::uninit();
    let res = unsafe { libc::getcontext(context.as_mut_ptr()) };
    Errno::result(res)?;
    unsafe { Ok(context.assume_init()) }
}

#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
fn get_crash_context(tid: Pid) -> CrashContext {
    let siginfo: libc::siginfo_t = unsafe { std::mem::zeroed() };
    let context = get_ucontext().expect("Failed to get ucontext");
    let float_state: fpstate_t = unsafe { std::mem::zeroed() };
    CrashContext {
        siginfo,
        tid,
        context,
        float_state,
    }
}

fn test_write_dump_helper(context: Context) {
    let num_of_threads = 3;
    let mut child = start_child_and_wait_for_threads(num_of_threads);
    let pid = child.id() as i32;

    let mut tmpfile = tempfile::Builder::new()
        .prefix("write_dump")
        .tempfile()
        .unwrap();

    let mut tmp = MinidumpWriter::new(pid, pid);
    #[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
    if context == Context::With {
        let crash_context = get_crash_context(pid);
        tmp.set_crash_context(crash_context);
    }
    let in_memory_buffer = tmp.dump(&mut tmpfile).expect("Could not write minidump");
    child.kill().expect("Failed to kill process");

    // Reap child
    let waitres = child.wait().expect("Failed to wait for child");
    let status = waitres.signal().expect("Child did not die due to signal");
    assert_eq!(waitres.code(), None);
    assert_eq!(status, Signal::SIGKILL as i32);

    let meta = std::fs::metadata(tmpfile.path()).expect("Couldn't get metadata for tempfile");
    assert!(meta.len() > 0);

    let mem_slice = std::fs::read(tmpfile.path()).expect("Failed to minidump");
    assert_eq!(mem_slice.len(), in_memory_buffer.len());
    assert_eq!(mem_slice, in_memory_buffer);
}
#[test]
fn test_write_dump() {
    test_write_dump_helper(Context::Without)
}
#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
#[test]
fn test_write_dump_with_context() {
    test_write_dump_helper(Context::With)
}

fn test_write_and_read_dump_from_parent_helper(context: Context) {
    let mut child = start_child_and_return("spawn_mmap_wait");
    let pid = child.id() as i32;

    let mut tmpfile = tempfile::Builder::new()
        .prefix("write_and_read_dump")
        .tempfile()
        .unwrap();

    let mut f = BufReader::new(child.stdout.as_mut().expect("Can't open stdout"));
    let mut buf = String::new();
    let _ = f
        .read_line(&mut buf)
        .expect("Couldn't read address provided by child");
    let mut output = buf.split_whitespace();
    let mmap_addr = usize::from_str(output.next().unwrap()).expect("unable to parse mmap_addr");
    let memory_size = usize::from_str(output.next().unwrap()).expect("unable to parse memory_size");
    // Add information about the mapped memory.
    let mapping = MappingInfo {
        start_address: mmap_addr,
        size: memory_size,
        offset: 0,
        executable: false,
        name: Some("a fake mapping".to_string()),
        system_mapping_info: SystemMappingInfo {
            start_address: mmap_addr,
            end_address: mmap_addr + memory_size,
        },
    };

    let identifier = vec![
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
        0xFF,
    ];
    let entry = MappingEntry {
        mapping,
        identifier,
    };

    let mut tmp = MinidumpWriter::new(pid, pid);
    #[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
    if context == Context::With {
        let crash_context = get_crash_context(pid);
        tmp.set_crash_context(crash_context);
    }

    tmp.set_user_mapping_list(vec![entry])
        .dump(&mut tmpfile)
        .expect("Could not write minidump");

    child.kill().expect("Failed to kill process");
    // Reap child
    let waitres = child.wait().expect("Failed to wait for child");
    let status = waitres.signal().expect("Child did not die due to signal");
    assert_eq!(waitres.code(), None);
    assert_eq!(status, Signal::SIGKILL as i32);

    let dump = Minidump::read_path(tmpfile.path()).expect("Failed to read minidump");
    let module_list: MinidumpModuleList = dump
        .get_stream()
        .expect("Couldn't find stream MinidumpModuleList");
    let module = module_list
        .module_at_address(mmap_addr as u64)
        .expect("Couldn't find user mapping module");
    assert_eq!(module.base_address(), mmap_addr as u64);
    assert_eq!(module.size(), memory_size as u64);
    assert_eq!(module.code_file(), "a fake mapping");
    assert_eq!(
        module.debug_identifier(),
        Some("33221100554477668899AABBCCDDEEFF0".into())
    );

    let _: MinidumpException = dump.get_stream().expect("Couldn't find MinidumpException");
    let _: MinidumpThreadList = dump.get_stream().expect("Couldn't find MinidumpThreadList");
    let _: MinidumpMemoryList = dump.get_stream().expect("Couldn't find MinidumpMemoryList");
    let _: MinidumpException = dump.get_stream().expect("Couldn't find MinidumpException");
    let _: MinidumpSystemInfo = dump.get_stream().expect("Couldn't find MinidumpSystemInfo");
    let _ = dump
        .get_raw_stream(LinuxCpuInfo)
        .expect("Couldn't find LinuxCpuInfo");
    let _ = dump
        .get_raw_stream(LinuxProcStatus)
        .expect("Couldn't find LinuxProcStatus");
    let _ = dump
        .get_raw_stream(LinuxCmdLine)
        .expect("Couldn't find LinuxCmdLine");
    let _ = dump
        .get_raw_stream(LinuxEnviron)
        .expect("Couldn't find LinuxEnviron");
    let _ = dump
        .get_raw_stream(LinuxAuxv)
        .expect("Couldn't find LinuxAuxv");
    let _ = dump
        .get_raw_stream(LinuxMaps)
        .expect("Couldn't find LinuxMaps");
    let _ = dump
        .get_raw_stream(LinuxDsoDebug)
        .expect("Couldn't find LinuxDsoDebug");
}
#[test]
fn test_write_and_read_dump_from_parent() {
    test_write_and_read_dump_from_parent_helper(Context::Without)
}
#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
#[test]
fn test_write_and_read_dump_from_parent_with_context() {
    test_write_and_read_dump_from_parent_helper(Context::With)
}

fn test_write_with_additional_memory_helper(context: Context) {
    let mut child = start_child_and_return("spawn_alloc_wait");
    let pid = child.id() as i32;

    let mut tmpfile = tempfile::Builder::new()
        .prefix("additional_memory")
        .tempfile()
        .unwrap();

    let mut f = BufReader::new(child.stdout.as_mut().expect("Can't open stdout"));
    let mut buf = String::new();
    let _ = f
        .read_line(&mut buf)
        .expect("Couldn't read address provided by child");
    let mut output = buf.split_whitespace();
    let memory_addr = usize::from_str_radix(output.next().unwrap().trim_start_matches("0x"), 16)
        .expect("unable to parse mmap_addr");
    let memory_size = usize::from_str(output.next().unwrap()).expect("unable to parse memory_size");

    let app_memory = AppMemory {
        ptr: memory_addr,
        length: memory_size,
    };

    let mut tmp = MinidumpWriter::new(pid, pid);
    #[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
    if context == Context::With {
        let crash_context = get_crash_context(pid);
        tmp.set_crash_context(crash_context);
    }

    tmp.set_app_memory(vec![app_memory])
        .dump(&mut tmpfile)
        .expect("Could not write minidump");

    child.kill().expect("Failed to kill process");
    // Reap child
    let waitres = child.wait().expect("Failed to wait for child");
    let status = waitres.signal().expect("Child did not die due to signal");
    assert_eq!(waitres.code(), None);
    assert_eq!(status, Signal::SIGKILL as i32);

    // Read dump file and check its contents
    let dump = Minidump::read_path(tmpfile.path()).expect("Failed to read minidump");

    let section: MinidumpMemoryList = dump.get_stream().expect("Couldn't find MinidumpMemoryList");
    let region = section
        .memory_at_address(memory_addr as u64)
        .expect("Couldn't find memory region");

    assert_eq!(region.base_address, memory_addr as u64);
    assert_eq!(region.size, memory_size as u64);

    let mut values = Vec::<u8>::with_capacity(memory_size);
    for idx in 0..memory_size {
        values.push((idx % 255) as u8);
    }

    // Verify memory contents.
    assert_eq!(region.bytes, values);
}
#[test]
fn test_write_with_additional_memory() {
    test_write_with_additional_memory_helper(Context::Without)
}
#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
#[test]
fn test_write_with_additional_memory_with_context() {
    test_write_with_additional_memory_helper(Context::With)
}

#[test]
fn test_minidump_size_limit() {
    let num_of_threads = 40;
    let mut child = start_child_and_wait_for_threads(num_of_threads);
    let pid = child.id() as i32;

    let mut total_normal_stack_size = 0;
    let normal_file_size;
    // First, write a minidump with no size limit.
    {
        let mut tmpfile = tempfile::Builder::new()
            .prefix("write_dump_unlimited")
            .tempfile()
            .unwrap();

        MinidumpWriter::new(pid, pid)
            .dump(&mut tmpfile)
            .expect("Could not write minidump");

        let meta = std::fs::metadata(tmpfile.path()).expect("Couldn't get metadata for tempfile");
        assert!(meta.len() > 0);

        normal_file_size = meta.len();

        // Read dump file and check its contents
        let dump = Minidump::read_path(tmpfile.path()).expect("Failed to read minidump");
        let thread_list: MinidumpThreadList =
            dump.get_stream().expect("Couldn't find MinidumpThreadList");
        for thread in thread_list.threads {
            assert!(thread.raw.thread_id > 0);
            assert!(thread.raw.stack.memory.data_size > 0);
            total_normal_stack_size += thread.raw.stack.memory.data_size;
        }
    }

    // Second, write a minidump with a size limit big enough to not trigger
    // anything.
    {
        // Set size limit arbitrarily 1MB larger than the normal file size -- such
        // that the limiting code will not kick in.
        let minidump_size_limit = normal_file_size + 1024 * 1024;

        let mut tmpfile = tempfile::Builder::new()
            .prefix("write_dump_pseudolimited")
            .tempfile()
            .unwrap();

        MinidumpWriter::new(pid, pid)
            .set_minidump_size_limit(minidump_size_limit)
            .dump(&mut tmpfile)
            .expect("Could not write minidump");

        let meta = std::fs::metadata(tmpfile.path()).expect("Couldn't get metadata for tempfile");

        // Make sure limiting wasn't actually triggered.  NOTE: If you fail this,
        // first make sure that "minidump_size_limit" above is indeed set to a
        // large enough value -- the limit-checking code in minidump_writer.rs
        // does just a rough estimate.
        assert_eq!(meta.len(), normal_file_size);
    }

    // Third, write a minidump with a size limit small enough to be triggered.
    {
        // Set size limit to some arbitrary amount, such that the limiting code
        // will kick in.  The equation used to set this value was determined by
        // simply reversing the size-limit logic a little bit in order to pick a
        // size we know will trigger it.

        // Copyied from sections/thread_list_stream.rs
        const LIMIT_AVERAGE_THREAD_STACK_LENGTH: u64 = 8 * 1024;
        let mut minidump_size_limit = LIMIT_AVERAGE_THREAD_STACK_LENGTH * 40;

        // If, in reality, each of the threads' stack is *smaller* than
        // kLimitAverageThreadStackLength, the normal file size could very well be
        // smaller than the arbitrary limit that was just set.  In that case,
        // either of these numbers should trigger the size-limiting code, but we
        // might as well pick the smallest.
        if normal_file_size < minidump_size_limit {
            minidump_size_limit = normal_file_size;
        }

        let mut tmpfile = tempfile::Builder::new()
            .prefix("write_dump_limited")
            .tempfile()
            .unwrap();

        MinidumpWriter::new(pid, pid)
            .set_minidump_size_limit(minidump_size_limit)
            .dump(&mut tmpfile)
            .expect("Could not write minidump");

        let meta = std::fs::metadata(tmpfile.path()).expect("Couldn't get metadata for tempfile");
        assert!(meta.len() > 0);
        // Make sure the file size is at least smaller than the original.  If this
        // fails because it's the same size, then the size-limit logic didn't kick
        // in like it was supposed to.
        assert!(meta.len() < normal_file_size);

        let mut total_limit_stack_size = 0;
        // Read dump file and check its contents
        let dump = Minidump::read_path(tmpfile.path()).expect("Failed to read minidump");
        let thread_list: MinidumpThreadList =
            dump.get_stream().expect("Couldn't find MinidumpThreadList");
        for thread in thread_list.threads {
            assert!(thread.raw.thread_id > 0);
            assert!(thread.raw.stack.memory.data_size > 0);
            total_limit_stack_size += thread.raw.stack.memory.data_size;
        }

        // Make sure stack size shrunk by at least 1KB per extra thread.
        // Note: The 1KB is arbitrary, and assumes that the thread stacks are big
        // enough to shrink by that much.  For example, if each thread stack was
        // originally only 2KB, the current size-limit logic wouldn't actually
        // shrink them because that's the size to which it tries to shrink.  If
        // you fail this part of the test due to something like that, the test
        // logic should probably be improved to account for your situation.

        // Copyied from sections/thread_list_stream.rs
        const LIMIT_BASE_THREAD_COUNT: usize = 20;
        const MIN_PER_EXTRA_THREAD_STACK_REDUCTION: usize = 1024;
        let min_expected_reduction =
            (40 - LIMIT_BASE_THREAD_COUNT) * MIN_PER_EXTRA_THREAD_STACK_REDUCTION;
        assert!(total_limit_stack_size < total_normal_stack_size - min_expected_reduction as u32);
    }

    child.kill().expect("Failed to kill process");

    // Reap child
    let waitres = child.wait().expect("Failed to wait for child");
    let status = waitres.signal().expect("Child did not die due to signal");
    assert_eq!(waitres.code(), None);
    assert_eq!(status, Signal::SIGKILL as i32);
}

#[test]
fn test_with_deleted_binary() {
    let num_of_threads = 1;
    let binary_copy_dir = tempfile::Builder::new()
        .prefix("deleted_binary")
        .tempdir()
        .unwrap();
    let binary_copy = binary_copy_dir.as_ref().join("binary_copy");

    let path: &'static str = std::env!("CARGO_BIN_EXE_test");

    std::fs::copy(path, &binary_copy).expect("Failed to copy binary");
    let mem_slice = std::fs::read(&binary_copy).expect("Failed to read binary");

    let mut child = Command::new(&binary_copy)
        .env("RUST_BACKTRACE", "1")
        .arg("spawn_and_wait")
        .arg(format!("{}", num_of_threads))
        .stdout(Stdio::piped())
        .spawn()
        .expect("failed to execute child");
    wait_for_threads(&mut child, num_of_threads);

    let pid = child.id() as i32;

    let build_id = LinuxPtraceDumper::elf_file_identifier_from_mapped_file(&mem_slice)
        .expect("Failed to get build_id");

    let guid = GUID {
        data1: u32::from_ne_bytes(build_id[0..4].try_into().unwrap()),
        data2: u16::from_ne_bytes(build_id[4..6].try_into().unwrap()),
        data3: u16::from_ne_bytes(build_id[6..8].try_into().unwrap()),
        data4: build_id[8..16].try_into().unwrap(),
    };

    // guid_to_string() is not public in minidump, so copied it here
    // And append a zero, because module IDs include an "age" field
    // which is always zero on Linux.
    let filtered = format!(
        "{:08X}{:04X}{:04X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}0",
        guid.data1,
        guid.data2,
        guid.data3,
        guid.data4[0],
        guid.data4[1],
        guid.data4[2],
        guid.data4[3],
        guid.data4[4],
        guid.data4[5],
        guid.data4[6],
        guid.data4[7],
    );
    // Strip out dashes
    //let mut filtered: String = identifier.chars().filter(|x| *x != '-').collect();

    std::fs::remove_file(&binary_copy).expect("Failed to remove binary");

    let mut tmpfile = tempfile::Builder::new()
        .prefix("deleted_binary")
        .tempfile()
        .unwrap();

    MinidumpWriter::new(pid, pid)
        .dump(&mut tmpfile)
        .expect("Could not write minidump");

    child.kill().expect("Failed to kill process");

    // Reap child
    let waitres = child.wait().expect("Failed to wait for child");
    let status = waitres.signal().expect("Child did not die due to signal");
    assert_eq!(waitres.code(), None);
    assert_eq!(status, Signal::SIGKILL as i32);

    // Begin checks on dump
    let meta = std::fs::metadata(tmpfile.path()).expect("Couldn't get metadata for tempfile");
    assert!(meta.len() > 0);

    let dump = Minidump::read_path(tmpfile.path()).expect("Failed to read minidump");
    let module_list: MinidumpModuleList = dump
        .get_stream()
        .expect("Couldn't find stream MinidumpModuleList");
    let main_module = module_list
        .main_module()
        .expect("Could not get main module");
    assert_eq!(main_module.code_file(), binary_copy.to_string_lossy());
    assert_eq!(
        main_module.debug_identifier(),
        Some(std::borrow::Cow::from(filtered.as_str()))
    );
}

fn test_skip_if_requested_helper(context: Context) {
    let num_of_threads = 1;
    let mut child = start_child_and_wait_for_threads(num_of_threads);
    let pid = child.id() as i32;

    let mut tmpfile = tempfile::Builder::new()
        .prefix("skip_if_requested")
        .tempfile()
        .unwrap();

    let mut tmp = MinidumpWriter::new(pid, pid);
    #[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
    if context == Context::With {
        let crash_context = get_crash_context(pid);
        tmp.set_crash_context(crash_context);
    }

    let pr_mapping_addr;
    #[cfg(target_pointer_width = "64")]
    {
        pr_mapping_addr = 0x0102030405060708;
    }
    #[cfg(target_pointer_width = "32")]
    {
        pr_mapping_addr = 0x010203040;
    };
    let res = tmp
        .skip_stacks_if_mapping_unreferenced()
        .set_principal_mapping_address(pr_mapping_addr)
        .dump(&mut tmpfile);
    child.kill().expect("Failed to kill process");

    // Reap child
    let waitres = child.wait().expect("Failed to wait for child");
    let status = waitres.signal().expect("Child did not die due to signal");
    assert_eq!(waitres.code(), None);
    assert_eq!(status, Signal::SIGKILL as i32);

    assert!(res.is_err());
}
#[test]
fn test_skip_if_requested() {
    test_skip_if_requested_helper(Context::Without)
}
#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
#[test]
fn test_skip_if_requested_with_context() {
    test_skip_if_requested_helper(Context::With)
}

fn test_sanitized_stacks_helper(context: Context) {
    let num_of_threads = 1;
    let mut child = start_child_and_wait_for_threads(num_of_threads);
    let pid = child.id() as i32;

    let mut tmpfile = tempfile::Builder::new()
        .prefix("skip_if_requested")
        .tempfile()
        .unwrap();

    let mut tmp = MinidumpWriter::new(pid, pid);
    #[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
    if context == Context::With {
        let crash_context = get_crash_context(pid);
        tmp.set_crash_context(crash_context);
    }
    tmp.sanitize_stack()
        .dump(&mut tmpfile)
        .expect("Faild to dump minidump");
    child.kill().expect("Failed to kill process");

    // Reap child
    let waitres = child.wait().expect("Failed to wait for child");
    let status = waitres.signal().expect("Child did not die due to signal");
    assert_eq!(waitres.code(), None);
    assert_eq!(status, Signal::SIGKILL as i32);

    // Read dump file and check its contents
    let dump = Minidump::read_path(tmpfile.path()).expect("Failed to read minidump");
    let dump_array = std::fs::read(tmpfile.path()).expect("Failed to read minidump as vec");
    let thread_list: MinidumpThreadList =
        dump.get_stream().expect("Couldn't find MinidumpThreadList");

    let defaced;
    #[cfg(target_pointer_width = "64")]
    {
        defaced = 0x0defaced0defacedusize.to_ne_bytes();
    }
    #[cfg(target_pointer_width = "32")]
    {
        defaced = 0x0defacedusize.to_ne_bytes()
    };

    for thread in thread_list.threads {
        let mem = thread.raw.stack.memory;
        let start = mem.rva as usize;
        let end = (mem.rva + mem.data_size) as usize;
        let slice = &dump_array.as_slice()[start..end];
        assert!(slice
            .windows(defaced.len())
            .position(|window| window == defaced)
            .is_some());
    }
}
#[test]
fn test_sanitized_stacks() {
    test_sanitized_stacks_helper(Context::Without)
}
#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
#[test]
fn test_sanitized_stacks_with_context() {
    test_sanitized_stacks_helper(Context::Without)
}

fn test_write_early_abort_helper(context: Context) {
    let mut child = start_child_and_return("spawn_alloc_wait");
    let pid = child.id() as i32;

    let mut tmpfile = tempfile::Builder::new()
        .prefix("additional_memory")
        .tempfile()
        .unwrap();

    let mut f = BufReader::new(child.stdout.as_mut().expect("Can't open stdout"));
    let mut buf = String::new();
    let _ = f
        .read_line(&mut buf)
        .expect("Couldn't read address provided by child");
    let mut output = buf.split_whitespace();
    // We do not read the actual memory_address, but use NULL, which
    // should create an error during dumping and lead to a truncated minidump.
    let _ = usize::from_str_radix(output.next().unwrap().trim_start_matches("0x"), 16)
        .expect("unable to parse mmap_addr");
    let memory_addr = 0;
    let memory_size = usize::from_str(output.next().unwrap()).expect("unable to parse memory_size");

    let app_memory = AppMemory {
        ptr: memory_addr,
        length: memory_size,
    };

    let mut tmp = MinidumpWriter::new(pid, pid);
    #[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
    if context == Context::With {
        let crash_context = get_crash_context(pid);
        tmp.set_crash_context(crash_context);
    }

    // This should fail, because during the dump an error is detected (try_from fails)
    match tmp.set_app_memory(vec![app_memory]).dump(&mut tmpfile) {
        Err(WriterError::SectionAppMemoryError(_)) => (),
        _ => panic!("Wrong kind of error returned"),
    }

    child.kill().expect("Failed to kill process");
    // Reap child
    let waitres = child.wait().expect("Failed to wait for child");
    let status = waitres.signal().expect("Child did not die due to signal");
    assert_eq!(waitres.code(), None);
    assert_eq!(status, Signal::SIGKILL as i32);

    // Read dump file and check its contents. There should be a truncated minidump available
    let dump = Minidump::read_path(tmpfile.path()).expect("Failed to read minidump");
    // Should be there
    let _: MinidumpThreadList = dump.get_stream().expect("Couldn't find MinidumpThreadList");
    let _: MinidumpModuleList = dump.get_stream().expect("Couldn't find MinidumpThreadList");

    // Should be missing:
    assert!(dump.get_stream::<MinidumpMemoryList>().is_err());
}
#[test]
fn test_write_early_abort() {
    test_write_early_abort_helper(Context::Without)
}
#[cfg(not(any(target_arch = "mips", target_arch = "arm")))]
#[test]
fn test_write_early_abort_with_context() {
    test_write_early_abort_helper(Context::With)
}

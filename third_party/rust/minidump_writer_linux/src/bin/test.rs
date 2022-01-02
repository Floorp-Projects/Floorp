// This binary shouldn't be under /src, but under /tests, but that is
// currently not possible (https://github.com/rust-lang/cargo/issues/4356)
use minidump_writer_linux::linux_ptrace_dumper::{LinuxPtraceDumper, AT_SYSINFO_EHDR};
use minidump_writer_linux::{linux_ptrace_dumper, LINUX_GATE_LIBRARY_NAME};
use nix::sys::mman::{mmap, MapFlags, ProtFlags};
use nix::unistd::getppid;
use std::convert::TryInto;
use std::env;
use std::error;
use std::result;

type Error = Box<dyn error::Error + std::marker::Send + std::marker::Sync>;
pub type Result<T> = result::Result<T, Error>;

macro_rules! test {
    ($x:expr, $errmsg:expr) => {
        if $x {
            Ok(())
        } else {
            Err($errmsg)
        }
    };
}

fn test_setup() -> Result<()> {
    let ppid = getppid();
    linux_ptrace_dumper::LinuxPtraceDumper::new(ppid.as_raw())?;
    Ok(())
}

fn test_thread_list() -> Result<()> {
    let ppid = getppid();
    let dumper = linux_ptrace_dumper::LinuxPtraceDumper::new(ppid.as_raw())?;
    test!(!dumper.threads.is_empty(), "No threads")?;
    test!(
        dumper
            .threads
            .iter()
            .filter(|x| x.tid == ppid.as_raw())
            .count()
            == 1,
        "Thread found multiple times"
    )?;
    Ok(())
}

fn test_copy_from_process(stack_var: usize, heap_var: usize) -> Result<()> {
    let ppid = getppid().as_raw();
    let mut dumper = linux_ptrace_dumper::LinuxPtraceDumper::new(ppid)?;
    dumper.suspend_threads()?;
    let stack_res = LinuxPtraceDumper::copy_from_process(ppid, stack_var as *mut libc::c_void, 1)?;

    let expected_stack: libc::c_long = 0x11223344;
    test!(
        stack_res == expected_stack.to_ne_bytes(),
        "stack var not correct"
    )?;

    let heap_res = LinuxPtraceDumper::copy_from_process(ppid, heap_var as *mut libc::c_void, 1)?;
    let expected_heap: libc::c_long = 0x55667788;
    test!(
        heap_res == expected_heap.to_ne_bytes(),
        "heap var not correct"
    )?;
    dumper.resume_threads()?;
    Ok(())
}

fn test_find_mappings(addr1: usize, addr2: usize) -> Result<()> {
    let ppid = getppid();
    let dumper = linux_ptrace_dumper::LinuxPtraceDumper::new(ppid.as_raw())?;
    dumper
        .find_mapping(addr1)
        .ok_or("No mapping for addr1 found")?;

    dumper
        .find_mapping(addr2)
        .ok_or("No mapping for addr2 found")?;

    test!(dumper.find_mapping(0).is_none(), "NULL found")?;
    Ok(())
}

fn test_file_id() -> Result<()> {
    let ppid = getppid().as_raw();
    let exe_link = format!("/proc/{}/exe", ppid);
    let exe_name = std::fs::read_link(&exe_link)?.into_os_string();
    let mut dumper = linux_ptrace_dumper::LinuxPtraceDumper::new(getppid().as_raw())?;
    let mut found_exe = None;
    for (idx, mapping) in dumper.mappings.iter().enumerate() {
        if mapping.name.as_ref().map(|x| x.into()).as_ref() == Some(&exe_name) {
            found_exe = Some(idx);
            break;
        }
    }
    let idx = found_exe.unwrap();
    let id = dumper.elf_identifier_for_mapping_index(idx)?;
    assert!(!id.is_empty());
    assert!(id.iter().any(|&x| x > 0));
    Ok(())
}

fn test_merged_mappings(path: String, mapped_mem: usize, mem_size: usize) -> Result<()> {
    // Now check that LinuxPtraceDumper interpreted the mappings properly.
    let dumper = linux_ptrace_dumper::LinuxPtraceDumper::new(getppid().as_raw())?;
    let mut mapping_count = 0;
    for map in &dumper.mappings {
        if map.name == Some(path.clone()) {
            mapping_count += 1;
            // This mapping should encompass the entire original mapped
            // range.
            assert_eq!(map.start_address, mapped_mem);
            assert_eq!(map.size, mem_size);
            assert_eq!(0, map.offset);
        }
    }
    assert_eq!(1, mapping_count);
    Ok(())
}

fn test_linux_gate_mapping_id() -> Result<()> {
    let ppid = getppid().as_raw();
    let mut dumper = linux_ptrace_dumper::LinuxPtraceDumper::new(ppid)?;
    let mut found_linux_gate = false;
    for mut mapping in dumper.mappings.clone() {
        if mapping.name.as_deref() == Some(LINUX_GATE_LIBRARY_NAME) {
            found_linux_gate = true;
            dumper.suspend_threads()?;
            let id = LinuxPtraceDumper::elf_identifier_for_mapping(&mut mapping, ppid)?;
            test!(!id.is_empty(), "id-vec is empty")?;
            test!(id.iter().any(|&x| x > 0), "all id elements are 0")?;
            dumper.resume_threads()?;
            break;
        }
    }
    test!(found_linux_gate, "found no linux_gate")?;
    Ok(())
}

fn test_mappings_include_linux_gate() -> Result<()> {
    let ppid = getppid().as_raw();
    let dumper = linux_ptrace_dumper::LinuxPtraceDumper::new(ppid)?;
    let linux_gate_loc = dumper.auxv[&AT_SYSINFO_EHDR];
    test!(linux_gate_loc != 0, "linux_gate_loc == 0")?;
    let mut found_linux_gate = false;
    for mapping in &dumper.mappings {
        if mapping.name.as_deref() == Some(LINUX_GATE_LIBRARY_NAME) {
            found_linux_gate = true;
            test!(
                linux_gate_loc == mapping.start_address.try_into()?,
                "linux_gate_loc != start_address"
            )?;

            // This doesn't work here, as we do not test via "fork()", so the addresses are different
            // let ll = mapping.start_address as *const u8;
            // for idx in 0..header::SELFMAG {
            //     let mag = unsafe { std::ptr::read(ll.offset(idx as isize)) == header::ELFMAG[idx] };
            //     test!(
            //         mag,
            //         format!("ll: {} != ELFMAG: {} at {}", mag, header::ELFMAG[idx], idx)
            //     )?;
            // }
            break;
        }
    }
    test!(found_linux_gate, "found no linux_gate")?;
    Ok(())
}

fn spawn_and_wait(num: usize) -> Result<()> {
    // One less than the requested amount, as the main thread counts as well
    for _ in 1..num {
        std::thread::spawn(|| {
            println!("1");
            loop {
                std::thread::park();
            }
        });
    }
    println!("1");
    loop {
        std::thread::park();
    }
}

fn spawn_name_wait(num: usize) -> Result<()> {
    // One less than the requested amount, as the main thread counts as well
    for id in 1..num {
        std::thread::Builder::new()
            .name(format!("thread_{}", id))
            .spawn(|| {
                println!("1");
                loop {
                    std::thread::park();
                }
            })?;
    }
    println!("1");
    loop {
        std::thread::park();
    }
}
fn spawn_mmap_wait() -> Result<()> {
    let page_size = nix::unistd::sysconf(nix::unistd::SysconfVar::PAGE_SIZE).unwrap();
    let memory_size = page_size.unwrap() as usize;
    // Get some memory to be mapped by the child-process
    let mapped_mem = unsafe {
        mmap(
            std::ptr::null_mut(),
            memory_size,
            ProtFlags::PROT_READ | ProtFlags::PROT_WRITE,
            MapFlags::MAP_PRIVATE | MapFlags::MAP_ANON,
            -1,
            0,
        )
        .unwrap()
    };

    println!("{} {}", mapped_mem as usize, memory_size);
    loop {
        std::thread::park();
    }
}

fn spawn_alloc_wait() -> Result<()> {
    let page_size = nix::unistd::sysconf(nix::unistd::SysconfVar::PAGE_SIZE).unwrap();
    let memory_size = page_size.unwrap() as usize;

    let mut values = Vec::<u8>::with_capacity(memory_size);
    for idx in 0..memory_size {
        values.push((idx % 255) as u8);
    }

    println!("{:p} {}", values.as_ptr(), memory_size);
    loop {
        std::thread::park();
    }
}

fn main() -> Result<()> {
    let args: Vec<_> = env::args().skip(1).collect();
    match args.len() {
        1 => match args[0].as_ref() {
            "file_id" => test_file_id(),
            "setup" => test_setup(),
            "thread_list" => test_thread_list(),
            "mappings_include_linux_gate" => test_mappings_include_linux_gate(),
            "linux_gate_mapping_id" => test_linux_gate_mapping_id(),
            "spawn_mmap_wait" => spawn_mmap_wait(),
            "spawn_alloc_wait" => spawn_alloc_wait(),
            _ => Err("Len 1: Unknown test option".into()),
        },
        2 => match args[0].as_ref() {
            "spawn_and_wait" => {
                let num_of_threads: usize = args[1].parse().unwrap();
                spawn_and_wait(num_of_threads)
            }
            "spawn_name_wait" => {
                let num_of_threads: usize = args[1].parse().unwrap();
                spawn_name_wait(num_of_threads)
            }
            _ => Err(format!("Len 2: Unknown test option: {}", args[0]).into()),
        },
        3 => {
            if args[0] == "find_mappings" {
                let addr1: usize = args[1].parse().unwrap();
                let addr2: usize = args[2].parse().unwrap();
                test_find_mappings(addr1, addr2)
            } else if args[0] == "copy_from_process" {
                let stack_var: usize = args[1].parse().unwrap();
                let heap_var: usize = args[2].parse().unwrap();
                test_copy_from_process(stack_var, heap_var)
            } else {
                Err(format!("Len 3: Unknown test option: {}", args[0]).into())
            }
        }
        4 => {
            if args[0] == "merged_mappings" {
                let path = &args[1];
                let mapped_mem: usize = args[2].parse().unwrap();
                let mem_size: usize = args[3].parse().unwrap();
                test_merged_mappings(path.to_string(), mapped_mem, mem_size)
            } else {
                Err(format!("Len 4: Unknown test option: {}", args[0]).into())
            }
        }
        _ => Err("Unknown test option".into()),
    }
}

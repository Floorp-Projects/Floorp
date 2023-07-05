use std::cmp::min;

use super::*;
use crate::{minidump_cpu::RawContextCPU, minidump_writer::CrashingThreadContext};

// The following kLimit* constants are for when minidump_size_limit_ is set
// and the minidump size might exceed it.
//
// Estimate for how big each thread's stack will be (in bytes).
const LIMIT_AVERAGE_THREAD_STACK_LENGTH: usize = 8 * 1024;
// Number of threads whose stack size we don't want to limit.  These base
// threads will simply be the first N threads returned by the dumper (although
// the crashing thread will never be limited).  Threads beyond this count are
// the extra threads.
const LIMIT_BASE_THREAD_COUNT: usize = 20;
// Maximum stack size to dump for any extra thread (in bytes).
const LIMIT_MAX_EXTRA_THREAD_STACK_LEN: usize = 2 * 1024;
// Make sure this number of additional bytes can fit in the minidump
// (exclude the stack data).
const LIMIT_MINIDUMP_FUDGE_FACTOR: u64 = 64 * 1024;

#[derive(Debug, Clone, Copy)]
enum MaxStackLen {
    None,
    Len(usize),
}

pub fn write(
    config: &mut MinidumpWriter,
    buffer: &mut DumpBuf,
    dumper: &PtraceDumper,
) -> Result<MDRawDirectory, errors::SectionThreadListError> {
    let num_threads = dumper.threads.len();
    // Memory looks like this:
    // <num_threads><thread_1><thread_2>...

    let list_header = MemoryWriter::<u32>::alloc_with_val(buffer, num_threads as u32)?;

    let mut dirent = MDRawDirectory {
        stream_type: MDStreamType::ThreadListStream as u32,
        location: list_header.location(),
    };

    let mut thread_list = MemoryArrayWriter::<MDRawThread>::alloc_array(buffer, num_threads)?;
    dirent.location.data_size += thread_list.location().data_size;
    // If there's a minidump size limit, check if it might be exceeded.  Since
    // most of the space is filled with stack data, just check against that.
    // If this expects to exceed the limit, set extra_thread_stack_len such
    // that any thread beyond the first kLimitBaseThreadCount threads will
    // have only kLimitMaxExtraThreadStackLen bytes dumped.
    let mut extra_thread_stack_len = MaxStackLen::None; // default to no maximum
    if let Some(minidump_size_limit) = config.minidump_size_limit {
        let estimated_total_stack_size = (num_threads * LIMIT_AVERAGE_THREAD_STACK_LENGTH) as u64;
        let curr_pos = buffer.position();
        let estimated_minidump_size =
            curr_pos + estimated_total_stack_size + LIMIT_MINIDUMP_FUDGE_FACTOR;
        if estimated_minidump_size > minidump_size_limit {
            extra_thread_stack_len = MaxStackLen::Len(LIMIT_MAX_EXTRA_THREAD_STACK_LEN);
        }
    }

    for (idx, item) in dumper.threads.clone().iter().enumerate() {
        let mut thread = MDRawThread {
            thread_id: item.tid.try_into()?,
            suspend_count: 0,
            priority_class: 0,
            priority: 0,
            teb: 0,
            stack: MDMemoryDescriptor::default(),
            thread_context: MDLocationDescriptor::default(),
        };

        // We have a different source of information for the crashing thread. If
        // we used the actual state of the thread we would find it running in the
        // signal handler with the alternative stack, which would be deeply
        // unhelpful.
        if config.crash_context.is_some() && thread.thread_id == config.blamed_thread as u32 {
            let crash_context = config.crash_context.as_ref().unwrap();
            let instruction_ptr = crash_context.get_instruction_pointer();
            let stack_pointer = crash_context.get_stack_pointer();
            fill_thread_stack(
                config,
                buffer,
                dumper,
                &mut thread,
                instruction_ptr,
                stack_pointer,
                MaxStackLen::None,
            )?;
            // Copy 256 bytes around crashing instruction pointer to minidump.
            let ip_memory_size: usize = 256;
            // Bound it to the upper and lower bounds of the memory map
            // it's contained within. If it's not in mapped memory,
            // don't bother trying to write it.
            for mapping in &dumper.mappings {
                if instruction_ptr < mapping.start_address
                    || instruction_ptr >= mapping.start_address + mapping.size
                {
                    continue;
                }
                // Try to get 128 bytes before and after the IP, but
                // settle for whatever's available.
                let mut ip_memory_d = MDMemoryDescriptor {
                    start_of_memory_range: std::cmp::max(
                        mapping.start_address,
                        instruction_ptr - ip_memory_size / 2,
                    ) as u64,
                    ..Default::default()
                };

                let end_of_range = std::cmp::min(
                    mapping.start_address + mapping.size,
                    instruction_ptr + ip_memory_size / 2,
                ) as u64;
                ip_memory_d.memory.data_size =
                    (end_of_range - ip_memory_d.start_of_memory_range) as u32;

                let memory_copy = PtraceDumper::copy_from_process(
                    thread.thread_id as i32,
                    ip_memory_d.start_of_memory_range as *mut libc::c_void,
                    ip_memory_d.memory.data_size as usize,
                )?;

                let mem_section = MemoryArrayWriter::alloc_from_array(buffer, &memory_copy)?;
                ip_memory_d.memory = mem_section.location();
                config.memory_blocks.push(ip_memory_d);

                break;
            }
            // let cpu = MemoryWriter::alloc(buffer, &memory_copy)?;
            let mut cpu: RawContextCPU = Default::default();
            let crash_context = config.crash_context.as_ref().unwrap();
            crash_context.fill_cpu_context(&mut cpu);
            let cpu_section = MemoryWriter::alloc_with_val(buffer, cpu)?;
            thread.thread_context = cpu_section.location();

            config.crashing_thread_context =
                CrashingThreadContext::CrashContext(cpu_section.location());
        } else {
            let info = dumper.get_thread_info_by_index(idx)?;
            let max_stack_len =
                if config.minidump_size_limit.is_some() && idx >= LIMIT_BASE_THREAD_COUNT {
                    extra_thread_stack_len
                } else {
                    MaxStackLen::None // default to no maximum for this thread
                };
            let instruction_ptr = info.get_instruction_pointer();
            fill_thread_stack(
                config,
                buffer,
                dumper,
                &mut thread,
                instruction_ptr,
                info.stack_pointer,
                max_stack_len,
            )?;

            let mut cpu = RawContextCPU::default();
            info.fill_cpu_context(&mut cpu);
            let cpu_section = MemoryWriter::<RawContextCPU>::alloc_with_val(buffer, cpu)?;
            thread.thread_context = cpu_section.location();
            if item.tid == config.blamed_thread {
                // This is the crashing thread of a live process, but
                // no context was provided, so set the crash address
                // while the instruction pointer is already here.
                config.crashing_thread_context = CrashingThreadContext::CrashContextPlusAddress((
                    cpu_section.location(),
                    instruction_ptr,
                ));
            }
        }
        thread_list.set_value_at(buffer, thread, idx)?;
    }
    Ok(dirent)
}

fn fill_thread_stack(
    config: &mut MinidumpWriter,
    buffer: &mut DumpBuf,
    dumper: &PtraceDumper,
    thread: &mut MDRawThread,
    instruction_ptr: usize,
    stack_ptr: usize,
    max_stack_len: MaxStackLen,
) -> Result<(), errors::SectionThreadListError> {
    thread.stack.start_of_memory_range = stack_ptr.try_into()?;
    thread.stack.memory.data_size = 0;
    thread.stack.memory.rva = buffer.position() as u32;

    if let Ok((valid_stack_ptr, stack_len)) = dumper.get_stack_info(stack_ptr) {
        let stack_len = if let MaxStackLen::Len(max_stack_len) = max_stack_len {
            min(stack_len, max_stack_len)
        } else {
            stack_len
        };

        let mut stack_bytes = PtraceDumper::copy_from_process(
            thread.thread_id.try_into()?,
            valid_stack_ptr as *mut libc::c_void,
            stack_len,
        )?;
        let stack_pointer_offset = stack_ptr.saturating_sub(valid_stack_ptr);
        if config.skip_stacks_if_mapping_unreferenced {
            if let Some(principal_mapping) = &config.principal_mapping {
                let low_addr = principal_mapping.system_mapping_info.start_address;
                let high_addr = principal_mapping.system_mapping_info.end_address;
                if (instruction_ptr < low_addr || instruction_ptr > high_addr)
                    && !principal_mapping
                        .stack_has_pointer_to_mapping(&stack_bytes, stack_pointer_offset)
                {
                    return Ok(());
                }
            } else {
                return Ok(());
            }
        }

        if config.sanitize_stack {
            dumper.sanitize_stack_copy(&mut stack_bytes, stack_ptr, stack_pointer_offset)?;
        }

        let stack_location = MDLocationDescriptor {
            data_size: stack_bytes.len() as u32,
            rva: buffer.position() as u32,
        };
        buffer.write_all(&stack_bytes);
        thread.stack.start_of_memory_range = valid_stack_ptr as u64;
        thread.stack.memory = stack_location;
        config.memory_blocks.push(thread.stack);
    }
    Ok(())
}

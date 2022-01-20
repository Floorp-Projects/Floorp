use std::{mem, slice};
use std::io::{Write, stderr};

use super::winapi::shared::basetsd::DWORD_PTR;
use super::winapi::shared::minwindef::{DWORD, UINT};
use super::winapi::um::mmeapi::midiInAddBuffer;
use super::winapi::um::mmsystem::{HMIDIIN, MIDIHDR, MMSYSERR_NOERROR, MM_MIM_DATA,
                                  MM_MIM_LONGDATA, MM_MIM_LONGERROR};
use super::HandlerData;
use Ignore;

pub extern "system" fn handle_input<T>(_: HMIDIIN,
                input_status: UINT, 
                instance_ptr: DWORD_PTR,
                midi_message: DWORD_PTR,
                timestamp: DWORD) {
    if input_status != MM_MIM_DATA && input_status != MM_MIM_LONGDATA && input_status != MM_MIM_LONGERROR { return; }
    
    let data: &mut HandlerData<T> = unsafe { &mut *(instance_ptr as *mut HandlerData<T>) };
    
    // Calculate time stamp.
    data.message.timestamp = timestamp as u64 * 1000; // milliseconds -> microseconds
    
    if input_status == MM_MIM_DATA { // Channel or system message
        // Make sure the first byte is a status byte.
        let status: u8 = (midi_message & 0x000000FF) as u8;
        if !(status & 0x80 != 0) { return; }
        
        // Determine the number of bytes in the MIDI message.
        let nbytes: u16 = if status < 0xC0 { 3 }
        else if status < 0xE0 { 2 }
        else if status < 0xF0 { 3 }
        else if status == 0xF1 {
            if data.ignore_flags.contains(Ignore::Time) { return; }
            else  { 2 }
        } else if status == 0xF2 { 3 }
        else if status == 0xF3 { 2 }
        else if status == 0xF8 && (data.ignore_flags.contains(Ignore::Time)) {
            // A MIDI timing tick message and we're ignoring it.
            return;
        } else if status == 0xFE && (data.ignore_flags.contains(Ignore::ActiveSense)) {
            // A MIDI active sensing message and we're ignoring it.
            return;
        } else { 1 };
        
        // Copy bytes to our MIDI message.
        let ptr = (&midi_message) as *const DWORD_PTR as *const u8;
        let bytes: &[u8] = unsafe { slice::from_raw_parts(ptr, nbytes as usize) };
        data.message.bytes.extend_from_slice(bytes);
    } else { // Sysex message (MIM_LONGDATA or MIM_LONGERROR)
        let sysex = unsafe { &*(midi_message as *const MIDIHDR) };
        if !data.ignore_flags.contains(Ignore::Sysex) && input_status != MM_MIM_LONGERROR {
            // Sysex message and we're not ignoring it
            let bytes: &[u8] = unsafe { slice::from_raw_parts(sysex.lpData as *const u8, sysex.dwBytesRecorded as usize) };
            data.message.bytes.extend_from_slice(bytes);
            // TODO: If sysex messages are longer than RT_SYSEX_BUFFER_SIZE, they
            //       are split in chunks. We could reassemble a single message.
        }
    
        // The WinMM API requires that the sysex buffer be requeued after
        // input of each sysex message.  Even if we are ignoring sysex
        // messages, we still need to requeue the buffer in case the user
        // decides to not ignore sysex messages in the future.  However,
        // it seems that WinMM calls this function with an empty sysex
        // buffer when an application closes and in this case, we should
        // avoid requeueing it, else the computer suddenly reboots after
        // one or two minutes.
        if (unsafe {*data.sysex_buffer.0[sysex.dwUser as usize]}).dwBytesRecorded > 0 {
        //if ( sysex->dwBytesRecorded > 0 ) {
            let in_handle = data.in_handle.as_ref().unwrap().0.lock().unwrap();
            let result = unsafe { midiInAddBuffer(*in_handle, data.sysex_buffer.0[sysex.dwUser as usize], mem::size_of::<MIDIHDR>() as u32) };
            drop(in_handle);
            if result != MMSYSERR_NOERROR {
                let _ = writeln!(stderr(), "\nError in handle_input: Requeuing WinMM input sysex buffer failed.\n");
            }
            
            if data.ignore_flags.contains(Ignore::Sysex) { return; }
        } else { return; }
    }
    
    (data.callback)(data.message.timestamp, &data.message.bytes, data.user_data.as_mut().unwrap());   
    
    // Clear the vector for the next input message.
    data.message.bytes.clear();
}
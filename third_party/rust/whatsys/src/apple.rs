/* Based on code from sysinfo: https://crates.io/crates/sysinfo
 * Original licenses: MIT
 * Original author: Guillaume Gomez
 * License file: https://github.com/GuillaumeGomez/sysinfo/blob/master/LICENSE
 */

use libc::c_int;

fn get_system_info(value: c_int) -> Option<String> {
    let mut mib: [c_int; 2] = [libc::CTL_KERN, value];
    let mut size = 0;

    // Call first to get size
    unsafe {
        libc::sysctl(
            mib.as_mut_ptr(),
            2,
            std::ptr::null_mut(),
            &mut size,
            std::ptr::null_mut(),
            0,
        )
    };

    // exit early if we did not update the size
    if size == 0 {
        return None;
    }

    // set the buffer to the correct size
    let mut buf = vec![0_u8; size as usize];

    if unsafe {
        libc::sysctl(
            mib.as_mut_ptr(),
            2,
            buf.as_mut_ptr() as _,
            &mut size,
            std::ptr::null_mut(),
            0,
        )
    } == -1
    {
        // If command fails return default
        None
    } else {
        if let Some(pos) = buf.iter().position(|x| *x == 0) {
            // Shrink buffer to terminate the null bytes
            buf.resize(pos, 0);
        }

        String::from_utf8(buf).ok()
    }
}

/// Get the version of the currently running kernel.
///
/// Returns `None` if an error occured.
pub fn kernel_version() -> Option<String> {
    get_system_info(libc::KERN_OSRELEASE)
}

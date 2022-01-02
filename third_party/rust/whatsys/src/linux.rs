/* Based on code from sysinfo: https://crates.io/crates/sysinfo
 * Original licenses: MIT
 * Original author: Guillaume Gomez
 * License file: https://github.com/GuillaumeGomez/sysinfo/blob/master/LICENSE
 */

/// Get the version of the currently running kernel.
///
/// **Note**: The kernel version might include arbitrary alphanumeric suffixes.
///
/// Returns `None` if an error occured.
pub fn kernel_version() -> Option<String> {
    let mut raw = std::mem::MaybeUninit::<libc::utsname>::zeroed();

    unsafe {
        // SAFETY: We created the pointer from a value on the stack.
        if libc::uname(raw.as_mut_ptr()) == 0 {
            // SAFETY: If `uname` succesfully returns it filled in the data.
            let info = raw.assume_init();

            let release = info
                .release
                .iter()
                .filter(|c| **c != 0)
                .map(|c| *c as u8 as char)
                .collect::<String>();

            Some(release)
        } else {
            None
        }
    }
}

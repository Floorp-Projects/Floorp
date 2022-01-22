
use libc::{STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO, c_int, c_ulong, winsize};
use std::mem::zeroed;

// Unfortunately the actual command is not standardised...
#[cfg(any(target_os = "linux", target_os = "android"))]
static TIOCGWINSZ: c_ulong = 0x5413;

#[cfg(any(target_os = "macos",
            target_os = "ios",
            target_os = "bitrig",
            target_os = "dragonfly",
            target_os = "freebsd",
            target_os = "netbsd",
            target_os = "openbsd"))]
static TIOCGWINSZ: c_ulong = 0x40087468;

#[cfg(target_os = "solaris")]
static TIOCGWINSZ: c_ulong = 0x5468;

extern "C" {
    fn ioctl(fd: c_int, request: c_ulong, ...) -> c_int;
}

/// Runs the ioctl command. Returns (0, 0) if all of the streams are not to a terminal, or
/// there is an error. (0, 0) is an invalid size to have anyway, which is why
/// it can be used as a nil value.
unsafe fn get_dimensions_any() -> winsize {
    let mut window: winsize = zeroed();
    let mut result = ioctl(STDOUT_FILENO, TIOCGWINSZ, &mut window);

    if result == -1 {
        window = zeroed();
        result = ioctl(STDIN_FILENO, TIOCGWINSZ, &mut window);
        if result == -1 {
            window = zeroed();
            result = ioctl(STDERR_FILENO, TIOCGWINSZ, &mut window);
            if result == -1 {
                return zeroed();
            }
        }
    }
    window
}

/// Runs the ioctl command. Returns (0, 0) if the output is not to a terminal, or
/// there is an error. (0, 0) is an invalid size to have anyway, which is why
/// it can be used as a nil value.
unsafe fn get_dimensions_out() -> winsize {
    let mut window: winsize = zeroed();
    let result = ioctl(STDOUT_FILENO, TIOCGWINSZ, &mut window);

    if result != -1 {
        return window;
    }
    zeroed()
}

/// Runs the ioctl command. Returns (0, 0) if the input is not to a terminal, or
/// there is an error. (0, 0) is an invalid size to have anyway, which is why
/// it can be used as a nil value.
unsafe fn get_dimensions_in() -> winsize {
    let mut window: winsize = zeroed();
    let result = ioctl(STDIN_FILENO, TIOCGWINSZ, &mut window);

    if result != -1 {
        return window;
    }
    zeroed()
}

/// Runs the ioctl command. Returns (0, 0) if the error is not to a terminal, or
/// there is an error. (0, 0) is an invalid size to have anyway, which is why
/// it can be used as a nil value.
unsafe fn get_dimensions_err() -> winsize {
    let mut window: winsize = zeroed();
    let result = ioctl(STDERR_FILENO, TIOCGWINSZ, &mut window);

    if result != -1 {
        return window;
    }
    zeroed()
}

/// Query the current processes's output (`stdout`), input (`stdin`), and error (`stderr`) in
/// that order, in the attempt to determine terminal width. If one of those streams is actually
/// a tty, this function returns its width and height as a number of characters.
///
/// # Errors
/// 
/// If *all* of the streams are not ttys or return any errors this function will return `None`.
/// 
/// # Example
/// 
/// To get the dimensions of your terminal window, simply use the following:
/// 
/// ```no_run
/// # use term_size;
/// if let Some((w, h)) = term_size::dimensions() {
///     println!("Width: {}\nHeight: {}", w, h);
/// } else {
///     println!("Unable to get term size :(")
/// }
/// ```
pub fn dimensions() -> Option<(usize, usize)> {
    let w = unsafe { get_dimensions_any() };

    if w.ws_col == 0 || w.ws_row == 0 {
        None
    } else {
        Some((w.ws_col as usize, w.ws_row as usize))
    }
}

/// Query the current processes's output (`stdout`) *only*, in the attempt to determine
/// terminal width. If that stream is actually a tty, this function returns its width
/// and height as a number of characters.
///
/// # Errors
/// 
/// If the stream is not a tty or return any errors this function will return `None`.
/// 
/// # Example
/// 
/// To get the dimensions of your terminal window, simply use the following:
/// 
/// ```no_run
/// # use term_size;
/// if let Some((w, h)) = term_size::dimensions_stdout() {
///     println!("Width: {}\nHeight: {}", w, h);
/// } else {
///     println!("Unable to get term size :(")
/// }
/// ```
pub fn dimensions_stdout() -> Option<(usize, usize)> {
    let w = unsafe { get_dimensions_out() };

    if w.ws_col == 0 || w.ws_row == 0 {
        None
    } else {
        Some((w.ws_col as usize, w.ws_row as usize))
    }
}

/// Query the current processes's input (`stdin`) *only*, in the attempt to determine
/// terminal width. If that stream is actually a tty, this function returns its width
/// and height as a number of characters.
///
/// # Errors
/// 
/// If the stream is not a tty or return any errors this function will return `None`.
/// 
/// # Example
/// 
/// To get the dimensions of your terminal window, simply use the following:
/// 
/// ```no_run
/// # use term_size;
/// if let Some((w, h)) = term_size::dimensions_stdin() {
///     println!("Width: {}\nHeight: {}", w, h);
/// } else {
///     println!("Unable to get term size :(")
/// }
/// ```
pub fn dimensions_stdin() -> Option<(usize, usize)> {
    let w = unsafe { get_dimensions_in() };

    if w.ws_col == 0 || w.ws_row == 0 {
        None
    } else {
        Some((w.ws_col as usize, w.ws_row as usize))
    }
}

/// Query the current processes's error output (`stderr`) *only*, in the attempt to determine
/// terminal width. If that stream is actually a tty, this function returns its width
/// and height as a number of characters.
///
/// # Errors
/// 
/// If the stream is not a tty or return any errors this function will return `None`.
/// 
/// # Example
/// 
/// To get the dimensions of your terminal window, simply use the following:
/// 
/// ```no_run
/// # use term_size;
/// if let Some((w, h)) = term_size::dimensions_stderr() {
///     println!("Width: {}\nHeight: {}", w, h);
/// } else {
///     println!("Unable to get term size :(")
/// }
/// ```
pub fn dimensions_stderr() -> Option<(usize, usize)> {
    let w = unsafe { get_dimensions_err() };

    if w.ws_col == 0 || w.ws_row == 0 {
        None
    } else {
        Some((w.ws_col as usize, w.ws_row as usize))
    }
}
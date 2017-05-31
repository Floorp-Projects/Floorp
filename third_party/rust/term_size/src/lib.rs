// The following was originally taken and adapated from exa source
// repo: https://github.com/ogham/exa
// commit: b9eb364823d0d4f9085eb220233c704a13d0f611
// license: MIT - Copyright (c) 2014 Benjamin Sago

//! System calls for getting the terminal size.
//!
//! Getting the terminal size is performed using an ioctl command that takes
//! the file handle to the terminal -- which in this case, is stdout -- and
//! populates a structure containing the values.
//!
//! The size is needed when the user wants the output formatted into columns:
//! the default grid view, or the hybrid grid-details view.

#[cfg(not(target_os = "windows"))]
extern crate libc;
#[cfg(target_os = "windows")]
extern crate kernel32;
#[cfg(target_os = "windows")]
extern crate winapi;

pub use platform::{dimensions, dimensions_stdout, dimensions_stdin, dimensions_stderr};

#[cfg(any(target_os = "linux",
          target_os = "android",
          target_os = "macos",
          target_os = "ios",
          target_os = "bitrig",
          target_os = "dragonfly",
          target_os = "freebsd",
          target_os = "netbsd",
          target_os = "openbsd",
          target_os = "solaris"))]
mod platform {
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
    /// that order, in the attempt to dtermine terminal width. If one of those streams is actually
    /// a tty, this function returns its width and height as a number of characters.
    ///
    /// If *all* of the streams are not ttys or return any errors this function will return `None`.
    pub fn dimensions() -> Option<(usize, usize)> {
        let w = unsafe { get_dimensions_any() };

        if w.ws_col == 0 || w.ws_row == 0 {
            None
        } else {
            Some((w.ws_col as usize, w.ws_row as usize))
        }
    }

    /// Query the current processes's output (`stdout`) *only*, in the attempt to dtermine
    /// terminal width. If that streams is actually a tty, this function returns its width
    /// and height as a number of characters.
    ///
    /// If *all* of the streams are not ttys or return any errors this function will return `None`.
    pub fn dimensions_stdout() -> Option<(usize, usize)> {
        let w = unsafe { get_dimensions_out() };

        if w.ws_col == 0 || w.ws_row == 0 {
            None
        } else {
            Some((w.ws_col as usize, w.ws_row as usize))
        }
    }

    /// Query the current processes's input (`stdin`) *only*, in the attempt to dtermine
    /// terminal width. If that streams is actually a tty, this function returns its width
    /// and height as a number of characters.
    ///
    /// If *all* of the streams are not ttys or return any errors this function will return `None`.
    pub fn dimensions_stdin() -> Option<(usize, usize)> {
        let w = unsafe { get_dimensions_in() };

        if w.ws_col == 0 || w.ws_row == 0 {
            None
        } else {
            Some((w.ws_col as usize, w.ws_row as usize))
        }
    }

    /// Query the current processes's error output (`stderr`) *only*, in the attempt to dtermine
    /// terminal width. If that streams is actually a tty, this function returns its width
    /// and height as a number of characters.
    ///
    /// If *all* of the streams are not ttys or return any errors this function will return `None`.
    pub fn dimensions_stderr() -> Option<(usize, usize)> {
        let w = unsafe { get_dimensions_err() };

        if w.ws_col == 0 || w.ws_row == 0 {
            None
        } else {
            Some((w.ws_col as usize, w.ws_row as usize))
        }
    }
}

#[cfg(target_os = "windows")]
mod platform {
    use kernel32::{GetConsoleScreenBufferInfo, GetStdHandle};
    use winapi::{CONSOLE_SCREEN_BUFFER_INFO, COORD, SMALL_RECT, STD_OUTPUT_HANDLE};

    /// Query the current processes's output, returning its width and height as a
    /// number of characters. Returns `None` if the output isn't to a terminal.
    pub fn dimensions() -> Option<(usize, usize)> {
        let null_coord = COORD { X: 0, Y: 0 };
        let null_smallrect = SMALL_RECT {
            Left: 0,
            Top: 0,
            Right: 0,
            Bottom: 0,
        };

        let stdout_h = unsafe { GetStdHandle(STD_OUTPUT_HANDLE) };
        let mut console_data = CONSOLE_SCREEN_BUFFER_INFO {
            dwSize: null_coord,
            dwCursorPosition: null_coord,
            wAttributes: 0,
            srWindow: null_smallrect,
            dwMaximumWindowSize: null_coord,
        };

        if unsafe { GetConsoleScreenBufferInfo(stdout_h, &mut console_data) } != 0 {
            Some(((console_data.srWindow.Right - console_data.srWindow.Left) as usize,
                  (console_data.srWindow.Bottom - console_data.srWindow.Top) as usize))
        } else {
            None
        }
    }
    /// Query the current processes's output, returning its width and height as a
    /// number of characters. Returns `None` if the output isn't to a terminal.
    pub fn dimensions_stdout() -> Option<(usize, usize)> { dimensions() }
    /// This isn't implemented for Windows
    pub fn dimensions_stdin() -> Option<(usize, usize)> { unimplemented!() }
    /// This isn't implemented for Windows
    pub fn dimensions_stderr() -> Option<(usize, usize)> { unimplemented!() }
}

// makes project compilable on unsupported platforms
#[cfg(not(any(target_os = "linux",
              target_os = "android",
              target_os = "macos",
              target_os = "ios",
              target_os = "bitrig",
              target_os = "dragonfly",
              target_os = "freebsd",
              target_os = "netbsd",
              target_os = "openbsd",
              target_os = "solaris",
              target_os = "windows")))]
mod platform {
    pub fn dimensions() -> Option<(usize, usize)> { None }
    pub fn dimensions_stdout() -> Option<(usize, usize)> { None }
    pub fn dimensions_stdin() -> Option<(usize, usize)> { None }
    pub fn dimensions_stderr() -> Option<(usize, usize)> { None }
}

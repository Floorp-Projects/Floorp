use std::io;
use std::mem;

use winapi::um::consoleapi::{GetConsoleMode, SetConsoleMode};
use winapi::um::wincon::{
    CONSOLE_SCREEN_BUFFER_INFO,
    GetConsoleScreenBufferInfo, SetConsoleTextAttribute,
};

use AsHandleRef;

/// Query the given handle for information about the console's screen buffer.
///
/// The given handle should represent a console. Otherwise, an error is
/// returned.
///
/// This corresponds to calling [`GetConsoleScreenBufferInfo`].
///
/// [`GetConsoleScreenBufferInfo`]: https://docs.microsoft.com/en-us/windows/console/getconsolescreenbufferinfo
pub fn screen_buffer_info<H: AsHandleRef>(
    h: H,
) -> io::Result<ScreenBufferInfo> {
    unsafe {
        let mut info: CONSOLE_SCREEN_BUFFER_INFO = mem::zeroed();
        let rc = GetConsoleScreenBufferInfo(h.as_raw(), &mut info);
        if rc == 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(ScreenBufferInfo(info))
    }
}

/// Set the text attributes of the console represented by the given handle.
///
/// This corresponds to calling [`SetConsoleTextAttribute`].
///
/// [`SetConsoleTextAttribute`]: https://docs.microsoft.com/en-us/windows/console/setconsoletextattribute
pub fn set_text_attributes<H: AsHandleRef>(
    h: H,
    attributes: u16,
) -> io::Result<()> {
    if unsafe { SetConsoleTextAttribute(h.as_raw(), attributes) } == 0 {
        Err(io::Error::last_os_error())
    } else {
        Ok(())
    }
}

/// Query the mode of the console represented by the given handle.
///
/// This corresponds to calling [`GetConsoleMode`], which describes the return
/// value.
///
/// [`GetConsoleMode`]: https://docs.microsoft.com/en-us/windows/console/getconsolemode
pub fn mode<H: AsHandleRef>(h: H) -> io::Result<u32> {
    let mut mode = 0;
    if unsafe { GetConsoleMode(h.as_raw(), &mut mode) } == 0 {
        Err(io::Error::last_os_error())
    } else {
        Ok(mode)
    }
}

/// Set the mode of the console represented by the given handle.
///
/// This corresponds to calling [`SetConsoleMode`], which describes the format
/// of the mode parameter.
///
/// [`SetConsoleMode`]: https://docs.microsoft.com/en-us/windows/console/setconsolemode
pub fn set_mode<H: AsHandleRef>(h: H, mode: u32) -> io::Result<()> {
    if unsafe { SetConsoleMode(h.as_raw(), mode) } == 0 {
        Err(io::Error::last_os_error())
    } else {
        Ok(())
    }
}

/// Represents console screen buffer information such as size, cursor position
/// and styling attributes.
///
/// This wraps a [`CONSOLE_SCREEN_BUFFER_INFO`].
///
/// [`CONSOLE_SCREEN_BUFFER_INFO`]: https://docs.microsoft.com/en-us/windows/console/console-screen-buffer-info-str
#[derive(Clone)]
pub struct ScreenBufferInfo(CONSOLE_SCREEN_BUFFER_INFO);

impl ScreenBufferInfo {
    /// Returns the size of the console screen buffer, in character columns and
    /// rows.
    ///
    /// This corresponds to `dwSize`.
    pub fn size(&self) -> (i16, i16) {
        (self.0.dwSize.X, self.0.dwSize.Y)
    }

    /// Returns the position of the cursor in terms of column and row
    /// coordinates of the console screen buffer.
    ///
    /// This corresponds to `dwCursorPosition`.
    pub fn cursor_position(&self) -> (i16, i16) {
        (self.0.dwCursorPosition.X, self.0.dwCursorPosition.Y)
    }

    /// Returns the character attributes associated with this console.
    ///
    /// This corresponds to `wAttributes`.
    ///
    /// See [`char info`] for more details.
    ///
    /// [`char info`]: https://docs.microsoft.com/en-us/windows/console/char-info-str
    pub fn attributes(&self) -> u16 {
        self.0.wAttributes
    }

    /// Returns the maximum size of the console window, in character columns
    /// and rows, given the current screen buffer size and font and the screen
    /// size.
    pub fn max_window_size(&self) -> (i16, i16) {
        (self.0.dwMaximumWindowSize.X, self.0.dwMaximumWindowSize.Y)
    }
}

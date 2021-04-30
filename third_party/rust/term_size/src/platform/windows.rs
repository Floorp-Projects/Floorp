use winapi::um::processenv::GetStdHandle;
use winapi::um::winbase::STD_OUTPUT_HANDLE;
use winapi::um::wincon::GetConsoleScreenBufferInfo;
use winapi::um::wincon::{CONSOLE_SCREEN_BUFFER_INFO, COORD, SMALL_RECT};

/// Query the current processes's output, returning its width and height as a
/// number of characters. 
///
/// # Errors
/// 
/// Returns `None` if the output isn't to a terminal.
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
        Some(((console_data.srWindow.Right - console_data.srWindow.Left + 1) as usize,
                (console_data.srWindow.Bottom - console_data.srWindow.Top + 1) as usize))
    } else {
        None
    }
}

/// Query the current processes's output, returning its width and height as a
/// number of characters. Returns `None` if the output isn't to a terminal.
///
/// # Errors
/// 
/// Returns `None` if the output isn't to a terminal.
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
pub fn dimensions_stdout() -> Option<(usize, usize)> { dimensions() }

/// This isn't implemented for Windows
/// 
/// # Panics
/// 
/// This function `panic!`s unconditionally with the `unimplemented!`
/// macro
pub fn dimensions_stdin() -> Option<(usize, usize)> { unimplemented!() }

/// This isn't implemented for Windows
/// 
/// # Panics
/// 
/// This function `panic!`s unconditionally with the `unimplemented!`
/// macro
pub fn dimensions_stderr() -> Option<(usize, usize)> { unimplemented!() }

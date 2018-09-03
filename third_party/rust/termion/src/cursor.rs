//! Cursor movement.

use std::fmt;
use std::io::{self, Write, Error, ErrorKind, Read};
use async::async_stdin;
use std::time::{SystemTime, Duration};
use raw::CONTROL_SEQUENCE_TIMEOUT;

derive_csi_sequence!("Hide the cursor.", Hide, "?25l");
derive_csi_sequence!("Show the cursor.", Show, "?25h");

derive_csi_sequence!("Restore the cursor.", Restore, "u");
derive_csi_sequence!("Save the cursor.", Save, "s");

/// Goto some position ((1,1)-based).
///
/// # Why one-based?
///
/// ANSI escapes are very poorly designed, and one of the many odd aspects is being one-based. This
/// can be quite strange at first, but it is not that big of an obstruction once you get used to
/// it.
///
/// # Example
///
/// ```rust
/// extern crate termion;
///
/// fn main() {
///     print!("{}{}Stuff", termion::clear::All, termion::cursor::Goto(5, 3));
/// }
/// ```
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct Goto(pub u16, pub u16);

impl Default for Goto {
    fn default() -> Goto {
        Goto(1, 1)
    }
}

impl fmt::Display for Goto {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        debug_assert!(self != &Goto(0, 0), "Goto is one-based.");

        write!(f, csi!("{};{}H"), self.1, self.0)
    }
}

/// Move cursor left.
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct Left(pub u16);

impl fmt::Display for Left {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, csi!("{}D"), self.0)
    }
}

/// Move cursor right.
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct Right(pub u16);

impl fmt::Display for Right {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, csi!("{}C"), self.0)
    }
}

/// Move cursor up.
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct Up(pub u16);

impl fmt::Display for Up {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, csi!("{}A"), self.0)
    }
}

/// Move cursor down.
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct Down(pub u16);

impl fmt::Display for Down {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, csi!("{}B"), self.0)
    }
}

/// Types that allow detection of the cursor position.
pub trait DetectCursorPos {
    /// Get the (1,1)-based cursor position from the terminal.
    fn cursor_pos(&mut self) -> io::Result<(u16, u16)>;
}

impl<W: Write> DetectCursorPos for W {
    fn cursor_pos(&mut self) -> io::Result<(u16, u16)> {
        let mut stdin = async_stdin();

        // Where is the cursor?
        // Use `ESC [ 6 n`.
        write!(self, "\x1B[6n")?;
        self.flush()?;

        let mut buf: [u8; 1] = [0];
        let mut read_chars = Vec::new();

        let timeout = Duration::from_millis(CONTROL_SEQUENCE_TIMEOUT);
        let now = SystemTime::now();

        // Either consume all data up to R or wait for a timeout.
        while buf[0] != b'R' && now.elapsed().unwrap() < timeout {
            if stdin.read(&mut buf)? > 0 {
                read_chars.push(buf[0]);
            }
        }

        if read_chars.len() == 0 {
            return Err(Error::new(ErrorKind::Other, "Cursor position detection timed out."));
        }

        // The answer will look like `ESC [ Cy ; Cx R`.

        read_chars.pop(); // remove trailing R.
        let read_str = String::from_utf8(read_chars).unwrap();
        let beg = read_str.rfind('[').unwrap();
        let coords: String = read_str.chars().skip(beg + 1).collect();
        let mut nums = coords.split(';');

        let cy = nums.next()
            .unwrap()
            .parse::<u16>()
            .unwrap();
        let cx = nums.next()
            .unwrap()
            .parse::<u16>()
            .unwrap();

        Ok((cx, cy))
    }
}

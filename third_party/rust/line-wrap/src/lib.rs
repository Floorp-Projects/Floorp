//! Efficiently insert line endings.
//!
//! If you have a buffer full of data and want to insert any sort of regularly-spaced separator,
//! this will do it with a minimum of data copying. Commonly, this is to insert `\n` (see `lf()`) or `\r\n` (`crlf()`), but
//! any byte sequence can be used.
//!
//! 1. Pick a line ending. For single byte separators, see `ByteLineEnding`, or for two bytes, `TwoByteLineEnding`. For
//! arbitrary byte slices, use `SliceLineEnding`.
//! 2. Call `line_wrap`.
//! 3. Your data has been rearranged in place with the specified line ending inserted.
//!
//! # Examples
//!
//! ```
//! use line_wrap::*;
//! // suppose we have 80 bytes of data in a buffer and we want to wrap as per MIME.
//! // Buffer is large enough to hold line endings.
//! let mut data = vec![0; 82];
//!
//! assert_eq!(2, line_wrap(&mut data, 80, 76, &crlf()));
//!
//! // first line of zeroes
//! let mut expected_data = vec![0; 76];
//! // line ending
//! expected_data.extend_from_slice(b"\r\n");
//! // next line
//! expected_data.extend_from_slice(&[0, 0, 0, 0]);
//! assert_eq!(expected_data, data);
//! ```
//!
//! # Performance
//!
//! On an i7 6850k:
//!
//! - 10 byte input, 1 byte line length takes ~60ns (~160MiB/s)
//! - 100 byte input, 10 byte lines takes ~60ns (~1.6GiB/s)
//!     - Startup costs dominate at these small lengths
//! - 1,000 byte input, 100 byte lines takes ~65ns (~15GiB/s)
//! - 10,000 byte input, 100 byte lines takes ~550ns (~17GiB/s)
//! - In general, `SliceLineEncoding` is about 75% the speed of the fixed-length impls.
//!
//! Naturally, try `cargo +nightly bench` on your hardware to get more representative data.
extern crate safemem;

/// Unix-style line ending.
pub fn lf() -> ByteLineEnding { ByteLineEnding::new(b'\n') }

/// Windows-style line ending.
pub fn crlf() -> TwoByteLineEnding { TwoByteLineEnding::new(b'\r', b'\n') }

/// Writes line endings.
///
/// The trait allows specialization for the common single and double byte cases, netting nice
/// throughput improvements over simply using a slice for everything.
pub trait LineEnding {
    /// Write the line ending into the slice, which starts at the point where the ending should be written and is len() in length
    fn write_ending(&self, slice: &mut [u8]);
    /// The length of this particular line ending (must be constant and > 0)
    fn len(&self) -> usize;
}

/// A single byte line ending.
///
/// See `lf()`.
///
/// # Examples
///
/// ```
/// use line_wrap::*;
///
/// let ending = ByteLineEnding::new(b'\n');
///
/// let mut data = vec![1, 2, 3, 4, 5, 6, 255, 255];
///
/// assert_eq!(2, line_wrap(&mut data[..], 6, 2, &ending));
///
/// assert_eq!(vec![1, 2, b'\n', 3, 4, b'\n', 5, 6], data);
/// ```
pub struct ByteLineEnding {
    byte: u8
}

impl ByteLineEnding {
    pub fn new(byte: u8) -> ByteLineEnding {
        ByteLineEnding {
            byte
        }
    }
}

impl LineEnding for ByteLineEnding {
    #[inline]
    fn write_ending(&self, slice: &mut [u8]) {
        slice[0] = self.byte;
    }

    #[inline]
    fn len(&self) -> usize {
        1
    }
}

/// A double byte line ending.
///
/// See `crlf()`.
///
/// # Examples
///
/// ```
/// use line_wrap::*;
///
/// let ending = TwoByteLineEnding::new(b'\r', b'\n');
///
/// let mut data = vec![1, 2, 3, 4, 5, 6, 255, 255, 255, 255];
///
/// assert_eq!(4, line_wrap(&mut data[..], 6, 2, &ending));
///
/// assert_eq!(vec![1, 2, b'\r', b'\n', 3, 4, b'\r', b'\n', 5, 6], data);
/// ```
pub struct TwoByteLineEnding {
    byte0: u8,
    byte1: u8,
}

impl TwoByteLineEnding {
    pub fn new(byte0: u8, byte1: u8) -> TwoByteLineEnding {
        TwoByteLineEnding {
            byte0,
            byte1,
        }
    }
}

impl LineEnding for TwoByteLineEnding {
    #[inline]
    fn write_ending(&self, slice: &mut [u8]) {
        slice[0] = self.byte0;
        slice[1] = self.byte1;
    }

    #[inline]
    fn len(&self) -> usize {
        2
    }
}

/// A byte slice line ending.
///
/// Gives up some throughput compared to the specialized single/double byte impls, but works with
/// any length.
///
/// # Examples
///
/// ```
/// use line_wrap::*;
///
/// let ending = SliceLineEnding::new(b"xyz");
///
/// let mut data = vec![1, 2, 3, 4, 5, 6, 255, 255, 255, 255, 255, 255];
///
/// assert_eq!(6, line_wrap(&mut data[..], 6, 2, &ending));
///
/// assert_eq!(vec![1, 2, b'x', b'y', b'z', 3, 4, b'x', b'y', b'z', 5, 6], data);
/// ```
pub struct SliceLineEnding<'a> {
    slice: &'a [u8]
}

impl<'a> SliceLineEnding<'a> {
    pub fn new(slice: &[u8]) -> SliceLineEnding {
        SliceLineEnding {
            slice
        }
    }
}

impl<'a> LineEnding for SliceLineEnding<'a> {
    #[inline]
    fn write_ending(&self, slice: &mut [u8]) {
        slice.copy_from_slice(self.slice);
    }

    #[inline]
    fn len(&self) -> usize {
        self.slice.len()
    }
}

/// Insert line endings into the input.
///
/// Endings are inserted after each complete line, except the last line, even if the last line takes
/// up the full line width.
///
/// - `buf` must be large enough to handle the increased size after endings are inserted. In other
/// words, `buf.len() >= input_len / line_len * line_ending.len()`.
/// - `input_len` is the length of the unwrapped  in `buf`.
/// - `line_len` is the desired line width without line ending characters.
///
/// Returns the number of line ending bytes added.
///
/// # Panics
///
/// - When `line_ending.len() == 0`
/// - When `buf` is too small to contain the original input and its new line endings
pub fn line_wrap<L: LineEnding>(
    buf: &mut [u8],
    input_len: usize,
    line_len: usize,
    line_ending: &L,
) -> usize {
    assert!(line_ending.len() > 0);

    if input_len <= line_len {
        return 0;
    }

    let line_ending_len = line_ending.len();
    let line_wrap_params = line_wrap_parameters(input_len, line_len, line_ending_len);

    // ptr.offset() is undefined if it wraps, and there is no checked_offset(). However, because
    // we perform this check up front to make sure we have enough capacity, we know that none of
    // the subsequent pointer operations (assuming they implement the desired behavior of course!)
    // will overflow.
    assert!(
        buf.len() >= line_wrap_params.total_len,
        "Buffer must be able to hold encoded data after line wrapping"
    );

    // Move the last line, either partial or full, by itself as it does not have a line ending
    // afterwards
    let last_line_start = input_len.checked_sub(line_wrap_params.last_line_len)
        .expect("Last line start index underflow");
    // last line starts immediately after all the wrapped full lines
    let new_line_start = line_wrap_params.total_full_wrapped_lines_len;

    safemem::copy_over(
        buf,
        last_line_start,
        new_line_start,
        line_wrap_params.last_line_len,
    );

    let mut total_line_ending_bytes = 0;

    // initialize so that the initial decrement will set them correctly
    let mut old_line_start = last_line_start;
    let mut new_line_start = line_wrap_params.total_full_wrapped_lines_len;

    // handle the full lines
    for _ in 0..line_wrap_params.lines_with_endings {
        // the index after the end of the line ending we're about to write is the start of the next
        // line
        let end_of_line_ending = new_line_start;
        let start_of_line_ending = end_of_line_ending
            .checked_sub(line_ending_len)
            .expect("Line ending start index underflow");

        // doesn't underflow because it's decremented `line_wrap_params.lines_with_endings` times
        old_line_start = old_line_start.checked_sub(line_len)
            .expect("Old line start index underflow");
        new_line_start = new_line_start.checked_sub(line_wrap_params.line_with_ending_len)
            .expect("New line start index underflow");

        safemem::copy_over(buf, old_line_start, new_line_start, line_len);

        line_ending.write_ending(&mut buf[start_of_line_ending..(end_of_line_ending)]);
        total_line_ending_bytes += line_ending_len;
    }

    assert_eq!(line_wrap_params.total_line_endings_len, total_line_ending_bytes);

    total_line_ending_bytes
}

#[derive(Debug, PartialEq)]
struct LineWrapParameters {
    line_with_ending_len: usize,
    // number of lines that need an ending
    lines_with_endings: usize,
    // length of last line (which never needs an ending)
    last_line_len: usize,
    // length of lines that need an ending (which are always full lines), with their endings
    total_full_wrapped_lines_len: usize,
    // length of all lines, including endings for the ones that need them
    total_len: usize,
    // length of the line endings only
    total_line_endings_len: usize,
}

/// Calculations about how many lines we'll get for a given line length, line ending, etc.
/// This assumes that the last line will not get an ending, even if it is the full line length.
// Inlining improves short input single-byte by 25%.
#[inline]
fn line_wrap_parameters(
    input_len: usize,
    line_len: usize,
    line_ending_len: usize,
) -> LineWrapParameters {
    let line_with_ending_len = line_len
        .checked_add(line_ending_len)
        .expect("Line length with ending exceeds usize");

    if input_len <= line_len {
        // no wrapping needed
        return LineWrapParameters {
            line_with_ending_len,
            lines_with_endings: 0,
            last_line_len: input_len,
            total_full_wrapped_lines_len: 0,
            total_len: input_len,
            total_line_endings_len: 0,
        };
    };

    // lines_with_endings > 0, last_line_len > 0
    let (lines_with_endings, last_line_len) = if input_len % line_len > 0 {
        // Every full line has an ending since there is a partial line at the end
        (input_len / line_len, input_len % line_len)
    } else {
        // Every line is a full line, but no trailing ending.
        // Subtraction will not underflow since we know input_len > line_len.
        (input_len / line_len - 1, line_len)
    };

    // Should we expose exceeding usize via Result to be kind to 16-bit users? Or is that
    // always going to be a panic anyway in practice?

    // length of just the full lines with line endings
    let total_full_wrapped_lines_len = lines_with_endings
        .checked_mul(line_with_ending_len)
        .expect("Full lines with endings length exceeds usize");
    // all lines with appropriate endings, including the last line
    let total_len = total_full_wrapped_lines_len
        .checked_add(last_line_len)
        .expect("All lines with endings length exceeds usize");
    let total_line_endings_len = lines_with_endings
        .checked_mul(line_ending_len)
        .expect("Total line endings length exceeds usize");

    LineWrapParameters {
        line_with_ending_len,
        lines_with_endings,
        last_line_len,
        total_full_wrapped_lines_len,
        total_len,
        total_line_endings_len,
    }
}

#[cfg(test)]
mod tests;
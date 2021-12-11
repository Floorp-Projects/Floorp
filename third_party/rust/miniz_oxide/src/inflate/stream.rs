//! Extra streaming decompression functionality.
//!
//! As of now this is mainly inteded for use to build a higher-level wrapper.
use crate::alloc::boxed::Box;
use core::{cmp, mem};

use crate::inflate::core::{decompress, inflate_flags, DecompressorOxide, TINFL_LZ_DICT_SIZE};
use crate::inflate::TINFLStatus;
use crate::{DataFormat, MZError, MZFlush, MZResult, MZStatus, StreamResult};

/// Tag that determines reset policy of [InflateState](struct.InflateState.html)
pub trait ResetPolicy {
    /// Performs reset
    fn reset(&self, state: &mut InflateState);
}

/// Resets state, without performing expensive ops (e.g. zeroing buffer)
///
/// Note that not zeroing buffer can lead to security issues when dealing with untrusted input.
pub struct MinReset;

impl ResetPolicy for MinReset {
    fn reset(&self, state: &mut InflateState) {
        state.decompressor().init();
        state.dict_ofs = 0;
        state.dict_avail = 0;
        state.first_call = true;
        state.has_flushed = false;
        state.last_status = TINFLStatus::NeedsMoreInput;
    }
}

/// Resets state and zero memory, continuing to use the same data format.
pub struct ZeroReset;

impl ResetPolicy for ZeroReset {
    #[inline]
    fn reset(&self, state: &mut InflateState) {
        MinReset.reset(state);
        state.dict = [0; TINFL_LZ_DICT_SIZE];
    }
}

/// Full reset of the state, including zeroing memory.
///
/// Requires to provide new data format.
pub struct FullReset(pub DataFormat);

impl ResetPolicy for FullReset {
    #[inline]
    fn reset(&self, state: &mut InflateState) {
        ZeroReset.reset(state);
        state.data_format = self.0;
    }
}

/// A struct that compbines a decompressor with extra data for streaming decompression.
///
pub struct InflateState {
    /// Inner decompressor struct
    decomp: DecompressorOxide,

    /// Buffer of input bytes for matches.
    /// TODO: Could probably do this a bit cleaner with some
    /// Cursor-like class.
    /// We may also look into whether we need to keep a buffer here, or just one in the
    /// decompressor struct.
    dict: [u8; TINFL_LZ_DICT_SIZE],
    /// Where in the buffer are we currently at?
    dict_ofs: usize,
    /// How many bytes of data to be flushed is there currently in the buffer?
    dict_avail: usize,

    first_call: bool,
    has_flushed: bool,

    /// Whether the input data is wrapped in a zlib header and checksum.
    /// TODO: This should be stored in the decompressor.
    data_format: DataFormat,
    last_status: TINFLStatus,
}

impl Default for InflateState {
    fn default() -> Self {
        InflateState {
            decomp: DecompressorOxide::default(),
            dict: [0; TINFL_LZ_DICT_SIZE],
            dict_ofs: 0,
            dict_avail: 0,
            first_call: true,
            has_flushed: false,
            data_format: DataFormat::Raw,
            last_status: TINFLStatus::NeedsMoreInput,
        }
    }
}
impl InflateState {
    /// Create a new state.
    ///
    /// Note that this struct is quite large due to internal buffers, and as such storing it on
    /// the stack is not recommended.
    ///
    /// # Parameters
    /// `data_format`: Determines whether the compressed data is assumed to wrapped with zlib
    /// metadata.
    pub fn new(data_format: DataFormat) -> InflateState {
        let mut b = InflateState::default();
        b.data_format = data_format;
        b
    }

    /// Create a new state on the heap.
    ///
    /// # Parameters
    /// `data_format`: Determines whether the compressed data is assumed to wrapped with zlib
    /// metadata.
    pub fn new_boxed(data_format: DataFormat) -> Box<InflateState> {
        let mut b: Box<InflateState> = Box::default();
        b.data_format = data_format;
        b
    }

    /// Access the innner decompressor.
    pub fn decompressor(&mut self) -> &mut DecompressorOxide {
        &mut self.decomp
    }

    /// Return the status of the last call to `inflate` with this `InflateState`.
    pub const fn last_status(&self) -> TINFLStatus {
        self.last_status
    }

    /// Create a new state using miniz/zlib style window bits parameter.
    ///
    /// The decompressor does not support different window sizes. As such,
    /// any positive (>0) value will set the zlib header flag, while a negative one
    /// will not.
    pub fn new_boxed_with_window_bits(window_bits: i32) -> Box<InflateState> {
        let mut b: Box<InflateState> = Box::default();
        b.data_format = DataFormat::from_window_bits(window_bits);
        b
    }

    #[inline]
    /// Reset the decompressor without re-allocating memory, using the given
    /// data format.
    pub fn reset(&mut self, data_format: DataFormat) {
        self.reset_as(FullReset(data_format));
    }

    #[inline]
    /// Resets the state according to specified policy.
    pub fn reset_as<T: ResetPolicy>(&mut self, policy: T) {
        policy.reset(self)
    }
}

/// Try to decompress from `input` to `output` with the given `InflateState`
///
/// # Errors
///
/// Returns `MZError::Buf` If the size of the `output` slice is empty or no progress was made due to
/// lack of expected input data or called after the decompression was
/// finished without MZFlush::Finish.
///
/// Returns `MZError::Param` if the compressor parameters are set wrong.
pub fn inflate(
    state: &mut InflateState,
    input: &[u8],
    output: &mut [u8],
    flush: MZFlush,
) -> StreamResult {
    let mut bytes_consumed = 0;
    let mut bytes_written = 0;
    let mut next_in = input;
    let mut next_out = output;

    if flush == MZFlush::Full {
        return StreamResult::error(MZError::Stream);
    }

    let mut decomp_flags = inflate_flags::TINFL_FLAG_COMPUTE_ADLER32;
    if state.data_format == DataFormat::Zlib {
        decomp_flags |= inflate_flags::TINFL_FLAG_PARSE_ZLIB_HEADER;
    }

    let first_call = state.first_call;
    state.first_call = false;
    if (state.last_status as i32) < 0 {
        return StreamResult::error(MZError::Data);
    }

    if state.has_flushed && (flush != MZFlush::Finish) {
        return StreamResult::error(MZError::Stream);
    }
    state.has_flushed |= flush == MZFlush::Finish;

    if (flush == MZFlush::Finish) && first_call {
        decomp_flags |= inflate_flags::TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;

        let status = decompress(&mut state.decomp, next_in, next_out, 0, decomp_flags);
        let in_bytes = status.1;
        let out_bytes = status.2;
        let status = status.0;

        state.last_status = status;

        bytes_consumed += in_bytes;
        bytes_written += out_bytes;

        let ret_status = {
            if (status as i32) < 0 {
                Err(MZError::Data)
            } else if status != TINFLStatus::Done {
                state.last_status = TINFLStatus::Failed;
                Err(MZError::Buf)
            } else {
                Ok(MZStatus::StreamEnd)
            }
        };
        return StreamResult {
            bytes_consumed,
            bytes_written,
            status: ret_status,
        };
    }

    if flush != MZFlush::Finish {
        decomp_flags |= inflate_flags::TINFL_FLAG_HAS_MORE_INPUT;
    }

    if state.dict_avail != 0 {
        bytes_written += push_dict_out(state, &mut next_out);
        return StreamResult {
            bytes_consumed,
            bytes_written,
            status: Ok(
                if (state.last_status == TINFLStatus::Done) && (state.dict_avail == 0) {
                    MZStatus::StreamEnd
                } else {
                    MZStatus::Ok
                },
            ),
        };
    }

    let status = inflate_loop(
        state,
        &mut next_in,
        &mut next_out,
        &mut bytes_consumed,
        &mut bytes_written,
        decomp_flags,
        flush,
    );
    StreamResult {
        bytes_consumed,
        bytes_written,
        status,
    }
}

fn inflate_loop(
    state: &mut InflateState,
    next_in: &mut &[u8],
    next_out: &mut &mut [u8],
    total_in: &mut usize,
    total_out: &mut usize,
    decomp_flags: u32,
    flush: MZFlush,
) -> MZResult {
    let orig_in_len = next_in.len();
    loop {
        let status = decompress(
            &mut state.decomp,
            *next_in,
            &mut state.dict,
            state.dict_ofs,
            decomp_flags,
        );

        let in_bytes = status.1;
        let out_bytes = status.2;
        let status = status.0;

        state.last_status = status;

        *next_in = &next_in[in_bytes..];
        *total_in += in_bytes;

        state.dict_avail = out_bytes;
        *total_out += push_dict_out(state, next_out);

        // The stream was corrupted, and decompression failed.
        if (status as i32) < 0 {
            return Err(MZError::Data);
        }

        // The decompressor has flushed all it's data and is waiting for more input, but
        // there was no more input provided.
        if (status == TINFLStatus::NeedsMoreInput) && orig_in_len == 0 {
            return Err(MZError::Buf);
        }

        if flush == MZFlush::Finish {
            if status == TINFLStatus::Done {
                // There is not enough space in the output buffer to flush the remaining
                // decompressed data in the internal buffer.
                return if state.dict_avail != 0 {
                    Err(MZError::Buf)
                } else {
                    Ok(MZStatus::StreamEnd)
                };
            // No more space in the output buffer, but we're not done.
            } else if next_out.is_empty() {
                return Err(MZError::Buf);
            }
        } else {
            // We're not expected to finish, so it's fine if we can't flush everything yet.
            let empty_buf = next_in.is_empty() || next_out.is_empty();
            if (status == TINFLStatus::Done) || empty_buf || (state.dict_avail != 0) {
                return if (status == TINFLStatus::Done) && (state.dict_avail == 0) {
                    // No more data left, we're done.
                    Ok(MZStatus::StreamEnd)
                } else {
                    // Ok for now, still waiting for more input data or output space.
                    Ok(MZStatus::Ok)
                };
            }
        }
    }
}

fn push_dict_out(state: &mut InflateState, next_out: &mut &mut [u8]) -> usize {
    let n = cmp::min(state.dict_avail as usize, next_out.len());
    (next_out[..n]).copy_from_slice(&state.dict[state.dict_ofs..state.dict_ofs + n]);
    *next_out = &mut mem::replace(next_out, &mut [])[n..];
    state.dict_avail -= n;
    state.dict_ofs = (state.dict_ofs + (n)) & (TINFL_LZ_DICT_SIZE - 1);
    n
}

#[cfg(test)]
mod test {
    use super::{inflate, InflateState};
    use crate::{DataFormat, MZFlush, MZStatus};
    use std::vec;

    #[test]
    fn test_state() {
        let encoded = [
            120u8, 156, 243, 72, 205, 201, 201, 215, 81, 168, 202, 201, 76, 82, 4, 0, 27, 101, 4,
            19,
        ];
        let mut out = vec![0; 50];
        let mut state = InflateState::new_boxed(DataFormat::Zlib);
        let res = inflate(&mut state, &encoded, &mut out, MZFlush::Finish);
        let status = res.status.expect("Failed to decompress!");
        assert_eq!(status, MZStatus::StreamEnd);
        assert_eq!(out[..res.bytes_written as usize], b"Hello, zlib!"[..]);
        assert_eq!(res.bytes_consumed, encoded.len());

        state.reset_as(super::ZeroReset);
        out.iter_mut().map(|x| *x = 0).count();
        let res = inflate(&mut state, &encoded, &mut out, MZFlush::Finish);
        let status = res.status.expect("Failed to decompress!");
        assert_eq!(status, MZStatus::StreamEnd);
        assert_eq!(out[..res.bytes_written as usize], b"Hello, zlib!"[..]);
        assert_eq!(res.bytes_consumed, encoded.len());

        state.reset_as(super::MinReset);
        out.iter_mut().map(|x| *x = 0).count();
        let res = inflate(&mut state, &encoded, &mut out, MZFlush::Finish);
        let status = res.status.expect("Failed to decompress!");
        assert_eq!(status, MZStatus::StreamEnd);
        assert_eq!(out[..res.bytes_written as usize], b"Hello, zlib!"[..]);
        assert_eq!(res.bytes_consumed, encoded.len());
    }
}

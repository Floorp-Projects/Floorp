use ::encode::encode_to_slice;
use std::{cmp, fmt};
use std::io::{Result, Write};
use {encode_config_slice, Config};

pub(crate) const BUF_SIZE: usize = 1024;
/// The most bytes whose encoding will fit in `BUF_SIZE`
const MAX_INPUT_LEN: usize = BUF_SIZE / 4 * 3;
// 3 bytes of input = 4 bytes of base64, always (because we don't allow line wrapping)
const MIN_ENCODE_CHUNK_SIZE: usize = 3;

/// A `Write` implementation that base64 encodes data before delegating to the wrapped writer.
///
/// Because base64 has special handling for the end of the input data (padding, etc), there's a
/// `finish()` method on this type that encodes any leftover input bytes and adds padding if
/// appropriate. It's called automatically when deallocated (see the `Drop` implementation), but
/// any error that occurs when invoking the underlying writer will be suppressed. If you want to
/// handle such errors, call `finish()` yourself.
///
/// # Examples
///
/// ```
/// use std::io::Write;
///
/// // use a vec as the simplest possible `Write` -- in real code this is probably a file, etc.
/// let mut wrapped_writer = Vec::new();
/// {
///     let mut enc = base64::write::EncoderWriter::new(
///         &mut wrapped_writer, base64::STANDARD);
///
///     // handle errors as you normally would
///     enc.write_all(b"asdf").unwrap();
///     // could leave this out to be called by Drop, if you don't care
///     // about handling errors
///     enc.finish().unwrap();
///
/// }
///
/// // base64 was written to the writer
/// assert_eq!(b"YXNkZg==", &wrapped_writer[..]);
///
/// ```
///
/// # Panics
///
/// Calling `write()` after `finish()` is invalid and will panic.
///
/// # Errors
///
/// Base64 encoding itself does not generate errors, but errors from the wrapped writer will be
/// returned as per the contract of `Write`.
///
/// # Performance
///
/// It has some minor performance loss compared to encoding slices (a couple percent).
/// It does not do any heap allocation.
pub struct EncoderWriter<'a, W: 'a + Write> {
    config: Config,
    /// Where encoded data is written to
    w: &'a mut W,
    /// Holds a partial chunk, if any, after the last `write()`, so that we may then fill the chunk
    /// with the next `write()`, encode it, then proceed with the rest of the input normally.
    extra: [u8; MIN_ENCODE_CHUNK_SIZE],
    /// How much of `extra` is occupied, in `[0, MIN_ENCODE_CHUNK_SIZE]`.
    extra_len: usize,
    /// Buffer to encode into.
    output: [u8; BUF_SIZE],
    /// True iff padding / partial last chunk has been written.
    finished: bool,
    /// panic safety: don't write again in destructor if writer panicked while we were writing to it
    panicked: bool
}

impl<'a, W: Write> fmt::Debug for EncoderWriter<'a, W> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "extra:{:?} extra_len:{:?} output[..5]: {:?}",
            self.extra,
            self.extra_len,
            &self.output[0..5]
        )
    }
}

impl<'a, W: Write> EncoderWriter<'a, W> {
    /// Create a new encoder around an existing writer.
    pub fn new(w: &'a mut W, config: Config) -> EncoderWriter<'a, W> {
        EncoderWriter {
            config,
            w,
            extra: [0u8; MIN_ENCODE_CHUNK_SIZE],
            extra_len: 0,
            output: [0u8; BUF_SIZE],
            finished: false,
            panicked: false
        }
    }

    /// Encode all remaining buffered data and write it, including any trailing incomplete input
    /// triples and associated padding.
    ///
    /// Once this succeeds, no further writes can be performed, as that would produce invalid
    /// base64.
    ///
    /// # Errors
    ///
    /// Assuming the wrapped writer obeys the `Write` contract, if this returns `Err`, no data was
    /// written, and `finish()` may be retried if appropriate for the type of error, etc.
    pub fn finish(&mut self) -> Result<()> {
        if self.finished {
            return Ok(());
        };

        if self.extra_len > 0 {
            let encoded_len = encode_config_slice(
                &self.extra[..self.extra_len],
                self.config,
                &mut self.output[..],
            );
            self.panicked = true;
            let _ = self.w.write(&self.output[..encoded_len])?;
            self.panicked = false;
            // write succeeded, do not write the encoding of extra again if finish() is retried
            self.extra_len = 0;
        }

        self.finished = true;
        Ok(())
    }
}

impl<'a, W: Write> Write for EncoderWriter<'a, W> {
    fn write(&mut self, input: &[u8]) -> Result<usize> {
        if self.finished {
            panic!("Cannot write more after calling finish()");
        }

        if input.len() == 0 {
            return Ok(0);
        }

        // The contract of `Write::write` places some constraints on this implementation:
        // - a call to `write()` represents at most one call to a wrapped `Write`, so we can't
        // iterate over the input and encode multiple chunks.
        // - Errors mean that "no bytes were written to this writer", so we need to reset the
        // internal state to what it was before the error occurred

        // how many bytes, if any, were read into `extra` to create a triple to encode
        let mut extra_input_read_len = 0;
        let mut input = input;

        let orig_extra_len = self.extra_len;

        let mut encoded_size = 0;
        // always a multiple of MIN_ENCODE_CHUNK_SIZE
        let mut max_input_len = MAX_INPUT_LEN;

        // process leftover stuff from last write
        if self.extra_len > 0 {
            debug_assert!(self.extra_len < 3);
            if input.len() + self.extra_len >= MIN_ENCODE_CHUNK_SIZE {
                // Fill up `extra`, encode that into `output`, and consume as much of the rest of
                // `input` as possible.
                // We could write just the encoding of `extra` by itself but then we'd have to
                // return after writing only 4 bytes, which is inefficient if the underlying writer
                // would make a syscall.
                extra_input_read_len = MIN_ENCODE_CHUNK_SIZE - self.extra_len;
                debug_assert!(extra_input_read_len > 0);
                // overwrite only bytes that weren't already used. If we need to rollback extra_len
                // (when the subsequent write errors), the old leading bytes will still be there.
                self.extra[self.extra_len..MIN_ENCODE_CHUNK_SIZE].copy_from_slice(&input[0..extra_input_read_len]);

                let len = encode_to_slice(&self.extra[0..MIN_ENCODE_CHUNK_SIZE],
                                          &mut self.output[..],
                                          self.config.char_set.encode_table());
                debug_assert_eq!(4, len);

                input = &input[extra_input_read_len..];

                // consider extra to be used up, since we encoded it
                self.extra_len = 0;
                // don't clobber where we just encoded to
                encoded_size = 4;
                // and don't read more than can be encoded
                max_input_len = MAX_INPUT_LEN - MIN_ENCODE_CHUNK_SIZE;

                // fall through to normal encoding
            } else {
                // `extra` and `input` are non empty, but `|extra| + |input| < 3`, so there must be
                // 1 byte in each.
                debug_assert_eq!(1, input.len());
                debug_assert_eq!(1, self.extra_len);

                self.extra[self.extra_len] = input[0];
                self.extra_len += 1;
                return Ok(1);
            };
        } else if input.len() < MIN_ENCODE_CHUNK_SIZE {
            // `extra` is empty, and `input` fits inside it
            self.extra[0..input.len()].copy_from_slice(input);
            self.extra_len = input.len();
            return Ok(input.len());
        };

        // either 0 or 1 complete chunks encoded from extra
        debug_assert!(encoded_size == 0 || encoded_size == 4);
        debug_assert!(MAX_INPUT_LEN - max_input_len == 0
            || MAX_INPUT_LEN - max_input_len == MIN_ENCODE_CHUNK_SIZE);

        // handle complete triples
        let input_complete_chunks_len = input.len() - (input.len() % MIN_ENCODE_CHUNK_SIZE);
        let input_chunks_to_encode_len = cmp::min(input_complete_chunks_len, max_input_len);
        debug_assert_eq!(0, max_input_len % MIN_ENCODE_CHUNK_SIZE);
        debug_assert_eq!(0, input_chunks_to_encode_len % MIN_ENCODE_CHUNK_SIZE);

        encoded_size += encode_to_slice(
            &input[..(input_chunks_to_encode_len)],
            &mut self.output[encoded_size..],
            self.config.char_set.encode_table(),
        );
        self.panicked = true;
        let r = self.w.write(&self.output[..encoded_size]);
        self.panicked = false;
        match r {
            Ok(_) => return Ok(extra_input_read_len + input_chunks_to_encode_len),
            Err(_) => {
                // in case we filled and encoded `extra`, reset extra_len
                self.extra_len = orig_extra_len;
                return r;
            }
        }

        // we could hypothetically copy a few more bytes into `extra` but the extra 1-2 bytes
        // are not worth all the complexity (and branches)
    }

    /// Because this is usually treated as OK to call multiple times, it will *not* flush any
    /// incomplete chunks of input or write padding.
    fn flush(&mut self) -> Result<()> {
        self.w.flush()
    }
}

impl<'a, W: Write> Drop for EncoderWriter<'a, W> {
    fn drop(&mut self) {
        if !self.panicked {
            // like `BufWriter`, ignore errors during drop
            let _ = self.finish();
        }
    }
}

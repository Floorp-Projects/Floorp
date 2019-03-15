use encode::{add_padding, encode_to_slice};
use line_wrap::line_wrap;
use std::cmp;
use {Config, LineEnding, LineWrap};

/// The output mechanism for ChunkedEncoder's encoded bytes.
pub trait Sink {
    type Error;

    /// Handle a chunk of encoded base64 data (as UTF-8 bytes)
    fn write_encoded_bytes(&mut self, encoded: &[u8]) -> Result<(), Self::Error>;
}

#[derive(Debug, PartialEq)]
pub enum ChunkedEncoderError {
    /// If wrapping is configured, the line length must be a multiple of 4, and must not be absurdly
    /// large (see BUF_SIZE).
    InvalidLineLength,
}

const BUF_SIZE: usize = 1024;

/// A base64 encoder that emits encoded bytes in chunks without heap allocation.
pub struct ChunkedEncoder {
    config: Config,
    max_input_chunk_len: usize,
}

impl ChunkedEncoder {
    pub fn new(config: Config) -> Result<ChunkedEncoder, ChunkedEncoderError> {
        Ok(ChunkedEncoder {
            config,
            max_input_chunk_len: max_input_length(BUF_SIZE, &config)?,
        })
    }

    pub fn encode<S: Sink>(&self, bytes: &[u8], sink: &mut S) -> Result<(), S::Error> {
        let mut encode_buf: [u8; BUF_SIZE] = [0; BUF_SIZE];
        let encode_table = self.config.char_set.encode_table();

        let mut input_index = 0;

        while input_index < bytes.len() {
            // either the full input chunk size, or it's the last iteration
            let input_chunk_len = cmp::min(self.max_input_chunk_len, bytes.len() - input_index);

            let chunk = &bytes[input_index..(input_index + input_chunk_len)];

            let mut b64_bytes_written = encode_to_slice(chunk, &mut encode_buf, encode_table);

            input_index += input_chunk_len;
            let more_input_left = input_index < bytes.len();

            if self.config.pad && !more_input_left {
                // no more input, add padding if needed. Buffer will have room because
                // max_input_length leaves room for it.
                b64_bytes_written += add_padding(bytes.len(), &mut encode_buf[b64_bytes_written..]);
            }

            let line_ending_bytes = match self.config.line_wrap {
                LineWrap::NoWrap => 0,
                LineWrap::Wrap(line_len, line_ending) => {
                    let initial_line_ending_bytes =
                        line_wrap(&mut encode_buf, b64_bytes_written, line_len, line_ending);

                    if more_input_left {
                        assert_eq!(input_chunk_len, self.max_input_chunk_len);
                        // If there are more bytes of input, then we know we didn't just do the
                        // last chunk. line_wrap() doesn't put an ending after the last line, so we
                        // append one more line ending here. Since the chunk just encoded was not
                        // the last one, it was multiple of the line length (max_input_chunk_len),
                        // and therefore we can just put the line ending bytes at the end of the
                        // contents of the buffer.
                        match line_ending {
                            LineEnding::LF => {
                                encode_buf[b64_bytes_written + initial_line_ending_bytes] = b'\n';
                                initial_line_ending_bytes + 1
                            }
                            LineEnding::CRLF => {
                                encode_buf[b64_bytes_written + initial_line_ending_bytes] = b'\r';
                                encode_buf[b64_bytes_written + initial_line_ending_bytes + 1] =
                                    b'\n';
                                initial_line_ending_bytes + 2
                            }
                        }
                    } else {
                        initial_line_ending_bytes
                    }
                }
            };

            let total_bytes_written = b64_bytes_written + line_ending_bytes;

            sink.write_encoded_bytes(&encode_buf[0..total_bytes_written])?;
        }

        Ok(())
    }
}

/// Calculate the longest input that can be encoded for the given output buffer size.
///
/// If config requires line wrap, the calculated input length will be the maximum number of input
/// lines that can fit in the output buffer after each line has had its line ending appended.
///
/// If the config requires padding, two bytes of buffer space will be set aside so that the last
/// chunk of input can be encoded safely.
///
/// The input length will always be a multiple of 3 so that no encoding state has to be carried over
/// between chunks.
///
/// If the configured line length is not divisible by 4 (and therefore would require carrying
/// encoder state between chunks), or if the line length is too big for the buffer, an error will be
/// returned.
///
/// Note that the last overall line of input should *not* have an ending appended, but this will
/// conservatively calculate space as if it should because encoding is done in chunks, and all the
/// chunks before the last one will need a line ending after the last encoded line in that chunk.
fn max_input_length(encoded_buf_len: usize, config: &Config) -> Result<usize, ChunkedEncoderError> {
    let effective_buf_len = if config.pad {
        // make room for padding
        encoded_buf_len
            .checked_sub(2)
            .expect("Don't use a tiny buffer")
    } else {
        encoded_buf_len
    };

    match config.line_wrap {
        // No wrapping, no padding, so just normal base64 expansion.
        LineWrap::NoWrap => Ok((effective_buf_len / 4) * 3),
        LineWrap::Wrap(line_len, line_ending) => {
            // To avoid complicated encode buffer shuffling, only allow line lengths that are
            // multiples of 4 (which map to input lengths that are multiples of 3).
            // line_len is never 0.
            if line_len % 4 != 0 {
                return Err(ChunkedEncoderError::InvalidLineLength);
            }

            let single_encoded_full_line_with_ending_len = line_len
                .checked_add(line_ending.len())
                .expect("Encoded line length with ending exceeds usize");

            // max number of complete lines with endings that will fit in buffer
            let num_encoded_wrapped_lines_in_buffer =
                effective_buf_len / single_encoded_full_line_with_ending_len;

            if num_encoded_wrapped_lines_in_buffer == 0 {
                // line + ending is longer than can fit into encode buffer; give up
                Err(ChunkedEncoderError::InvalidLineLength)
            } else {
                let input_len_for_line_len = (line_len / 4) * 3;

                let input_len = input_len_for_line_len
                    .checked_mul(num_encoded_wrapped_lines_in_buffer)
                    .expect("Max input size exceeds usize");

                assert!(input_len % 3 == 0 && input_len > 1);

                Ok(input_len)
            }
        }
    }
}

#[cfg(test)]
pub mod tests {
    extern crate rand;

    use super::*;
    use tests::random_config;
    use *;

    use std::str;

    use self::rand::distributions::{IndependentSample, Range};
    use self::rand::Rng;

    #[test]
    fn chunked_encode_empty() {
        assert_eq!("", chunked_encode_str(&[], STANDARD));
    }

    #[test]
    fn chunked_encode_intermediate_fast_loop() {
        // > 8 bytes input, will enter the pretty fast loop
        assert_eq!(
            "Zm9vYmFyYmF6cXV4",
            chunked_encode_str(b"foobarbazqux", STANDARD)
        );
    }

    #[test]
    fn chunked_encode_fast_loop() {
        // > 32 bytes input, will enter the uber fast loop
        assert_eq!(
            "Zm9vYmFyYmF6cXV4cXV1eGNvcmdlZ3JhdWx0Z2FycGx5eg==",
            chunked_encode_str(b"foobarbazquxquuxcorgegraultgarplyz", STANDARD)
        );
    }

    #[test]
    fn chunked_encode_slow_loop_only() {
        // < 8 bytes input, slow loop only
        assert_eq!("Zm9vYmFy", chunked_encode_str(b"foobar", STANDARD));
    }

    #[test]
    fn chunked_encode_line_wrap_padding() {
        // < 8 bytes input, slow loop only
        let config = config_wrap(true, 4, LineEnding::LF);
        assert_eq!(
            "Zm9v\nYmFy\nZm9v\nYmFy\nZg==",
            chunked_encode_str(b"foobarfoobarf", config)
        );
    }

    #[test]
    fn chunked_encode_longer_than_one_buffer_adds_final_line_wrap_lf() {
        // longest line len possible
        let config = config_wrap(false, 1020, LineEnding::LF);
        let input = vec![0xFF; 768];
        let encoded = chunked_encode_str(&input, config);
        // got a line wrap
        assert_eq!(1024 + 1, encoded.len());

        for &b in encoded.as_bytes()[0..1020].iter() {
            // ascii /
            assert_eq!(47, b);
        }

        assert_eq!(10, encoded.as_bytes()[1020]);

        for &b in encoded.as_bytes()[1021..].iter() {
            // ascii /
            assert_eq!(47, b);
        }
    }

    #[test]
    fn chunked_encode_longer_than_one_buffer_adds_final_line_wrap_crlf() {
        // longest line len possible
        let config = config_wrap(false, 1020, LineEnding::CRLF);
        let input = vec![0xFF; 768];
        let encoded = chunked_encode_str(&input, config);
        // got a line wrap
        assert_eq!(1024 + 2, encoded.len());

        for &b in encoded.as_bytes()[0..1020].iter() {
            // ascii /
            assert_eq!(47, b);
        }

        assert_eq!(13, encoded.as_bytes()[1020]);
        assert_eq!(10, encoded.as_bytes()[1021]);

        for &b in encoded.as_bytes()[1022..].iter() {
            // ascii /
            assert_eq!(47, b);
        }
    }

    #[test]
    fn chunked_encode_matches_normal_encode_random_string_sink() {
        let helper = StringSinkTestHelper;
        chunked_encode_matches_normal_encode_random(&helper);
    }

    #[test]
    fn max_input_length_no_wrap_no_pad() {
        let config = config_no_wrap(false);
        assert_eq!(768, max_input_length(1024, &config).unwrap());
    }

    #[test]
    fn max_input_length_no_wrap_with_pad_decrements_one_triple() {
        let config = config_no_wrap(true);
        assert_eq!(765, max_input_length(1024, &config).unwrap());
    }

    #[test]
    fn max_input_length_no_wrap_with_pad_one_byte_short() {
        let config = config_no_wrap(true);
        assert_eq!(765, max_input_length(1025, &config).unwrap());
    }

    #[test]
    fn max_input_length_no_wrap_with_pad_fits_exactly() {
        let config = config_no_wrap(true);
        assert_eq!(768, max_input_length(1026, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_lf_fits_exactly_no_pad() {
        // 10 * (72 + 1) = 730. 54 input bytes = 72 encoded bytes, + 1 for LF.
        let config = config_wrap(false, 72, LineEnding::LF);
        assert_eq!(540, max_input_length(730, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_lf_fits_one_spare_byte_no_pad() {
        // 10 * (72 + 1) = 730. 54 input bytes = 72 encoded bytes, + 1 for LF.
        let config = config_wrap(false, 72, LineEnding::LF);
        assert_eq!(540, max_input_length(731, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_lf_size_one_byte_short_of_another_line_no_pad() {
        // 10 * (72 + 1) = 730. 54 input bytes = 72 encoded bytes, + 1 for LF.
        // 73 * 11 = 803
        let config = config_wrap(false, 72, LineEnding::LF);
        assert_eq!(540, max_input_length(802, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_lf_size_another_line_no_pad() {
        // 10 * (72 + 1) = 730. 54 input bytes = 72 encoded bytes, + 1 for LF.
        // 73 * 11 = 803
        let config = config_wrap(false, 72, LineEnding::LF);
        assert_eq!(594, max_input_length(803, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_lf_one_byte_short_with_pad() {
        // one fewer input line
        let config = config_wrap(true, 72, LineEnding::LF);
        assert_eq!(486, max_input_length(731, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_lf_fits_exactly_with_pad() {
        // 10 * (72 + 1) = 730. 54 input bytes = 72 encoded bytes, + 1 for LF.
        let config = config_wrap(true, 72, LineEnding::LF);
        assert_eq!(540, max_input_length(732, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_line_len_wont_fit_one_line_lf() {
        // 300 bytes is 400 encoded, + 1 for LF
        let config = config_wrap(false, 400, LineEnding::LF);
        assert_eq!(
            ChunkedEncoderError::InvalidLineLength,
            max_input_length(400, &config).unwrap_err()
        );
    }

    #[test]
    fn max_input_length_wrap_line_len_just_fits_one_line_lf() {
        // 300 bytes is 400 encoded, + 1 for LF
        let config = Config::new(
            CharacterSet::Standard,
            false,
            false,
            LineWrap::Wrap(400, LineEnding::LF),
        );
        assert_eq!(300, max_input_length(401, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_crlf_fits_exactly_no_pad() {
        // 10 * (72 + 2) = 740. 54 input bytes = 72 encoded bytes, + 2 for CRLF.
        let config = config_wrap(false, 72, LineEnding::CRLF);
        assert_eq!(540, max_input_length(740, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_crlf_fits_one_spare_byte_no_pad() {
        // 10 * (72 + 2) = 740. 54 input bytes = 72 encoded bytes, + 2 for CRLF.
        let config = config_wrap(false, 72, LineEnding::CRLF);
        assert_eq!(540, max_input_length(741, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_crlf_size_one_byte_short_of_another_line_no_pad() {
        // 10 * (72 + 2) = 740. 54 input bytes = 72 encoded bytes, + 2 for CRLF.
        // 74 * 11 = 814
        let config = config_wrap(false, 72, LineEnding::CRLF);
        assert_eq!(540, max_input_length(813, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_with_crlf_size_another_line_no_pad() {
        // 10 * (72 + 2) = 740. 54 input bytes = 72 encoded bytes, + 2 for CRLF.
        // 74 * 11 = 814
        let config = config_wrap(false, 72, LineEnding::CRLF);
        assert_eq!(594, max_input_length(814, &config).unwrap());
    }

    #[test]
    fn max_input_length_wrap_line_len_not_multiple_of_4_rejected() {
        let config = config_wrap(false, 41, LineEnding::LF);
        assert_eq!(
            ChunkedEncoderError::InvalidLineLength,
            max_input_length(400, &config).unwrap_err()
        );
    }

    pub fn chunked_encode_matches_normal_encode_random<S: SinkTestHelper>(sink_test_helper: &S) {
        let mut input_buf: Vec<u8> = Vec::new();
        let mut output_buf = String::new();
        let mut rng = rand::weak_rng();
        let line_len_range = Range::new(1, 1020);
        let input_len_range = Range::new(1, 10_000);

        for _ in 0..5_000 {
            input_buf.clear();
            output_buf.clear();

            let buf_len = input_len_range.ind_sample(&mut rng);
            for _ in 0..buf_len {
                input_buf.push(rng.gen());
            }

            let config = random_config_for_chunked_encoder(&mut rng, &line_len_range);

            let chunk_encoded_string = sink_test_helper.encode_to_string(config, &input_buf);
            encode_config_buf(&input_buf, config, &mut output_buf);

            assert_eq!(
                output_buf, chunk_encoded_string,
                "input len={}, config: pad={}, wrap={:?}",
                buf_len, config.pad, config.line_wrap
            );
        }
    }

    fn chunked_encode_str(bytes: &[u8], config: Config) -> String {
        let mut sink = StringSink::new();

        {
            let encoder = ChunkedEncoder::new(config).unwrap();
            encoder.encode(bytes, &mut sink).unwrap();
        }

        return sink.string;
    }

    fn random_config_for_chunked_encoder<R: Rng>(
        rng: &mut R,
        line_len_range: &Range<usize>,
    ) -> Config {
        loop {
            let config = random_config(rng, line_len_range);

            // only use a config with line_len that is divisible by 4
            match config.line_wrap {
                LineWrap::NoWrap => return config,
                LineWrap::Wrap(line_len, _) => if line_len % 4 == 0 {
                    return config;
                },
            }
        }
    }

    fn config_no_wrap(pad: bool) -> Config {
        Config::new(CharacterSet::Standard, pad, false, LineWrap::NoWrap)
    }

    fn config_wrap(pad: bool, line_len: usize, line_ending: LineEnding) -> Config {
        Config::new(
            CharacterSet::Standard,
            pad,
            false,
            LineWrap::Wrap(line_len, line_ending),
        )
    }

    // An abstraction around sinks so that we can have tests that easily to any sink implementation
    pub trait SinkTestHelper {
        fn encode_to_string(&self, config: Config, bytes: &[u8]) -> String;
    }

    // A really simple sink that just appends to a string for testing
    struct StringSink {
        string: String,
    }

    impl StringSink {
        fn new() -> StringSink {
            StringSink {
                string: String::new(),
            }
        }
    }

    impl Sink for StringSink {
        type Error = ();

        fn write_encoded_bytes(&mut self, s: &[u8]) -> Result<(), Self::Error> {
            self.string.push_str(str::from_utf8(s).unwrap());

            Ok(())
        }
    }

    struct StringSinkTestHelper;

    impl SinkTestHelper for StringSinkTestHelper {
        fn encode_to_string(&self, config: Config, bytes: &[u8]) -> String {
            let encoder = ChunkedEncoder::new(config).unwrap();
            let mut sink = StringSink::new();

            encoder.encode(bytes, &mut sink).unwrap();

            sink.string
        }
    }

}

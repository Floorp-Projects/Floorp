use std::io::Write;
use std::io;

use deflate_state::DeflateState;
use encoder_state::EncoderState;
use lzvalue::LZValue;
use lz77::{lz77_compress_block, LZ77Status};
use huffman_lengths::{gen_huffman_lengths, write_huffman_lengths, BlockType};
use bitstream::LsbWriter;
use stored_block::{compress_block_stored, write_stored_header, MAX_STORED_BLOCK_LENGTH};

const LARGEST_OUTPUT_BUF_SIZE: usize = 1024 * 32;

/// Flush mode to use when compressing input received in multiple steps.
///
/// (The more obscure ZLIB flush modes are not implemented.)
#[derive(Eq, PartialEq, Debug, Copy, Clone)]
pub enum Flush {
    // Simply wait for more input when we are out of input data to process.
    None,
    // Send a "sync block", corresponding to Z_SYNC_FLUSH in zlib. This finishes compressing and
    // outputting all pending data, and then outputs an empty stored block.
    // (That is, the block header indicating a stored block followed by `0000FFFF`).
    Sync,
    _Partial,
    _Block,
    _Full,
    // Finish compressing and output all remaining input.
    Finish,
}

/// Write all the lz77 encoded data in the buffer using the specified `EncoderState`, and finish
/// with the end of block code.
pub fn flush_to_bitstream(buffer: &[LZValue], state: &mut EncoderState) {
    for &b in buffer {
        state.write_lzvalue(b.value());
    }
    state.write_end_of_block()
}

/// Compress the input data using only fixed huffman codes.
///
/// Currently only used in tests.
#[cfg(test)]
pub fn compress_data_fixed(input: &[u8]) -> Vec<u8> {
    use lz77::lz77_compress;

    let mut state = EncoderState::fixed(Vec::new());
    let compressed = lz77_compress(input).unwrap();

    // We currently don't split blocks here(this function is just used for tests anyhow)
    state.write_start_of_block(true, true);
    flush_to_bitstream(&compressed, &mut state);

    state.flush();
    state.reset(Vec::new())
}

fn write_stored_block(input: &[u8], mut writer: &mut LsbWriter, final_block: bool) {

    // If the input is not zero, we write stored blocks for the input data.
    if !input.is_empty() {
        let mut i = input.chunks(MAX_STORED_BLOCK_LENGTH).peekable();

        while let Some(chunk) = i.next() {
            let last_chunk = i.peek().is_none();
            // Write the block header
            write_stored_header(writer, final_block && last_chunk);

            // Write the actual data.
            compress_block_stored(chunk, &mut writer).expect("Write error");

        }
    } else {
        // If the input length is zero, we output an empty block. This is used for syncing.
        write_stored_header(writer, final_block);
        compress_block_stored(&[], &mut writer).expect("Write error");
    }
}

/// Inner compression function used by both the writers and the simple compression functions.
pub fn compress_data_dynamic_n<W: Write>(
    input: &[u8],
    deflate_state: &mut DeflateState<W>,
    flush: Flush,
) -> io::Result<usize> {
    let mut bytes_written = 0;

    let mut slice = input;

    loop {
        let output_buf_len = deflate_state.output_buf().len();
        let output_buf_pos = deflate_state.output_buf_pos;
        // If the output buffer has too much data in it already, flush it before doing anything
        // else.
        if output_buf_len > LARGEST_OUTPUT_BUF_SIZE {
            let written = deflate_state
                .inner
                .as_mut()
                .expect("Missing writer!")
                .write(&deflate_state.encoder_state.inner_vec()[output_buf_pos..])?;

            if written < output_buf_len.checked_sub(output_buf_pos).unwrap() {
                // Only some of the data was flushed, so keep track of where we were.
                deflate_state.output_buf_pos += written;
            } else {
                // If we flushed all of the output, reset the output buffer.
                deflate_state.output_buf_pos = 0;
                deflate_state.output_buf().clear();
            }

            if bytes_written == 0 {
                // If the buffer was already full when the function was called, this has to be
                // returned rather than Ok(0) to indicate that we didn't write anything, but are
                // not done yet.
                return Err(io::Error::new(
                    io::ErrorKind::Interrupted,
                    "Internal buffer full.",
                ));
            } else {
                return Ok(bytes_written);
            }
        }

        if deflate_state.lz77_state.is_last_block() {
            // The last block has already been written, so we don't ave anything to compress.
            break;
        }

        let (written, status, position) = lz77_compress_block(
            slice,
            &mut deflate_state.lz77_state,
            &mut deflate_state.input_buffer,
            &mut deflate_state.lz77_writer,
            flush,
        );

        // Bytes written in this call
        bytes_written += written;
        // Total bytes written since the compression process started
        // TODO: Should we realistically have to worry about overflowing here?
        deflate_state.bytes_written += written as u64;

        if status == LZ77Status::NeedInput {
            // If we've consumed all the data input so far, and we're not
            // finishing or syncing or ending the block here, simply return
            // the number of bytes consumed so far.
            return Ok(bytes_written);
        }

        // Increment start of input data
        slice = &slice[written..];

        // We need to check if this is the last block as the header will then be
        // slightly different to indicate this.
        let last_block = deflate_state.lz77_state.is_last_block();

        let current_block_input_bytes = deflate_state.lz77_state.current_block_input_bytes();

        if cfg!(debug_assertions) {
            deflate_state
                .bytes_written_control
                .add(current_block_input_bytes);
        }

        let partial_bits = deflate_state.encoder_state.writer.pending_bits();

        let res = {
            let (l_freqs, d_freqs) = deflate_state.lz77_writer.get_frequencies();
            let (l_lengths, d_lengths) =
                deflate_state.encoder_state.huffman_table.get_lengths_mut();

            gen_huffman_lengths(
                l_freqs,
                d_freqs,
                current_block_input_bytes,
                partial_bits,
                l_lengths,
                d_lengths,
                &mut deflate_state.length_buffers,
            )
        };

        // Check if we've actually managed to compress the input, and output stored blocks
        // if not.
        match res {
            BlockType::Dynamic(header) => {
                // Write the block header.
                deflate_state
                    .encoder_state
                    .write_start_of_block(false, last_block);

                // Output the lengths of the huffman codes used in this block.
                write_huffman_lengths(
                    &header,
                    &deflate_state.encoder_state.huffman_table,
                    &mut deflate_state.length_buffers.length_buf,
                    &mut deflate_state.encoder_state.writer,
                );

                // Uupdate the huffman codes that will be used to encode the
                // lz77-compressed data.
                deflate_state
                    .encoder_state
                    .huffman_table
                    .update_from_lengths();


                // Write the huffman compressed data and the end of block marker.
                flush_to_bitstream(
                    deflate_state.lz77_writer.get_buffer(),
                    &mut deflate_state.encoder_state,
                );
            }
            BlockType::Fixed => {
                // Write the block header for fixed code blocks.
                deflate_state
                    .encoder_state
                    .write_start_of_block(true, last_block);

                // Use the pre-defined static huffman codes.
                deflate_state.encoder_state.set_huffman_to_fixed();

                // Write the compressed data and the end of block marker.
                flush_to_bitstream(
                    deflate_state.lz77_writer.get_buffer(),
                    &mut deflate_state.encoder_state,
                );
            }
            BlockType::Stored => {
                // If compression fails, output a stored block instead.

                let start_pos = position.saturating_sub(current_block_input_bytes as usize);

                assert!(
                    position >= current_block_input_bytes as usize,
                    "Error! Trying to output a stored block with forgotten data!\
                     if you encounter this error, please file an issue!"
                );

                write_stored_block(
                    &deflate_state.input_buffer.get_buffer()[start_pos..position],
                    &mut deflate_state.encoder_state.writer,
                    flush == Flush::Finish && last_block,
                );
            }
        };

        // Clear the current lz77 data in the writer for the next call.
        deflate_state.lz77_writer.clear();
        // We are done with the block, so we reset the number of bytes taken
        // for the next one.
        deflate_state.lz77_state.reset_input_bytes();

        // We are done for now.
        if status == LZ77Status::Finished {
            // This flush mode means that there should be an empty stored block at the end.
            if flush == Flush::Sync {
                write_stored_block(&[], &mut deflate_state.encoder_state.writer, false);
            } else if !deflate_state.lz77_state.is_last_block() {
                // Make sure a block with the last block header has been output.
                // Not sure this can actually happen, but we make sure to finish properly
                // if it somehow does.
                // An empty fixed block is the shortest.
                let es = &mut deflate_state.encoder_state;
                es.set_huffman_to_fixed();
                es.write_start_of_block(true, true);
                es.write_end_of_block();
            }
            break;
        }
    }

    // If we reach this point, the remaining data in the buffers is to be flushed.
    deflate_state.encoder_state.flush();
    // Make sure we've output everything, and return the number of bytes written if everything
    // went well.
    let output_buf_pos = deflate_state.output_buf_pos;
    let written_to_writer = deflate_state
        .inner
        .as_mut()
        .expect("Missing writer!")
        .write(&deflate_state.encoder_state.inner_vec()[output_buf_pos..])?;
    if written_to_writer <
        deflate_state
            .output_buf()
            .len()
            .checked_sub(output_buf_pos)
            .unwrap()
    {
        deflate_state.output_buf_pos += written_to_writer;
    } else {
        // If we sucessfully wrote all the data, we can clear the output buffer.
        deflate_state.output_buf_pos = 0;
        deflate_state.output_buf().clear();
    }
    Ok(bytes_written)
}

#[cfg(test)]
mod test {
    use super::*;
    use test_utils::{get_test_data, decompress_to_end};

    #[test]
    /// Test compressing a short string using fixed encoding.
    fn fixed_string_mem() {
        let test_data = String::from("                    GNU GENERAL PUBLIC LICENSE").into_bytes();
        let compressed = compress_data_fixed(&test_data);

        let result = decompress_to_end(&compressed);

        assert_eq!(test_data, result);
    }

    #[test]
    fn fixed_data() {
        let data = vec![190u8; 400];
        let compressed = compress_data_fixed(&data);
        let result = decompress_to_end(&compressed);

        assert_eq!(data, result);
    }

    /// Test deflate example.
    ///
    /// Check if the encoder produces the same code as the example given by Mark Adler here:
    /// https://stackoverflow.com/questions/17398931/deflate-encoding-with-static-huffman-codes/17415203
    #[test]
    fn fixed_example() {
        let test_data = b"Deflate late";
        // let check =
        // [0x73, 0x49, 0x4d, 0xcb, 0x49, 0x2c, 0x49, 0x55, 0xc8, 0x49, 0x2c, 0x49, 0x5, 0x0];
        let check = [
            0x73,
            0x49,
            0x4d,
            0xcb,
            0x49,
            0x2c,
            0x49,
            0x55,
            0x00,
            0x11,
            0x00,
        ];
        let compressed = compress_data_fixed(test_data);
        assert_eq!(&compressed, &check);
        let decompressed = decompress_to_end(&compressed);
        assert_eq!(&decompressed, test_data)
    }

    #[test]
    /// Test compression from a file.
    fn fixed_string_file() {
        let input = get_test_data();

        let compressed = compress_data_fixed(&input);
        println!("Fixed codes compressed len: {}", compressed.len());
        let result = decompress_to_end(&compressed);

        assert_eq!(input.len(), result.len());
        // Not using assert_eq here deliberately to avoid massive amounts of output spam.
        assert!(input == result);
    }
}

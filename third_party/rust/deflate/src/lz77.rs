//! This module contains functionality for doing lz77 compression of data.
#![macro_use]
use std::cmp;
use std::ops::{Range, RangeFrom};
use std::iter::{self, Iterator};
use std::slice::Iter;
use std::fmt;

use input_buffer::InputBuffer;
use matching::longest_match;
#[cfg(test)]
use lzvalue::{LZValue, LZType};
use huffman_table;
use chained_hash_table::{ChainedHashTable, update_hash};
#[cfg(test)]
use compression_options::{HIGH_MAX_HASH_CHECKS, HIGH_LAZY_IF_LESS_THAN};
use output_writer::{BufferStatus, DynamicWriter};
use compress::Flush;
use rle::process_chunk_greedy_rle;

const MAX_MATCH: usize = huffman_table::MAX_MATCH as usize;
const MIN_MATCH: usize = huffman_table::MIN_MATCH as usize;

const NO_RLE: u16 = 43212;

/// An enum describing whether we use lazy or greedy matching.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum MatchingType {
    /// Use greedy matching: the matching algorithm simply uses a match right away
    /// if found.
    Greedy,
    /// Use lazy matching: after finding a match, the next input byte is checked, to see
    /// if there is a better match starting at that byte.
    ///
    /// As a special case, if max_hash_checks is set to 0, compression using only run-length
    /// (i.e maximum match distance of 1) is performed instead.
    Lazy,
}

impl fmt::Display for MatchingType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            MatchingType::Greedy => write!(f, "Greedy matching"),
            MatchingType::Lazy => write!(f, "Lazy matching"),
        }
    }
}

/// A struct that contains the hash table, and keeps track of where we are in the input data
pub struct LZ77State {
    /// Struct containing hash chains that will be used to find matches.
    hash_table: ChainedHashTable,
    /// True if this is the first window that is being processed.
    is_first_window: bool,
    /// Set to true when the last block has been processed.
    is_last_block: bool,
    /// How many bytes the last match in the previous window extended into the current one.
    overlap: usize,
    /// How many bytes of input the current block contains.
    current_block_input_bytes: u64,
    /// The maximum number of hash entries to search.
    max_hash_checks: u16,
    /// Only lazy match if we have a match length less than this.
    lazy_if_less_than: u16,
    /// Whether to use greedy or lazy parsing
    matching_type: MatchingType,
    /// Keep track of the previous match and byte in case the buffer is full when lazy matching.
    match_state: ChunkState,
    /// Keep track of how many bytes in the lookahead that was part of a match, but has not been
    /// added to the hash chain yet.
    bytes_to_hash: usize,
    /// Keep track of if sync flush was used. If this is the case, the two first bytes needs to be
    /// hashed.
    was_synced: bool,
}

impl LZ77State {
    /// Creates a new LZ77 state
    pub fn new(
        max_hash_checks: u16,
        lazy_if_less_than: u16,
        matching_type: MatchingType,
    ) -> LZ77State {
        LZ77State {
            hash_table: ChainedHashTable::new(),
            is_first_window: true,
            is_last_block: false,
            overlap: 0,
            current_block_input_bytes: 0,
            max_hash_checks: max_hash_checks,
            lazy_if_less_than: lazy_if_less_than,
            matching_type: matching_type,
            match_state: ChunkState::new(),
            bytes_to_hash: 0,
            was_synced: false,
        }
    }

    /// Resets the state excluding max_hash_checks and lazy_if_less_than
    pub fn reset(&mut self) {
        self.hash_table.reset();
        self.is_first_window = true;
        self.is_last_block = false;
        self.overlap = 0;
        self.current_block_input_bytes = 0;
        self.match_state = ChunkState::new();
        self.bytes_to_hash = 0
    }

    pub fn set_last(&mut self) {
        self.is_last_block = true;
    }

    /// Is this the last block we are outputting?
    pub fn is_last_block(&self) -> bool {
        self.is_last_block
    }

    /// How many bytes of input the current block contains.
    pub fn current_block_input_bytes(&self) -> u64 {
        self.current_block_input_bytes
    }

    /// Sets the number of input bytes for the current block to 0.
    pub fn reset_input_bytes(&mut self) {
        self.current_block_input_bytes = 0;
    }

    /// Is there a buffered byte that has not been output yet?
    pub fn pending_byte(&self) -> bool {
        self.match_state.add
    }

    /// Returns 1 if pending_byte is true, 0 otherwise.
    pub fn pending_byte_as_num(&self) -> usize {
        // This could be implemented by using `as usize` as the documentation states this would give
        // the same result, but not sure if that should be relied upon.
        if self.match_state.add {
            1
        } else {
            0
        }
    }
}

const DEFAULT_WINDOW_SIZE: usize = 32768;

#[derive(Debug)]
/// Status after calling `process_chunk`.
pub enum ProcessStatus {
    /// All the input data was processed.
    Ok,
    /// The output buffer was full.
    ///
    /// The argument is the position of the next byte to be checked by `process_chunk`.
    BufferFull(usize),
}

#[derive(Debug)]
/// A struct to keep track of status between calls of `process_chunk_lazy`
///
/// This is needed as the output buffer might become full before having output all pending data.
pub struct ChunkState {
    /// Length of the last match that was found, if any.
    current_length: u16,
    /// Distance of the last match that was found, if any.
    current_distance: u16,
    /// The previous byte checked in process_chunk.
    prev_byte: u8,
    /// The current byte being checked in process_chunk.
    cur_byte: u8,
    /// Whether prev_byte still needs to be output.
    add: bool,
}

impl ChunkState {
    pub fn new() -> ChunkState {
        ChunkState {
            current_length: 0,
            current_distance: 0,
            prev_byte: 0,
            cur_byte: 0,
            add: false,
        }
    }
}

pub fn buffer_full(position: usize) -> ProcessStatus {
    ProcessStatus::BufferFull(position)
}

fn process_chunk(
    data: &[u8],
    iterated_data: &Range<usize>,
    mut match_state: &mut ChunkState,
    hash_table: &mut ChainedHashTable,
    writer: &mut DynamicWriter,
    max_hash_checks: u16,
    lazy_if_less_than: usize,
    matching_type: MatchingType,
) -> (usize, ProcessStatus) {
    let avoid_rle = if cfg!(test) {
        // Avoid RLE if lazy_if_less than is a specific value.
        // This is used in some tests, ideally we should probably do this in a less clunky way,
        // but we use a value here that is higher than the maximum sensible one anyhow, and will
        // be truncated by deflate_state for calls from outside the library.
        lazy_if_less_than == NO_RLE as usize
    } else {
        false
    };
    match matching_type {
        MatchingType::Greedy => {
            process_chunk_greedy(data, iterated_data, hash_table, writer, max_hash_checks)
        }
        MatchingType::Lazy => {
            if max_hash_checks > 0 || avoid_rle {
                process_chunk_lazy(
                    data,
                    iterated_data,
                    &mut match_state,
                    hash_table,
                    writer,
                    max_hash_checks,
                    lazy_if_less_than,
                )
            } else {
                // Use the RLE method if max_hash_checks is set to 0.
                process_chunk_greedy_rle(data, iterated_data, writer)
            }
        }
    }
}

/// Add the specified number of bytes to the hash table from the iterators
/// adding `start` to the position supplied to the hash table.
fn add_to_hash_table(
    bytes_to_add: usize,
    insert_it: &mut iter::Zip<RangeFrom<usize>, Iter<u8>>,
    hash_it: &mut Iter<u8>,
    hash_table: &mut ChainedHashTable,
) {
    let taker = insert_it.by_ref().take(bytes_to_add);
    let mut hash_taker = hash_it.by_ref().take(bytes_to_add);
    // Update the hash manually here to keep it in a register.
    let mut hash = hash_table.current_hash();
    // Advance the iterators and add the bytes we jump over to the hash table and
    // checksum
    for (ipos, _) in taker {
        if let Some(&i_hash_byte) = hash_taker.next() {
            hash = update_hash(hash, i_hash_byte);
            hash_table.add_with_hash(ipos, hash);
        }
    }
    // Write the hash back once we are done.
    hash_table.set_hash(hash);
}

/// Write the specified literal `byte` to the writer `w`, and return
/// `ProcessStatus::BufferFull($pos)` if the buffer is full after writing.
///
/// `pos` should indicate the byte to start at in the next call to `process_chunk`,
/// `is_hashed` should be set to true of the byte at pos has been added to the hash chain.
macro_rules! write_literal{
    ($w:ident, $byte:expr, $pos:expr) => {
        let b_status = $w.write_literal($byte);

        if let BufferStatus::Full = b_status {
            return (0, buffer_full($pos));
        }
    };
}

/// If the match is only 3 bytes long and the distance is more than 8 * 1024, it's likely to take
/// up more space than it would save.
#[inline]
fn match_too_far(match_len: usize, match_dist: usize) -> bool {
    const TOO_FAR: usize = 8 * 1024;
    match_len == MIN_MATCH && match_dist > TOO_FAR
}

///Create the iterators used when processing through a chunk of data.
fn create_iterators<'a>(
    data: &'a [u8],
    iterated_data: &Range<usize>,
) -> (
    usize,
    iter::Zip<RangeFrom<usize>, Iter<'a, u8>>,
    Iter<'a, u8>,
) {
    let end = cmp::min(data.len(), iterated_data.end);
    let start = iterated_data.start;
    let current_chunk = &data[start..end];

    let insert_it = (start..).zip(current_chunk.iter());
    let hash_it = {
        let hash_start = if data.len() - start > 2 {
            start + 2
        } else {
            data.len()
        };
        (&data[hash_start..]).iter()
    };
    (end, insert_it, hash_it)
}

fn process_chunk_lazy(
    data: &[u8],
    iterated_data: &Range<usize>,
    state: &mut ChunkState,
    mut hash_table: &mut ChainedHashTable,
    writer: &mut DynamicWriter,
    max_hash_checks: u16,
    lazy_if_less_than: usize,
) -> (usize, ProcessStatus) {

    let (end, mut insert_it, mut hash_it) = create_iterators(data, iterated_data);

    const NO_LENGTH: u16 = 0;

    // The previous match length, if any.
    let mut prev_length = state.current_length;
    // The distance of the previous match if any.
    let mut prev_distance = state.current_distance;

    state.current_length = 0;
    state.current_distance = 0;

    // The number of bytes past end that was added due to finding a match that extends into
    // the lookahead window.
    let mut overlap = 0;


    // Set to true if we found a match that is equal to or longer than `lazy_if_less_than`,
    // indicating that we won't lazy match (check for a better match at the next byte).
    // If we had a good match, carry this over from the previous call.
    let mut ignore_next = prev_length as usize >= lazy_if_less_than;

    // This is to output the correct byte in case there is one pending to be output
    // from the previous call.
    state.prev_byte = state.cur_byte;

    // Iterate through the slice, adding literals or length/distance pairs
    while let Some((position, &b)) = insert_it.next() {
        state.cur_byte = b;
        if let Some(&hash_byte) = hash_it.next() {
            hash_table.add_hash_value(position, hash_byte);

            // Only lazy match if we have a match shorter than a set value
            // TODO: This should be cleaned up a bit
            if !ignore_next {
                let (mut match_len, match_dist) = {
                    // If there already was a decent match at the previous byte
                    // and we are lazy matching, do less match checks in this step.
                    let max_hash_checks = if prev_length >= 32 {
                        max_hash_checks >> 2
                    } else {
                        max_hash_checks
                    };

                    // Check if we can find a better match here than the one we had at
                    // the previous byte.
                    longest_match(
                        data,
                        hash_table,
                        position,
                        prev_length as usize,
                        max_hash_checks,
                    )
                };

                // If the match is only 3 bytes long and very far back, it's probably not worth
                // outputting.
                if match_too_far(match_len, match_dist) {
                    match_len = NO_LENGTH as usize;
                };

                if match_len >= lazy_if_less_than {
                    // We found a decent match, so we won't check for a better one at the next byte.
                    ignore_next = true;
                }
                state.current_length = match_len as u16;
                state.current_distance = match_dist as u16;
            } else {
                // We already had a decent match, so we don't bother checking for another one.
                state.current_length = NO_LENGTH;
                state.current_distance = 0;
                // Make sure we check again next time.
                ignore_next = false;
            };

            if prev_length >= state.current_length && prev_length >= MIN_MATCH as u16 {
                // The previous match was better so we add it.
                // Casting note: length and distance is already bounded by the longest match
                // function. Usize is just used for convenience.
                let b_status =
                    writer.write_length_distance(prev_length as u16, prev_distance as u16);

                // We add the bytes to the hash table and checksum.
                // Since we've already added two of them, we need to add two less than
                // the length.
                let bytes_to_add = prev_length - 2;

                add_to_hash_table(
                    bytes_to_add as usize,
                    &mut insert_it,
                    &mut hash_it,
                    &mut hash_table,
                );

                // If the match is longer than the current window, we have note how many
                // bytes we overlap, since we don't need to do any matching on these bytes
                // in the next call of this function.
                // We don't have to worry about setting overlap to 0 if this is false, as the
                // function will stop after this condition is true, and overlap is not altered
                // elsewhere.
                if position + prev_length as usize > end {
                    // We need to subtract 1 since the byte at pos is also included.
                    overlap = position + prev_length as usize - end - 1;
                };

                state.add = false;

                // Note that there is no current match.
                state.current_length = 0;
                state.current_distance = 0;

                if let BufferStatus::Full = b_status {
                    // MATCH(lazy)
                    return (overlap, buffer_full(position + prev_length as usize - 1));
                }

                ignore_next = false;

            } else if state.add {
                // We found a better match (or there was no previous match)
                // so output the previous byte.
                // BETTER OR NO MATCH
                write_literal!(writer, state.prev_byte, position + 1);
            } else {
                state.add = true
            }

            prev_length = state.current_length;
            prev_distance = state.current_distance;
            state.prev_byte = b;
        } else {
            // If there is a match at this point, it will not have been added, so we need to add it.
            if prev_length >= MIN_MATCH as u16 {
                let b_status =
                    writer.write_length_distance(prev_length as u16, prev_distance as u16);

                state.current_length = 0;
                state.current_distance = 0;
                state.add = false;

                // As this will be a 3-length match at the end of the input data, there can't be any
                // overlap.
                // TODO: Not sure if we need to signal that the buffer is full here.
                // It's only needed in the case of syncing.
                if let BufferStatus::Full = b_status {
                    // TODO: These bytes should be hashed when doing a sync flush.
                    // This can't be done here as the new input data does not exist yet.
                    return (0, buffer_full(end));
                } else {
                    return (0, ProcessStatus::Ok);
                }
            };

            if state.add {
                // We may still have a leftover byte at this point, so we add it here if needed.
                state.add = false;

                // ADD
                write_literal!(writer, state.prev_byte, position + 1);

            };

            // We are at the last two bytes we want to add, so there is no point
            // searching for matches here.

            // AFTER ADD
            write_literal!(writer, b, position + 1);
        }
    }
    (overlap, ProcessStatus::Ok)
}

fn process_chunk_greedy(
    data: &[u8],
    iterated_data: &Range<usize>,
    mut hash_table: &mut ChainedHashTable,
    writer: &mut DynamicWriter,
    max_hash_checks: u16,
) -> (usize, ProcessStatus) {

    let (end, mut insert_it, mut hash_it) = create_iterators(data, iterated_data);

    const NO_LENGTH: usize = 0;

    // The number of bytes past end that was added due to finding a match that extends into
    // the lookahead window.
    let mut overlap = 0;

    // Iterate through the slice, adding literals or length/distance pairs.
    while let Some((position, &b)) = insert_it.next() {
        if let Some(&hash_byte) = hash_it.next() {
            hash_table.add_hash_value(position, hash_byte);

            // TODO: This should be cleaned up a bit.
            let (match_len, match_dist) =
                { longest_match(data, hash_table, position, NO_LENGTH, max_hash_checks) };

            if match_len >= MIN_MATCH as usize && !match_too_far(match_len, match_dist) {
                // Casting note: length and distance is already bounded by the longest match
                // function. Usize is just used for convenience.
                let b_status = writer.write_length_distance(match_len as u16, match_dist as u16);

                // We add the bytes to the hash table and checksum.
                // Since we've already added one of them, we need to add one less than
                // the length.
                let bytes_to_add = match_len - 1;
                add_to_hash_table(
                    bytes_to_add,
                    &mut insert_it,
                    &mut hash_it,
                    &mut hash_table,
                );

                // If the match is longer than the current window, we have note how many
                // bytes we overlap, since we don't need to do any matching on these bytes
                // in the next call of this function.
                if position + match_len > end {
                    // We need to subtract 1 since the byte at pos is also included.
                    overlap = position + match_len - end;
                };

                if let BufferStatus::Full = b_status {
                    // MATCH
                    return (overlap, buffer_full(position + match_len));
                }

            } else {
                // NO MATCH
                write_literal!(writer, b, position + 1);
            }
        } else {
            // We are at the last two bytes we want to add, so there is no point
            // searching for matches here.
            // END
            write_literal!(writer, b, position + 1);
        }
    }
    (overlap, ProcessStatus::Ok)
}


#[derive(Eq, PartialEq, Clone, Copy, Debug)]
pub enum LZ77Status {
    /// Waiting for more input before doing any processing
    NeedInput,
    /// The output buffer is full, so the current block needs to be ended so the
    /// buffer can be flushed.
    EndBlock,
    /// All pending data has been processed.
    Finished,
}

#[cfg(test)]
pub fn lz77_compress_block_finish(
    data: &[u8],
    state: &mut LZ77State,
    buffer: &mut InputBuffer,
    mut writer: &mut DynamicWriter,
) -> (usize, LZ77Status) {
    let (consumed, status, _) =
        lz77_compress_block(data, state, buffer, &mut writer, Flush::Finish);
    (consumed, status)
}

/// Compress a slice with lz77 compression.
///
/// This function processes one window at a time, and returns when there is no input left,
/// or it determines it's time to end a block.
///
/// Returns the number of bytes of the input that were consumed, a status describing
/// whether there is no input, it's time to finish, or it's time to end the block, and the position
/// of the first byte in the input buffer that has not been output (but may have been checked for
/// matches).
pub fn lz77_compress_block(
    data: &[u8],
    state: &mut LZ77State,
    buffer: &mut InputBuffer,
    mut writer: &mut DynamicWriter,
    flush: Flush,
) -> (usize, LZ77Status, usize) {
    // Currently we only support the maximum window size
    let window_size = DEFAULT_WINDOW_SIZE;

    // Indicates whether we should try to process all the data including the lookahead, or if we
    // should wait until we have at least one window size of data before doing anything.
    let finish = flush == Flush::Finish || flush == Flush::Sync;
    let sync = flush == Flush::Sync;

    let mut current_position = 0;

    // The current status of the encoding.
    let mut status = LZ77Status::EndBlock;

    // Whether warm up the hash chain with the two first values.
    let mut add_initial = true;

    // If we have synced, add the two first bytes to the hash as they couldn't be added before.
    if state.was_synced {
        if buffer.current_end() > 2 {
            let pos_add = buffer.current_end() - 2;
            for (n, &b) in data.iter().take(2).enumerate() {
                state.hash_table.add_hash_value(n + pos_add, b);
            }
            add_initial = false;
        }
        state.was_synced = false;
    }

    // Add data to the input buffer and keep a reference to the slice of data not added yet.
    let mut remaining_data = buffer.add_data(data);

    loop {
        // Note if there is a pending byte from the previous call to process_chunk,
        // so we get the block input size right.
        let pending_previous = state.pending_byte_as_num();

        assert!(writer.buffer_length() <= (window_size * 2));
        // The process is a bit different for the first 32k bytes.
        // TODO: There is a lot of duplicate code between the two branches here, we should be able
        // to simplify this.
        if state.is_first_window {
            // Don't do anything until we are either flushing, or we have at least one window of
            // data.
            if buffer.current_end() >= (window_size * 2) + MAX_MATCH || finish {

                if buffer.get_buffer().len() >= 2 && add_initial &&
                    state.current_block_input_bytes == 0
                {
                    let b = buffer.get_buffer();
                    // Warm up the hash with the two first values, so we can find  matches at
                    // index 0.
                    state.hash_table.add_initial_hash_values(b[0], b[1]);
                    add_initial = false;
                }

                let first_chunk_end = cmp::min(window_size, buffer.current_end());

                let start = state.overlap;

                let (overlap, p_status) = process_chunk(
                    buffer.get_buffer(),
                    &(start..first_chunk_end),
                    &mut state.match_state,
                    &mut state.hash_table,
                    &mut writer,
                    state.max_hash_checks,
                    state.lazy_if_less_than as usize,
                    state.matching_type,
                );

                state.overlap = overlap;
                state.bytes_to_hash = overlap;

                // If the buffer is full, we want to end the block.
                if let ProcessStatus::BufferFull(written) = p_status {
                    state.overlap = if overlap > 0 { overlap } else { written };
                    status = LZ77Status::EndBlock;
                    current_position = written - state.pending_byte_as_num();
                    state.current_block_input_bytes +=
                        (written - start + overlap + pending_previous -
                             state.pending_byte_as_num()) as u64;
                    break;
                }


                // Update the length of how many input bytes the current block is representing,
                // taking into account pending bytes.
                state.current_block_input_bytes +=
                    (first_chunk_end - start + overlap + pending_previous -
                         state.pending_byte_as_num()) as u64;

                // We are at the first window so we don't need to slide the hash table yet.
                // If finishing or syncing, we stop here.
                if first_chunk_end >= buffer.current_end() && finish {
                    current_position = first_chunk_end - state.pending_byte_as_num();
                    if !sync {
                        state.set_last();
                        state.is_first_window = false;
                    } else {
                        state.overlap = first_chunk_end;
                        state.was_synced = true;
                    }
                    debug_assert!(
                        !state.pending_byte(),
                        "Bug! Ended compression wit a pending byte!"
                    );
                    status = LZ77Status::Finished;
                    break;
                }
                // Otherwise, continue.
                state.is_first_window = false;
            } else {
                status = LZ77Status::NeedInput;
                break;
            }
        } else if buffer.current_end() >= (window_size * 2) + MAX_MATCH || finish {
            if buffer.current_end() >= window_size + 2 {
                for (n, &h) in buffer.get_buffer()[window_size + 2..]
                    .iter()
                    .enumerate()
                    .take(state.bytes_to_hash)
                {
                    state.hash_table.add_hash_value(window_size + n, h);
                }
                state.bytes_to_hash = 0;
            }
            // This isn't the first chunk, so we start reading at one window in in the
            // buffer plus any additional overlap from earlier.
            let start = window_size + state.overlap;

            // Determine where we have to stop iterating to slide the buffer and hash,
            // or stop because we are at the end of the input data.
            let end = cmp::min(window_size * 2, buffer.current_end());

            let (overlap, p_status) = process_chunk(
                buffer.get_buffer(),
                &(start..end),
                &mut state.match_state,
                &mut state.hash_table,
                &mut writer,
                state.max_hash_checks,
                state.lazy_if_less_than as usize,
                state.matching_type,
            );

            state.bytes_to_hash = overlap;


            if let ProcessStatus::BufferFull(written) = p_status {
                state.current_block_input_bytes +=
                    (written - start + pending_previous - state.pending_byte_as_num()) as u64;

                // If the buffer is full, return and end the block.
                // If overlap is non-zero, the buffer was full after outputting the last byte,
                // otherwise we have to skip to the point in the buffer where we stopped in the
                // next call.
                state.overlap = if overlap > 0 {
                    // If we are at the end of the window, make sure we slide the buffer and the
                    // hash table.
                    if state.max_hash_checks > 0 {
                        state.hash_table.slide(window_size);
                    }
                    remaining_data = buffer.slide(remaining_data.unwrap_or(&[]));
                    overlap
                } else {
                    written - window_size
                };

                current_position = written - state.pending_byte_as_num();


                // Status is already EndBlock at this point.
                // status = LZ77Status::EndBlock;
                break;
            }

            state.current_block_input_bytes +=
                (end - start + overlap + pending_previous - state.pending_byte_as_num()) as u64;

            // The buffer is not full, but we still need to note if there is any overlap into the
            // next window.
            state.overlap = overlap;

            if remaining_data.is_none() && finish && end == buffer.current_end() {
                current_position = buffer.current_end();
                debug_assert!(
                    !state.pending_byte(),
                    "Bug! Ended compression wit a pending byte!"
                );

                // We stopped before or at the window size, so we are at the end.
                if !sync {
                    // If we are finishing and not syncing, we simply indicate that we are done.
                    state.set_last();
                } else {
                    // For sync flushing we need to slide the buffer and the hash chains so that the
                    // next call to this function starts at the right place.

                    // There won't be any overlap, since when syncing, we process to the end of the.
                    // pending data.
                    state.overlap = buffer.current_end() - window_size;
                    state.was_synced = true;
                }
                status = LZ77Status::Finished;
                break;
            } else {
                // We are not at the end, so slide and continue.
                // We slide the hash table back to make space for new hash values
                // We only need to remember 2^15 bytes back (the maximum distance allowed by the
                // deflate spec).
                if state.max_hash_checks > 0 {
                    state.hash_table.slide(window_size);
                }

                // Also slide the buffer, discarding data we no longer need and adding new data.
                remaining_data = buffer.slide(remaining_data.unwrap_or(&[]));
            }
        } else {
            // The caller has not indicated that they want to finish or flush, and there is less
            // than a window + lookahead of new data, so we wait for more.
            status = LZ77Status::NeedInput;
            break;
        }

    }

    (
        data.len() - remaining_data.unwrap_or(&[]).len(),
        status,
        current_position,
    )
}

#[cfg(test)]
pub fn decompress_lz77(input: &[LZValue]) -> Vec<u8> {
    decompress_lz77_with_backbuffer(input, &[])
}

#[cfg(test)]
pub fn decompress_lz77_with_backbuffer(input: &[LZValue], back_buffer: &[u8]) -> Vec<u8> {
    let mut output = Vec::new();
    for p in input {
        match p.value() {
            LZType::Literal(l) => output.push(l),
            LZType::StoredLengthDistance(l, d) => {
                // We found a match, so we have to get the data that the match refers to.
                let d = d as usize;
                let prev_output_len = output.len();
                // Get match data from the back buffer if the match extends that far.
                let consumed = if d > output.len() {
                    let into_back_buffer = d - output.len();

                    assert!(
                        into_back_buffer <= back_buffer.len(),
                        "ERROR: Attempted to refer to a match in non-existing data!\
                         into_back_buffer: {}, back_buffer len {}, d {}, l {:?}",
                        into_back_buffer,
                        back_buffer.len(),
                        d,
                        l
                    );
                    let start = back_buffer.len() - into_back_buffer;
                    let end = cmp::min(back_buffer.len(), start + l.actual_length() as usize);
                    output.extend_from_slice(&back_buffer[start..end]);

                    end - start
                } else {
                    0
                };

                // Get match data from the currently decompressed data.
                let start = prev_output_len.saturating_sub(d);
                let mut n = 0;
                while n < (l.actual_length() as usize).saturating_sub(consumed) {
                    let b = output[start + n];
                    output.push(b);
                    n += 1;
                }
            }
        }
    }
    output
}

#[cfg(test)]
pub struct TestStruct {
    state: LZ77State,
    buffer: InputBuffer,
    writer: DynamicWriter,
}

#[cfg(test)]
impl TestStruct {
    fn new() -> TestStruct {
        TestStruct::with_config(
            HIGH_MAX_HASH_CHECKS,
            HIGH_LAZY_IF_LESS_THAN,
            MatchingType::Lazy,
        )
    }

    fn with_config(
        max_hash_checks: u16,
        lazy_if_less_than: u16,
        matching_type: MatchingType,
    ) -> TestStruct {
        TestStruct {
            state: LZ77State::new(max_hash_checks, lazy_if_less_than, matching_type),
            buffer: InputBuffer::empty(),
            writer: DynamicWriter::new(),
        }
    }

    fn compress_block(&mut self, data: &[u8], flush: bool) -> (usize, LZ77Status, usize) {
        lz77_compress_block(
            data,
            &mut self.state,
            &mut self.buffer,
            &mut self.writer,
            if flush { Flush::Finish } else { Flush::None },
        )
    }
}

#[cfg(test)]
pub fn lz77_compress(data: &[u8]) -> Option<Vec<LZValue>> {
    lz77_compress_conf(
        data,
        HIGH_MAX_HASH_CHECKS,
        HIGH_LAZY_IF_LESS_THAN,
        MatchingType::Lazy,
    )
}

/// Compress a slice, not storing frequency information
///
/// This is a convenience function for compression with fixed huffman values
/// Only used in tests for now
#[cfg(test)]
pub fn lz77_compress_conf(
    data: &[u8],
    max_hash_checks: u16,
    lazy_if_less_than: u16,
    matching_type: MatchingType,
) -> Option<Vec<LZValue>> {
    let mut test_boxed = Box::new(TestStruct::with_config(
        max_hash_checks,
        lazy_if_less_than,
        matching_type,
    ));
    let mut out = Vec::<LZValue>::with_capacity(data.len() / 3);
    {
        let test = test_boxed.as_mut();
        let mut slice = data;

        while !test.state.is_last_block {
            let bytes_written = lz77_compress_block_finish(
                slice,
                &mut test.state,
                &mut test.buffer,
                &mut test.writer,
            ).0;
            slice = &slice[bytes_written..];
            out.extend(test.writer.get_buffer());
            test.writer.clear();
        }

    }

    Some(out)
}

#[cfg(test)]
mod test {
    use super::*;

    use lzvalue::{LZValue, LZType, lit, ld};
    use chained_hash_table::WINDOW_SIZE;
    use compression_options::DEFAULT_LAZY_IF_LESS_THAN;
    use test_utils::get_test_data;
    use output_writer::MAX_BUFFER_LENGTH;

    /// Helper function to print the output from the lz77 compression function
    fn print_output(input: &[LZValue]) {
        let mut output = vec![];
        for l in input {
            match l.value() {
                LZType::Literal(l) => output.push(l),
                LZType::StoredLengthDistance(l, d) => {
                    output.extend(format!("<L {}>", l.actual_length()).into_bytes());
                    output.extend(format!("<D {}>", d).into_bytes())
                }
            }
        }

        println!("\"{}\"", String::from_utf8(output).unwrap());
    }

    /// Test that a short string from an example on SO compresses correctly
    #[test]
    fn compress_short() {
        let test_bytes = String::from("Deflate late").into_bytes();
        let res = lz77_compress(&test_bytes).unwrap();

        let decompressed = decompress_lz77(&res);

        assert_eq!(test_bytes, decompressed);
        assert_eq!(*res.last().unwrap(), LZValue::length_distance(4, 5));
    }

    /// Test that compression is working for a longer file
    #[test]
    fn compress_long() {
        let input = get_test_data();
        let compressed = lz77_compress(&input).unwrap();
        assert!(compressed.len() < input.len());

        let decompressed = decompress_lz77(&compressed);
        println!("compress_long length: {}", input.len());

        // This is to check where the compression fails, if it were to
        for (n, (&a, &b)) in input.iter().zip(decompressed.iter()).enumerate() {
            if a != b {
                println!("First difference at {}, input: {}, output: {}", n, a, b);
                break;
            }
        }
        assert_eq!(input.len(), decompressed.len());
        assert!(&decompressed == &input);
    }

    /// Check that lazy matching is working as intended
    #[test]
    fn lazy() {
        // We want to match on `badger` rather than `nba` as it is longer
        // let data = b" nba nbadg badger nbadger";
        let data = b"nba badger nbadger";
        let compressed = lz77_compress(data).unwrap();
        let test = compressed[compressed.len() - 1];
        if let LZType::StoredLengthDistance(l, _) = test.value() {
            assert_eq!(l.actual_length(), 6);
        } else {
            print_output(&compressed);
            panic!();
        }
    }

    fn roundtrip(data: &[u8]) {
        let compressed = super::lz77_compress(&data).unwrap();
        let decompressed = decompress_lz77(&compressed);
        assert!(decompressed == data);
    }

    // Check that data with the exact window size is working properly
    #[test]
    #[allow(unused)]
    fn exact_window_size() {
        use std::io::Write;
        let mut data = vec![0; WINDOW_SIZE];
        roundtrip(&data);
        {
            data.write(&[22; WINDOW_SIZE]);
        }
        roundtrip(&data);
        {
            data.write(&[55; WINDOW_SIZE]);
        }
        roundtrip(&data);
    }

    /// Test that matches at the window border are working correctly
    #[test]
    fn border() {
        use chained_hash_table::WINDOW_SIZE;
        let mut data = vec![35; WINDOW_SIZE];
        data.extend(b"Test");
        let compressed = super::lz77_compress(&data).unwrap();
        assert!(compressed.len() < data.len());
        let decompressed = decompress_lz77(&compressed);

        assert_eq!(decompressed.len(), data.len());
        assert!(decompressed == data);
    }

    #[test]
    fn border_multiple_blocks() {
        use chained_hash_table::WINDOW_SIZE;
        let mut data = vec![0; (WINDOW_SIZE * 2) + 50];
        data.push(1);
        let compressed = super::lz77_compress(&data).unwrap();
        assert!(compressed.len() < data.len());
        let decompressed = decompress_lz77(&compressed);
        assert_eq!(decompressed.len(), data.len());
        assert!(decompressed == data);
    }

    #[test]
    fn compress_block_status() {
        use input_buffer::InputBuffer;

        let data = b"Test data data";
        let mut writer = DynamicWriter::new();

        let mut buffer = InputBuffer::empty();
        let mut state = LZ77State::new(4096, DEFAULT_LAZY_IF_LESS_THAN, MatchingType::Lazy);
        let status = lz77_compress_block_finish(data, &mut state, &mut buffer, &mut writer);
        assert_eq!(status.1, LZ77Status::Finished);
        assert!(&buffer.get_buffer()[..data.len()] == data);
        assert_eq!(buffer.current_end(), data.len());
    }

    #[test]
    fn compress_block_multiple_windows() {
        use input_buffer::InputBuffer;
        use output_writer::DynamicWriter;

        let data = get_test_data();
        assert!(data.len() > (WINDOW_SIZE * 2) + super::MAX_MATCH);
        let mut writer = DynamicWriter::new();

        let mut buffer = InputBuffer::empty();
        let mut state = LZ77State::new(0, DEFAULT_LAZY_IF_LESS_THAN, MatchingType::Lazy);
        let (bytes_consumed, status) =
            lz77_compress_block_finish(&data, &mut state, &mut buffer, &mut writer);
        assert_eq!(
            buffer.get_buffer().len(),
            (WINDOW_SIZE * 2) + super::MAX_MATCH
        );
        assert_eq!(status, LZ77Status::EndBlock);
        let buf_len = buffer.get_buffer().len();
        assert!(buffer.get_buffer()[..] == data[..buf_len]);

        writer.clear();
        let (_, status) = lz77_compress_block_finish(
            &data[bytes_consumed..],
            &mut state,
            &mut buffer,
            &mut writer,
        );
        assert_eq!(status, LZ77Status::EndBlock);

    }

    #[test]
    fn multiple_inputs() {
        let data = b"Badger badger bababa test data 25 asfgestghresjkgh";
        let comp1 = lz77_compress(data).unwrap();
        let comp2 = {
            const SPLIT: usize = 25;
            let first_part = &data[..SPLIT];
            let second_part = &data[SPLIT..];
            let mut state = TestStruct::new();
            let (bytes_written, status, _) = state.compress_block(first_part, false);
            assert_eq!(bytes_written, first_part.len());
            assert_eq!(status, LZ77Status::NeedInput);
            let (bytes_written, status, _) = state.compress_block(second_part, true);
            assert_eq!(bytes_written, second_part.len());
            assert_eq!(status, LZ77Status::Finished);
            Vec::from(state.writer.get_buffer())
        };
        assert!(comp1 == comp2);
    }


    #[test]
    /// Test that the exit from process_chunk when buffer is full is working correctly.
    fn buffer_fill() {
        let data = get_test_data();
        // The comments above these calls refers the positions with the
        // corersponding comments in process_chunk_{greedy/lazy}.
        // POS BETTER OR NO MATCH
        buffer_test_literals(&data);
        // POS END
        // POS NO MATCH
        buffer_test_last_bytes(MatchingType::Greedy, &data);
        // POS ADD
        // POS AFTER ADD
        buffer_test_last_bytes(MatchingType::Lazy, &data);

        // POS MATCH
        buffer_test_match(MatchingType::Greedy);
        // POS MATCH(lazy)
        buffer_test_match(MatchingType::Lazy);

        // POS END
        buffer_test_add_end(&data);
    }

    /// Test buffer fill when a byte is added due to no match being found.
    fn buffer_test_literals(data: &[u8]) {
        let mut state = TestStruct::with_config(0, NO_RLE, MatchingType::Lazy);
        let (bytes_consumed, status, position) = state.compress_block(&data, false);

        // There should be enough data for the block to have ended.
        assert_eq!(status, LZ77Status::EndBlock);
        assert!(bytes_consumed <= (WINDOW_SIZE * 2) + MAX_MATCH);

        // The buffer should be full.
        assert_eq!(state.writer.get_buffer().len(), MAX_BUFFER_LENGTH);
        assert_eq!(position, state.writer.get_buffer().len());
        // Since all literals have been input, the block should have the exact number of litlens
        // as there were input bytes.
        assert_eq!(
            state.state.current_block_input_bytes() as usize,
            MAX_BUFFER_LENGTH
        );
        state.state.reset_input_bytes();

        let mut out = decompress_lz77(state.writer.get_buffer());

        state.writer.clear();
        // The buffer should now be cleared.
        assert_eq!(state.writer.get_buffer().len(), 0);

        assert!(data[..out.len()] == out[..]);

        let _ = state.compress_block(&data[bytes_consumed..], false);
        // We should have some new data in the buffer at this point.
        assert!(state.writer.get_buffer().len() > 0);
        assert_eq!(
            state.state.current_block_input_bytes() as usize,
            MAX_BUFFER_LENGTH
        );

        out.extend_from_slice(&decompress_lz77(state.writer.get_buffer()));
        assert!(data[..out.len()] == out[..]);
    }

    /// Test buffer fill at the last two bytes that are not hashed.
    fn buffer_test_last_bytes(matching_type: MatchingType, data: &[u8]) {
        const BYTES_USED: usize = MAX_BUFFER_LENGTH;
        assert!(
            &data[..BYTES_USED] ==
                &decompress_lz77(&lz77_compress_conf(
                    &data[..BYTES_USED],
                    0,
                    NO_RLE,
                    matching_type,
                ).unwrap())
                    [..]
        );
        assert!(
            &data[..BYTES_USED + 1] ==
                &decompress_lz77(&lz77_compress_conf(
                    &data[..BYTES_USED + 1],
                    0,
                    NO_RLE,
                    matching_type,
                ).unwrap())
                    [..]
        );
    }

    /// Test buffer fill when buffer is full at a match.
    fn buffer_test_match(matching_type: MatchingType) {
        // TODO: Also test this for the second block to make sure
        // buffer is slid.
        let mut state = TestStruct::with_config(1, 0, matching_type);
        for _ in 0..MAX_BUFFER_LENGTH - 4 {
            assert!(state.writer.write_literal(0) == BufferStatus::NotFull);
        }
        state.compress_block(&[1, 2, 3, 1, 2, 3, 4], true);
        assert!(*state.writer.get_buffer().last().unwrap() == LZValue::length_distance(3, 3));

    }

    /// Test buffer fill for the lazy match algorithm when adding a pending byte at the end.
    fn buffer_test_add_end(_data: &[u8]) {
        // This is disabled while the buffer size has not been stabilized.
        /*
        let mut state = TestStruct::with_config(DEFAULT_MAX_HASH_CHECKS,
                                                DEFAULT_LAZY_IF_LESS_THAN,
                                                MatchingType::Lazy);
        // For the test file, this is how much data needs to be added to get the buffer
        // full at the right spot to test that this buffer full exit is workong correctly.
        for i in 0..31743 {
            assert!(state.writer.write_literal(0) == BufferStatus::NotFull, "Buffer pos: {}", i);
        }

        let dec = decompress_lz77(&state.writer.get_buffer()[pos..]);
        assert!(dec.len() > 0);
        assert!(dec[..] == data[..dec.len()]);
         */
    }

    /// Check that decompressing lz77-data that refers to the back-buffer works.
    #[test]
    fn test_decompress_with_backbuffer() {
        let bb = vec![5; WINDOW_SIZE];
        let lz_compressed = [lit(2), lit(4), ld(4, 20), lit(1), lit(1), ld(5, 10)];
        let dec = decompress_lz77_with_backbuffer(&lz_compressed, &bb);

        // ------------l2 l4  <-ld4,20-> l1 l1  <---ld5,10-->
        assert!(dec == [2, 4, 5, 5, 5, 5, 1, 1, 5, 5, 2, 4, 5]);
    }

}

#[cfg(all(test, feature = "benchmarks"))]
mod bench {
    use test_std::Bencher;
    use test_utils::get_test_data;
    use super::lz77_compress;
    #[bench]
    fn test_file_zlib_lz77_only(b: &mut Bencher) {
        let test_data = get_test_data();
        b.iter(|| lz77_compress(&test_data));
    }
}

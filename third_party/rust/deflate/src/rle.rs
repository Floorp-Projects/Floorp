use lz77::{ProcessStatus, buffer_full};
use output_writer::{BufferStatus, DynamicWriter};
use huffman_table;

use std::ops::Range;
use std::cmp;

const MIN_MATCH: usize = huffman_table::MIN_MATCH as usize;
const MAX_MATCH: usize = huffman_table::MAX_MATCH as usize;


/// Simple match function for run-length encoding.
///
/// Checks how many of the next bytes from the start of the slice `data` matches prev.
fn get_match_length_rle(data: &[u8], prev: u8) -> usize {
    data.iter()
        .take(MAX_MATCH)
        .take_while(|&&b| b == prev)
        .count()
}

/// L77-Compress data using the RLE(Run-length encoding) strategy
///
/// This function simply looks for runs of data of at least length 3.
pub fn process_chunk_greedy_rle(
    data: &[u8],
    iterated_data: &Range<usize>,
    writer: &mut DynamicWriter,
) -> (usize, ProcessStatus) {
    if data.is_empty() {
        return (0, ProcessStatus::Ok);
    };

    let end = cmp::min(data.len(), iterated_data.end);
    // Start on at least byte 1.
    let start = cmp::max(iterated_data.start, 1);
    // The previous byte.
    let mut prev = data[start - 1];
    // Iterate through the requested range, but avoid going off the end.
    let current_chunk = &data[cmp::min(start, end)..end];
    let mut insert_it = current_chunk.iter().enumerate();
    let mut overlap = 0;
    // Make sure to output the first byte
    if iterated_data.start == 0 && !data.is_empty() {
        write_literal!(writer, data[0], 1);
    }

    while let Some((n, &b)) = insert_it.next() {
        let position = n + start;
        let match_len = if prev == b {
            //TODO: Avoid comparing with self here.
            // Would use as_slice() but that doesn't work on an enumerated iterator.
            get_match_length_rle(&data[position..], prev)
        } else {
            0
        };
        if match_len >= MIN_MATCH {
            if position + match_len > end {
                overlap = position + match_len - end;
            };
            let b_status = writer.write_length_rle(match_len as u16);
            if b_status == BufferStatus::Full {
                return (overlap, buffer_full(position + match_len));
            }
            insert_it.nth(match_len - 2);
        } else {
            write_literal!(writer, b, position + 1);
        }
        prev = b;
    }

    (overlap, ProcessStatus::Ok)
}

#[cfg(test)]
mod test {
    use super::*;
    use lzvalue::{LZValue, lit, ld};

    fn l(c: char) -> LZValue {
        lit(c as u8)
    }

    #[test]
    fn rle_compress() {
        let input = b"textaaaaaaaaatext";
        let mut w = DynamicWriter::new();
        let r = 0..input.len();
        let (overlap, _) = process_chunk_greedy_rle(input, &r, &mut w);
        let expected = [
            l('t'),
            l('e'),
            l('x'),
            l('t'),
            l('a'),
            ld(8, 1),
            l('t'),
            l('e'),
            l('x'),
            l('t'),
        ];
        //println!("expected: {:?}", expected);
        //println!("actual: {:?}", w.get_buffer());
        assert!(w.get_buffer() == expected);
        assert_eq!(overlap, 0);
    }
}

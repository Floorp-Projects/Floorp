use std::cmp;

use chained_hash_table::WINDOW_SIZE;
use huffman_table;

const MAX_MATCH: usize = huffman_table::MAX_MATCH as usize;

/// The maximum size of the buffer.
pub const BUFFER_SIZE: usize = (WINDOW_SIZE * 2) + MAX_MATCH;

pub struct InputBuffer {
    buffer: Vec<u8>,
}

impl InputBuffer {
    #[cfg(test)]
    pub fn new<'a>(data: &'a [u8]) -> (InputBuffer, Option<&[u8]>) {
        let mut b = InputBuffer::empty();
        let rem = b.add_data(data);
        (b, rem)
    }

    pub fn empty() -> InputBuffer {
        InputBuffer {
            buffer: Vec::with_capacity(BUFFER_SIZE),
        }
    }

    /// Add data to the buffer.
    ///
    /// Returns a slice of the data that was not added (including the lookahead if any).
    pub fn add_data<'a>(&mut self, data: &'a [u8]) -> Option<&'a [u8]> {
        debug_assert!(self.current_end() <= BUFFER_SIZE);
        if self.current_end() + data.len() > BUFFER_SIZE {
            // Add data and return how much was left.
            let consumed = {
                let space_left = BUFFER_SIZE - self.buffer.len();
                self.buffer.extend_from_slice(&data[..space_left]);
                space_left
            };
            Some(&data[consumed..])
        } else {
            // There's space for all of the data.
            self.buffer.extend_from_slice(data);
            None
        }
    }

    /// Get the current amount of data in the buffer.
    pub fn current_end(&self) -> usize {
        self.buffer.len()
    }

    /// Slide the input window and add new data.
    ///
    /// Returns a slice containing the data that did not fit, or None if all data was consumed.
    pub fn slide<'a>(&mut self, data: &'a [u8]) -> Option<&'a [u8]> {
        // This should only be used when the buffer is full
        assert!(self.buffer.len() > WINDOW_SIZE * 2);

        // Do this in a closure to to end the borrow of buffer.
        let (final_len, upper_len, end) = {
            // Split into lower window and upper window + lookahead
            let (lower, upper) = self.buffer.split_at_mut(WINDOW_SIZE);
            // Copy the upper window to the lower window
            lower.copy_from_slice(&upper[..WINDOW_SIZE]);
            let lookahead_len = {
                // Copy the lookahead to the start of the upper window
                let (upper_2, lookahead) = upper.split_at_mut(WINDOW_SIZE);
                let lookahead_len = lookahead.len();
                debug_assert!(lookahead_len <= MAX_MATCH);
                upper_2[..lookahead_len].copy_from_slice(lookahead);
                lookahead_len
            };

            // Length of the upper window minus the lookahead bytes
            let upper_len = upper.len() - lookahead_len;
            let end = cmp::min(data.len(), upper_len);
            upper[lookahead_len..lookahead_len + end].copy_from_slice(&data[..end]);
            // Remove unused data if any.
            (lower.len() + lookahead_len + end, upper_len, end)
        };
        // Remove unused space.
        self.buffer.truncate(final_len);

        if data.len() > upper_len {
            // Return a slice of the data that was not added
            Some(&data[end..])
        } else {
            None
        }
    }

    /// Get a mutable slice of the used part of the buffer.
    pub fn get_buffer(&mut self) -> &mut [u8] {
        &mut self.buffer
    }
}

#[cfg(test)]
mod test {
    use super::MAX_MATCH;
    use chained_hash_table::WINDOW_SIZE;
    use super::*;
    #[test]
    pub fn buffer_add_full() {
        let data = [10u8; BUFFER_SIZE + 10];
        let (mut buf, extra) = InputBuffer::new(&data[..]);
        assert!(extra.unwrap() == &[10; 10]);
        let to_add = [2, 5, 3];
        let not_added = buf.add_data(&to_add);
        assert_eq!(not_added.unwrap(), to_add);
    }

    #[test]
    pub fn buffer_add_not_full() {
        let data = [10u8; BUFFER_SIZE - 5];
        let (mut buf, extra) = InputBuffer::new(&data[..]);
        assert_eq!(buf.current_end(), data.len());
        assert_eq!(extra, None);
        let to_add = [2, 5, 3];
        {
            let not_added = buf.add_data(&to_add);
            assert!(not_added.is_none());
        }
        let not_added = buf.add_data(&to_add);
        assert_eq!(not_added.unwrap()[0], 3);
    }

    #[test]
    fn slide() {
        let data = [10u8; BUFFER_SIZE];
        let (mut buf, extra) = InputBuffer::new(&data[..]);
        assert_eq!(extra, None);
        let to_add = [5; 5];
        let rem = buf.slide(&to_add);
        assert!(rem.is_none());
        {
            let slice = buf.get_buffer();
            assert!(slice[..WINDOW_SIZE + MAX_MATCH] == data[WINDOW_SIZE..]);
            assert_eq!(
                slice[WINDOW_SIZE + MAX_MATCH..WINDOW_SIZE + MAX_MATCH + 5],
                to_add
            );
        }
        assert_eq!(buf.current_end(), WINDOW_SIZE + MAX_MATCH + to_add.len());
    }
}

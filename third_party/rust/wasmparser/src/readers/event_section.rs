/* Copyright 2020 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use super::{
    BinaryReader, EventType, Range, Result, SectionIteratorLimited, SectionReader,
    SectionWithLimitedItems,
};

#[derive(Clone)]
pub struct EventSectionReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> EventSectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<EventSectionReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(EventSectionReader { reader, count })
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    /// Reads content of the event section.
    ///
    /// # Examples
    /// ```
    /// use wasmparser::EventSectionReader;
    /// # let data: &[u8] = &[0x01, 0x00, 0x01];
    /// let mut event_reader = EventSectionReader::new(data, 0).unwrap();
    /// for _ in 0..event_reader.get_count() {
    ///     let et = event_reader.read().expect("event_type");
    ///     println!("Event: {:?}", et);
    /// }
    /// ```
    pub fn read(&mut self) -> Result<EventType> {
        self.reader.read_event_type()
    }
}

impl<'a> SectionReader for EventSectionReader<'a> {
    type Item = EventType;
    fn read(&mut self) -> Result<Self::Item> {
        EventSectionReader::read(self)
    }
    fn eof(&self) -> bool {
        self.reader.eof()
    }
    fn original_position(&self) -> usize {
        EventSectionReader::original_position(self)
    }
    fn range(&self) -> Range {
        self.reader.range()
    }
}

impl<'a> SectionWithLimitedItems for EventSectionReader<'a> {
    fn get_count(&self) -> u32 {
        EventSectionReader::get_count(self)
    }
}

impl<'a> IntoIterator for EventSectionReader<'a> {
    type Item = Result<EventType>;
    type IntoIter = SectionIteratorLimited<EventSectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIteratorLimited::new(self)
    }
}

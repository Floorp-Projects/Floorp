/* Copyright 2018 Mozilla Foundation
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
    BinaryReader, BinaryReaderError, InitExpr, Result, SectionIteratorLimited, SectionReader,
    SectionWithLimitedItems,
};

#[derive(Debug, Copy, Clone)]
pub struct Data<'a> {
    pub memory_index: u32,
    pub init_expr: InitExpr<'a>,
    pub data: &'a [u8],
}

pub struct DataSectionReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> DataSectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<DataSectionReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(DataSectionReader { reader, count })
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    fn verify_data_end(&self, end: usize) -> Result<()> {
        if self.reader.buffer.len() < end {
            return Err(BinaryReaderError {
                message: "Data segment extends past end of the data section",
                offset: self.reader.original_offset + self.reader.buffer.len(),
            });
        }
        Ok(())
    }

    /// Reads content of the data section.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x05, 0x03, 0x01, 0x00, 0x02,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b,
    /// #     0x0b, 0x0b, 0x01, 0x00, 0x41, 0x80, 0x08, 0x0b, 0x04, 0x00, 0x00, 0x00, 0x00];
    /// use wasmparser::ModuleReader;
    /// let mut reader = ModuleReader::new(data).expect("module reader");
    /// let section = reader.read().expect("type section");
    /// let section = reader.read().expect("function section");
    /// let section = reader.read().expect("memory section");
    /// let section = reader.read().expect("code section");
    /// let section = reader.read().expect("data section");
    /// let mut data_reader = section.get_data_section_reader().expect("data section reader");
    /// for _ in 0..data_reader.get_count() {
    ///     let data = data_reader.read().expect("data");
    ///     println!("Data: {:?}", data);
    ///     let mut init_expr_reader = data.init_expr.get_binary_reader();
    ///     let op = init_expr_reader.read_operator().expect("op");
    ///     println!("Init const: {:?}", op);
    /// }
    /// ```
    pub fn read<'b>(&mut self) -> Result<Data<'b>>
    where
        'a: 'b,
    {
        let memory_index = self.reader.read_var_u32()?;
        let init_expr = {
            let expr_offset = self.reader.position;
            self.reader.skip_init_expr()?;
            let data = &self.reader.buffer[expr_offset..self.reader.position];
            InitExpr::new(data, self.reader.original_offset + expr_offset)
        };
        let data_len = self.reader.read_var_u32()? as usize;
        let data_end = self.reader.position + data_len;
        self.verify_data_end(data_end)?;
        let data = &self.reader.buffer[self.reader.position..data_end];
        self.reader.skip_to(data_end);
        Ok(Data {
            memory_index,
            init_expr,
            data,
        })
    }
}

impl<'a> SectionReader for DataSectionReader<'a> {
    type Item = Data<'a>;
    fn read(&mut self) -> Result<Self::Item> {
        DataSectionReader::read(self)
    }
    fn eof(&self) -> bool {
        self.reader.eof()
    }
    fn original_position(&self) -> usize {
        DataSectionReader::original_position(self)
    }
}

impl<'a> SectionWithLimitedItems for DataSectionReader<'a> {
    fn get_count(&self) -> u32 {
        DataSectionReader::get_count(self)
    }
}

impl<'a> IntoIterator for DataSectionReader<'a> {
    type Item = Result<Data<'a>>;
    type IntoIter = SectionIteratorLimited<DataSectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIteratorLimited::new(self)
    }
}

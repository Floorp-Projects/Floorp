use super::{
    BinaryReader, Range, Result, SectionIteratorLimited, SectionReader, SectionWithLimitedItems,
};

#[derive(Clone)]
pub struct ModuleSectionReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> ModuleSectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<ModuleSectionReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(ModuleSectionReader { reader, count })
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    pub fn read(&mut self) -> Result<u32> {
        self.reader.read_var_u32()
    }
}

impl<'a> SectionReader for ModuleSectionReader<'a> {
    type Item = u32;

    fn read(&mut self) -> Result<Self::Item> {
        ModuleSectionReader::read(self)
    }
    fn eof(&self) -> bool {
        self.reader.eof()
    }
    fn original_position(&self) -> usize {
        ModuleSectionReader::original_position(self)
    }
    fn range(&self) -> Range {
        self.reader.range()
    }
}

impl<'a> SectionWithLimitedItems for ModuleSectionReader<'a> {
    fn get_count(&self) -> u32 {
        ModuleSectionReader::get_count(self)
    }
}

impl<'a> IntoIterator for ModuleSectionReader<'a> {
    type Item = Result<u32>;
    type IntoIter = SectionIteratorLimited<ModuleSectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIteratorLimited::new(self)
    }
}

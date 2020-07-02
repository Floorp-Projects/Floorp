use crate::{
    BinaryReader, BinaryReaderError, ExternalKind, Result, SectionIteratorLimited, SectionReader,
    SectionWithLimitedItems,
};

pub struct AliasSectionReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

#[derive(Debug)]
pub struct Alias {
    pub instance: AliasedInstance,
    pub kind: ExternalKind,
    pub index: u32,
}

#[derive(Debug)]
pub enum AliasedInstance {
    Parent,
    Child(u32),
}

impl<'a> AliasSectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<AliasSectionReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(AliasSectionReader { reader, count })
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    pub fn read(&mut self) -> Result<Alias> {
        Ok(Alias {
            instance: match self.reader.read_u8()? {
                0x00 => AliasedInstance::Child(self.reader.read_var_u32()?),
                0x01 => AliasedInstance::Parent,
                _ => {
                    return Err(BinaryReaderError::new(
                        "invalid byte in alias",
                        self.original_position() - 1,
                    ))
                }
            },
            kind: self.reader.read_external_kind()?,
            index: self.reader.read_var_u32()?,
        })
    }
}

impl<'a> SectionReader for AliasSectionReader<'a> {
    type Item = Alias;

    fn read(&mut self) -> Result<Self::Item> {
        AliasSectionReader::read(self)
    }
    fn eof(&self) -> bool {
        self.reader.eof()
    }
    fn original_position(&self) -> usize {
        AliasSectionReader::original_position(self)
    }
}

impl<'a> SectionWithLimitedItems for AliasSectionReader<'a> {
    fn get_count(&self) -> u32 {
        AliasSectionReader::get_count(self)
    }
}

impl<'a> IntoIterator for AliasSectionReader<'a> {
    type Item = Result<Alias>;
    type IntoIter = SectionIteratorLimited<AliasSectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIteratorLimited::new(self)
    }
}

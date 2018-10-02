use compact_symbol_table::CompactSymbolTable;
use object::{ElfFile, Object, SymbolKind, Uuid};
use std::collections::HashMap;

fn get_symbol_map<'a, 'b, T>(object_file: &'b T) -> HashMap<u32, &'a str>
where
  T: Object<'a, 'b>,
{
  object_file
    .dynamic_symbols()
    .chain(object_file.symbols())
    .filter(|symbol| symbol.kind() == SymbolKind::Text)
    .filter_map(|symbol| symbol.name().map(|name| (symbol.address() as u32, name)))
    .collect()
}

pub fn get_compact_symbol_table(buffer: &[u8], breakpad_id: &str) -> Option<CompactSymbolTable> {
  let elf_file = ElfFile::parse(buffer).ok()?;
  let elf_id = Uuid::from_bytes(elf_file.build_id()?).ok()?;
  if format!("{:X}0", elf_id.simple()) != breakpad_id {
    return None;
  }
  return Some(CompactSymbolTable::from_map(get_symbol_map(&elf_file)));
}

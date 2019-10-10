#![cfg(all(feature = "read", feature = "write"))]

use object::read::{Object, ObjectSection};
use object::{read, write};
use object::{RelocationEncoding, RelocationKind, SectionKind, SymbolKind, SymbolScope};
use target_lexicon::{Architecture, BinaryFormat};

#[test]
fn coff_x86_64() {
    let mut object = write::Object::new(BinaryFormat::Coff, Architecture::X86_64);

    let text = object.section_id(write::StandardSection::Text);
    object.append_section_data(text, &[1; 30], 4);

    let func1_offset = object.append_section_data(text, &[1; 30], 4);
    assert_eq!(func1_offset, 32);
    let func1_symbol = object.add_symbol(write::Symbol {
        name: b"func1".to_vec(),
        value: func1_offset,
        size: 32,
        kind: SymbolKind::Text,
        scope: SymbolScope::Linkage,
        weak: false,
        section: Some(text),
    });
    object
        .add_relocation(
            text,
            write::Relocation {
                offset: 8,
                size: 64,
                kind: RelocationKind::Absolute,
                encoding: RelocationEncoding::Generic,
                symbol: func1_symbol,
                addend: 0,
            },
        )
        .unwrap();

    let bytes = object.write().unwrap();
    let object = read::File::parse(&bytes).unwrap();
    assert_eq!(object.format(), BinaryFormat::Coff);
    assert_eq!(object.architecture(), Architecture::X86_64);

    let mut sections = object.sections();

    let text = sections.next().unwrap();
    println!("{:?}", text);
    let text_index = text.index();
    assert_eq!(text.name(), Some(".text"));
    assert_eq!(text.kind(), SectionKind::Text);
    assert_eq!(text.address(), 0);
    assert_eq!(text.size(), 62);
    assert_eq!(&text.data()[..30], &[1; 30]);
    assert_eq!(&text.data()[32..62], &[1; 30]);

    let mut symbols = object.symbols();

    let (func1_symbol, symbol) = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Some("_func1"));
    assert_eq!(symbol.address(), func1_offset);
    assert_eq!(symbol.kind(), SymbolKind::Text);
    assert_eq!(symbol.section_index(), Some(text_index));
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert_eq!(symbol.is_weak(), false);
    assert_eq!(symbol.is_undefined(), false);

    let mut relocations = text.relocations();

    let (offset, relocation) = relocations.next().unwrap();
    println!("{:?}", relocation);
    assert_eq!(offset, 8);
    assert_eq!(relocation.kind(), RelocationKind::Absolute);
    assert_eq!(relocation.encoding(), RelocationEncoding::Generic);
    assert_eq!(relocation.size(), 64);
    assert_eq!(
        relocation.target(),
        read::RelocationTarget::Symbol(func1_symbol)
    );
    assert_eq!(relocation.addend(), 0);
}

#[test]
fn elf_x86_64() {
    let mut object = write::Object::new(BinaryFormat::Elf, Architecture::X86_64);

    let text = object.section_id(write::StandardSection::Text);
    object.append_section_data(text, &[1; 30], 4);

    let func1_offset = object.append_section_data(text, &[1; 30], 4);
    assert_eq!(func1_offset, 32);
    let func1_symbol = object.add_symbol(write::Symbol {
        name: b"func1".to_vec(),
        value: func1_offset,
        size: 32,
        kind: SymbolKind::Text,
        scope: SymbolScope::Linkage,
        weak: false,
        section: Some(text),
    });
    object
        .add_relocation(
            text,
            write::Relocation {
                offset: 8,
                size: 64,
                kind: RelocationKind::Absolute,
                encoding: RelocationEncoding::Generic,
                symbol: func1_symbol,
                addend: 0,
            },
        )
        .unwrap();

    let bytes = object.write().unwrap();
    let object = read::File::parse(&bytes).unwrap();
    assert_eq!(object.format(), BinaryFormat::Elf);
    assert_eq!(object.architecture(), Architecture::X86_64);

    let mut sections = object.sections();

    let section = sections.next().unwrap();
    println!("{:?}", text);
    assert_eq!(section.name(), Some(""));
    assert_eq!(section.kind(), SectionKind::Metadata);
    assert_eq!(section.address(), 0);
    assert_eq!(section.size(), 0);

    let text = sections.next().unwrap();
    println!("{:?}", text);
    let text_index = text.index();
    assert_eq!(text.name(), Some(".text"));
    assert_eq!(text.kind(), SectionKind::Text);
    assert_eq!(text.address(), 0);
    assert_eq!(text.size(), 62);
    assert_eq!(&text.data()[..30], &[1; 30]);
    assert_eq!(&text.data()[32..62], &[1; 30]);

    let mut symbols = object.symbols();

    let (_, symbol) = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Some(""));
    assert_eq!(symbol.address(), 0);
    assert_eq!(symbol.kind(), SymbolKind::Null);
    assert_eq!(symbol.section_index(), None);
    assert_eq!(symbol.scope(), SymbolScope::Unknown);
    assert_eq!(symbol.is_weak(), false);
    assert_eq!(symbol.is_undefined(), true);

    let (func1_symbol, symbol) = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Some("func1"));
    assert_eq!(symbol.address(), func1_offset);
    assert_eq!(symbol.kind(), SymbolKind::Text);
    assert_eq!(symbol.section_index(), Some(text_index));
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert_eq!(symbol.is_weak(), false);
    assert_eq!(symbol.is_undefined(), false);

    let mut relocations = text.relocations();

    let (offset, relocation) = relocations.next().unwrap();
    println!("{:?}", relocation);
    assert_eq!(offset, 8);
    assert_eq!(relocation.kind(), RelocationKind::Absolute);
    assert_eq!(relocation.encoding(), RelocationEncoding::Generic);
    assert_eq!(relocation.size(), 64);
    assert_eq!(
        relocation.target(),
        read::RelocationTarget::Symbol(func1_symbol)
    );
    assert_eq!(relocation.addend(), 0);
}

#[test]
fn macho_x86_64() {
    let mut object = write::Object::new(BinaryFormat::Macho, Architecture::X86_64);

    let text = object.section_id(write::StandardSection::Text);
    object.append_section_data(text, &[1; 30], 4);

    let func1_offset = object.append_section_data(text, &[1; 30], 4);
    assert_eq!(func1_offset, 32);
    let func1_symbol = object.add_symbol(write::Symbol {
        name: b"func1".to_vec(),
        value: func1_offset,
        size: 32,
        kind: SymbolKind::Text,
        scope: SymbolScope::Linkage,
        weak: false,
        section: Some(text),
    });
    object
        .add_relocation(
            text,
            write::Relocation {
                offset: 8,
                size: 64,
                kind: RelocationKind::Absolute,
                encoding: RelocationEncoding::Generic,
                symbol: func1_symbol,
                addend: 0,
            },
        )
        .unwrap();
    object
        .add_relocation(
            text,
            write::Relocation {
                offset: 16,
                size: 32,
                kind: RelocationKind::Relative,
                encoding: RelocationEncoding::Generic,
                symbol: func1_symbol,
                addend: -4,
            },
        )
        .unwrap();

    let bytes = object.write().unwrap();
    let object = read::File::parse(&bytes).unwrap();
    assert_eq!(object.format(), BinaryFormat::Macho);
    assert_eq!(object.architecture(), Architecture::X86_64);

    let mut sections = object.sections();

    let text = sections.next().unwrap();
    println!("{:?}", text);
    let text_index = text.index();
    assert_eq!(text.name(), Some("__text"));
    assert_eq!(text.segment_name(), Some("__TEXT"));
    assert_eq!(text.kind(), SectionKind::Text);
    assert_eq!(text.address(), 0);
    assert_eq!(text.size(), 62);
    assert_eq!(&text.data()[..30], &[1; 30]);
    assert_eq!(&text.data()[32..62], &[1; 30]);

    let mut symbols = object.symbols();

    let (func1_symbol, symbol) = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Some("_func1"));
    assert_eq!(symbol.address(), func1_offset);
    assert_eq!(symbol.kind(), SymbolKind::Text);
    assert_eq!(symbol.section_index(), Some(text_index));
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert_eq!(symbol.is_weak(), false);
    assert_eq!(symbol.is_undefined(), false);

    let mut relocations = text.relocations();

    let (offset, relocation) = relocations.next().unwrap();
    println!("{:?}", relocation);
    assert_eq!(offset, 8);
    assert_eq!(relocation.kind(), RelocationKind::Absolute);
    assert_eq!(relocation.encoding(), RelocationEncoding::Generic);
    assert_eq!(relocation.size(), 64);
    assert_eq!(
        relocation.target(),
        read::RelocationTarget::Symbol(func1_symbol)
    );
    assert_eq!(relocation.addend(), 0);

    let (offset, relocation) = relocations.next().unwrap();
    println!("{:?}", relocation);
    assert_eq!(offset, 16);
    assert_eq!(relocation.kind(), RelocationKind::Relative);
    assert_eq!(relocation.encoding(), RelocationEncoding::X86RipRelative);
    assert_eq!(relocation.size(), 32);
    assert_eq!(
        relocation.target(),
        read::RelocationTarget::Symbol(func1_symbol)
    );
    assert_eq!(relocation.addend(), -4);
}

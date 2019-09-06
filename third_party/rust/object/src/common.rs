/// The kind of a section.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SectionKind {
    /// The section kind is unknown.
    Unknown,
    /// An executable code section.
    ///
    /// Example ELF sections: `.text`
    ///
    /// Example Mach-O sections: `__TEXT/__text`
    Text,
    /// A data section.
    ///
    /// Example ELF sections: `.data`
    ///
    /// Example Mach-O sections: `__DATA/__data`
    Data,
    /// A read only data section.
    ///
    /// Example ELF sections: `.rodata`
    ///
    /// Example Mach-O sections: `__TEXT/__const`, `__DATA/__const`
    ReadOnlyData,
    /// A loadable string section.
    ///
    /// Example ELF sections: `.rodata.str`
    ///
    /// Example Mach-O sections: `__TEXT/__cstring`
    ReadOnlyString,
    /// An uninitialized data section.
    ///
    /// Example ELF sections: `.bss`
    ///
    /// Example Mach-O sections: `__DATA/__bss`
    UninitializedData,
    /// A TLS data section.
    ///
    /// Example ELF sections: `.tdata`
    ///
    /// Example Mach-O sections: `__DATA/__thread_data`
    Tls,
    /// An uninitialized TLS data section.
    ///
    /// Example ELF sections: `.tbss`
    ///
    /// Example Mach-O sections: `__DATA/__thread_bss`
    UninitializedTls,
    /// A TLS variables section.
    ///
    /// This contains TLS variable structures, rather than the variable initializers.
    ///
    /// Example Mach-O sections: `__DATA/__thread_vars`
    TlsVariables,
    /// A non-loadable string section.
    ///
    /// Example ELF sections: `.comment`, `.debug_str`
    OtherString,
    /// Some other non-loadable section.
    ///
    /// Example ELF sections: `.debug_info`
    Other,
    /// Debug information.
    ///
    /// Example Mach-O sections: `__DWARF/__debug_info`
    Debug,
    /// Information for the linker.
    ///
    /// Example COFF sections: `.drectve`
    Linker,
    /// Metadata such as symbols or relocations.
    ///
    /// Example ELF sections: `.symtab`, `.strtab`
    Metadata,
}

/// The kind of a symbol.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SymbolKind {
    /// The symbol kind is unknown.
    Unknown,
    /// The symbol is a null placeholder.
    Null,
    /// The symbol is for executable code.
    Text,
    /// The symbol is for a data object.
    Data,
    /// The symbol is for a section.
    Section,
    /// The symbol is the name of a file. It precedes symbols within that file.
    File,
    /// The symbol is for a code label.
    Label,
    /// The symbol is for an uninitialized common block.
    Common,
    /// The symbol is for a thread local storage entity.
    Tls,
}

/// A symbol scope.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SymbolScope {
    /// Unknown scope.
    Unknown,
    /// Symbol is visible to the compilation unit.
    Compilation,
    /// Symbol is visible to the static linkage unit.
    Linkage,
    /// Symbol is visible to dynamically linked objects.
    Dynamic,
}

/// The operation used to calculate the result of the relocation.
///
/// The relocation descriptions use the following definitions. Note that
/// these definitions probably don't match any ELF ABI.
///
/// * A - The value of the addend.
/// * G - The address of the symbol's entry within the global offset table.
/// * L - The address of the symbol's entry within the procedure linkage table.
/// * P - The address of the place of the relocation.
/// * S - The address of the symbol.
/// * GotBase - The address of the global offset table.
/// * Image - The base address of the image.
/// * Section - The address of the section containing the symbol.
///
/// 'XxxRelative' means 'Xxx + A - P'.  'XxxOffset' means 'S + A - Xxx'.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RelocationKind {
    /// S + A
    Absolute,
    /// S + A - P
    Relative,
    /// G + A - GotBase
    Got,
    /// G + A - P
    GotRelative,
    /// GotBase + A - P
    GotBaseRelative,
    /// S + A - GotBase
    GotBaseOffset,
    /// L + A - P
    PltRelative,
    /// S + A - Image
    ImageOffset,
    /// S + A - Section
    SectionOffset,
    /// The index of the section containing the symbol.
    SectionIndex,
    /// Some other operation and encoding. The value is dependent on file format and machine.
    Other(u32),
}

/// Information about how the result of the relocation operation is encoded in the place.
///
/// This is usually architecture specific, such as specifying an addressing mode or
/// a specific instruction.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RelocationEncoding {
    /// Generic encoding.
    Generic,

    /// x86 sign extension at runtime.
    ///
    /// Used with `RelocationKind::Absolute`.
    X86Signed,
    /// x86 rip-relative addressing.
    ///
    /// The `RelocationKind` must be PC relative.
    X86RipRelative,
    /// x86 rip-relative addressing in movq instruction.
    ///
    /// The `RelocationKind` must be PC relative.
    X86RipRelativeMovq,
    /// x86 branch instruction.
    ///
    /// The `RelocationKind` must be PC relative.
    X86Branch,
}

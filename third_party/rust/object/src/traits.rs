use alloc::borrow::Cow;
use {Uuid, Machine, SectionKind, Symbol, SymbolMap};

/// An object file.
pub trait Object<'data, 'file> {
    /// A segment in the object file.
    type Segment: ObjectSegment<'data>;

    /// An iterator over the segments in the object file.
    type SegmentIterator: Iterator<Item = Self::Segment>;

    /// A section in the object file.
    type Section: ObjectSection<'data>;

    /// An iterator over the sections in the object file.
    type SectionIterator: Iterator<Item = Self::Section>;

    /// An iterator over the symbols in the object file.
    type SymbolIterator: Iterator<Item = Symbol<'data>>;

    /// Get the machine type of the file.
    fn machine(&self) -> Machine;

    /// Get an iterator over the segments in the file.
    fn segments(&'file self) -> Self::SegmentIterator;

    /// Get the entry point address of the binary
    fn entry(&'file self) -> u64;

    /// Get the contents of the section named `section_name`, if such
    /// a section exists.
    ///
    /// If `section_name` starts with a '.' then it is treated as a system section name,
    /// and is compared using the conventions specific to the object file format.
    /// For example, if ".text" is requested for a Mach-O object file, then the actual
    /// section name that is searched for is "__text".
    ///
    /// For some object files, multiple segments may contain sections with the same
    /// name. In this case, the first matching section will be used.
    ///
    /// This may decompress section data.
    fn section_data_by_name(&self, section_name: &str) -> Option<Cow<'data, [u8]>>;

    /// Get an iterator over the sections in the file.
    fn sections(&'file self) -> Self::SectionIterator;

    /// Get an iterator over the debugging symbols in the file.
    fn symbols(&'file self) -> Self::SymbolIterator;

    /// Get an iterator over the dynamic linking symbols in the file.
    fn dynamic_symbols(&'file self) -> Self::SymbolIterator;

    /// Construct a map from addresses to symbols.
    fn symbol_map(&self) -> SymbolMap<'data>;

    /// Return true if the file is little endian, false if it is big endian.
    fn is_little_endian(&self) -> bool;

    /// Return true if the file contains debug information sections, false if not.
    fn has_debug_symbols(&self) -> bool;

    /// The UUID from a Mach-O `LC_UUID` load command.
    #[inline]
    fn mach_uuid(&self) -> Option<Uuid> {
        None
    }

    /// The build ID from an ELF `NT_GNU_BUILD_ID` note.
    #[inline]
    fn build_id(&self) -> Option<&'data [u8]> {
        None
    }

    /// The filename and CRC from a `.gnu_debuglink` section.
    #[inline]
    fn gnu_debuglink(&self) -> Option<(&'data [u8], u32)> {
        None
    }
}

/// A loadable segment defined in an object file.
///
/// For ELF, this is a program header with type `PT_LOAD`.
/// For Mach-O, this is a load command with type `LC_SEGMENT` or `LC_SEGMENT_64`.
pub trait ObjectSegment<'data> {
    /// Returns the virtual address of the segment.
    fn address(&self) -> u64;

    /// Returns the size of the segment in memory.
    fn size(&self) -> u64;

    /// Returns a reference to the file contents of the segment.
    /// The length of this data may be different from the size of the
    /// segment in memory.
    fn data(&self) -> &'data [u8];

    /// Returns the name of the segment.
    fn name(&self) -> Option<&str>;
}

/// A section defined in an object file.
pub trait ObjectSection<'data> {
    /// Returns the address of the section.
    fn address(&self) -> u64;

    /// Returns the size of the section in memory.
    fn size(&self) -> u64;

    /// Returns a reference to the raw contents of the section.
    /// The length of this data may be different from the size of the
    /// section in memory.
    ///
    /// This does not do any decompression.
    fn data(&self) -> Cow<'data, [u8]>;

    /// Returns the name of the section.
    fn name(&self) -> Option<&str>;

    /// Returns the name of the segment for this section.
    fn segment_name(&self) -> Option<&str>;

    /// Return the kind of this section.
    fn kind(&self) -> SectionKind;
}

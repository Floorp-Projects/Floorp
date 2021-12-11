use super::*;

/// An encoder for the memory section.
///
/// # Example
///
/// ```
/// use wasm_encoder::{Module, MemorySection, MemoryType};
///
/// let mut memories = MemorySection::new();
/// memories.memory(MemoryType {
///     minimum: 1,
///     maximum: None,
///     memory64: false,
/// });
///
/// let mut module = Module::new();
/// module.section(&memories);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct MemorySection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl MemorySection {
    /// Create a new memory section encoder.
    pub fn new() -> MemorySection {
        MemorySection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many memories have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Define a memory.
    pub fn memory(&mut self, memory_type: MemoryType) -> &mut Self {
        memory_type.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Section for MemorySection {
    fn id(&self) -> u8 {
        SectionId::Memory.into()
    }

    fn encode<S>(&self, sink: &mut S)
    where
        S: Extend<u8>,
    {
        let num_added = encoders::u32(self.num_added);
        let n = num_added.len();
        sink.extend(
            encoders::u32(u32::try_from(n + self.bytes.len()).unwrap())
                .chain(num_added)
                .chain(self.bytes.iter().copied()),
        );
    }
}

/// A memory's type.
#[derive(Clone, Copy, Debug)]
pub struct MemoryType {
    /// Minimum size, in pages, of this memory
    pub minimum: u64,
    /// Maximum size, in pages, of this memory
    pub maximum: Option<u64>,
    /// Whether or not this is a 64-bit memory.
    pub memory64: bool,
}

impl MemoryType {
    pub(crate) fn encode(&self, bytes: &mut Vec<u8>) {
        let mut flags = 0;
        if self.maximum.is_some() {
            flags |= 0b001;
        }
        if self.memory64 {
            flags |= 0b100;
        }
        bytes.push(flags);
        bytes.extend(encoders::u64(self.minimum));
        if let Some(max) = self.maximum {
            bytes.extend(encoders::u64(max));
        }
    }
}

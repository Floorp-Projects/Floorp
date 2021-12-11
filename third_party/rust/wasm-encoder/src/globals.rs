use super::*;

/// An encoder for the global section.
///
/// # Example
///
/// ```
/// use wasm_encoder::{Module, GlobalSection, GlobalType, Instruction, ValType};
///
/// let mut globals = GlobalSection::new();
/// globals.global(
///     GlobalType {
///         val_type: ValType::I32,
///         mutable: false,
///     },
///     Instruction::I32Const(42),
/// );
///
/// let mut module = Module::new();
/// module.section(&globals);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct GlobalSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl GlobalSection {
    /// Create a new global section encoder.
    pub fn new() -> GlobalSection {
        GlobalSection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many globals have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Define a global.
    pub fn global(&mut self, global_type: GlobalType, init_expr: Instruction) -> &mut Self {
        global_type.encode(&mut self.bytes);
        init_expr.encode(&mut self.bytes);
        Instruction::End.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Section for GlobalSection {
    fn id(&self) -> u8 {
        SectionId::Global.into()
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

/// A global's type.
#[derive(Clone, Copy, Debug)]
pub struct GlobalType {
    /// This global's value type.
    pub val_type: ValType,
    /// Whether this global is mutable or not.
    pub mutable: bool,
}

impl GlobalType {
    pub(crate) fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.push(self.val_type.into());
        bytes.push(self.mutable as u8);
    }
}

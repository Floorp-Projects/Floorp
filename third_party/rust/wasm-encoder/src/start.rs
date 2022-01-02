use super::*;

/// An encoder for the start section.
///
/// # Example
///
/// Note: this doesn't actually define the function at index 0, its type, or its
/// code body, so the resulting Wasm module will be invalid. See `TypeSection`,
/// `FunctionSection`, and `CodeSection` for details on how to generate those
/// things.
///
/// ```
/// use wasm_encoder::{Module, StartSection};
///
/// let start = StartSection { function_index: 0 };
///
/// let mut module = Module::new();
/// module.section(&start);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Copy, Debug)]
pub struct StartSection {
    /// The index of the start function.
    pub function_index: u32,
}

impl Section for StartSection {
    fn id(&self) -> u8 {
        SectionId::Start.into()
    }

    fn encode<S>(&self, sink: &mut S)
    where
        S: Extend<u8>,
    {
        let f = encoders::u32(self.function_index);
        let n = f.len();
        sink.extend(encoders::u32(n as u32).chain(f));
    }
}

use super::*;

/// An encoder for the module section.
///
/// Note that this is part of the [module linking proposal][proposal] and is
/// not currently part of stable WebAssembly.
///
/// [proposal]: https://github.com/webassembly/module-linking
///
/// # Example
///
/// ```
/// use wasm_encoder::{ModuleSection, Module};
///
/// let mut modules = ModuleSection::new();
/// modules.module(&Module::new());
/// modules.module(&Module::new());
///
/// let mut module = Module::new();
/// module.section(&modules);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct ModuleSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl ModuleSection {
    /// Create a new code section encoder.
    pub fn new() -> ModuleSection {
        ModuleSection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many modules have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Writes a module into this module code section.
    pub fn module(&mut self, module: &Module) -> &mut Self {
        self.bytes.extend(
            encoders::u32(u32::try_from(module.bytes.len()).unwrap())
                .chain(module.bytes.iter().copied()),
        );
        self.num_added += 1;
        self
    }
}

impl Section for ModuleSection {
    fn id(&self) -> u8 {
        SectionId::Module.into()
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

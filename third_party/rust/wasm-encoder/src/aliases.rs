use super::*;

/// An encoder for the alias section.
///
/// Note that this is part of the [module linking proposal][proposal] and is not
/// currently part of stable WebAssembly.
///
/// [proposal]: https://github.com/webassembly/module-linking
///
/// # Example
///
/// ```
/// use wasm_encoder::{Module, AliasSection, ItemKind};
///
/// let mut aliases = AliasSection::new();
/// aliases.outer_type(0, 2);
/// aliases.instance_export(0, ItemKind::Function, "foo");
///
/// let mut module = Module::new();
/// module.section(&aliases);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct AliasSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl AliasSection {
    /// Construct a new alias section encoder.
    pub fn new() -> AliasSection {
        AliasSection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many aliases have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Define an alias that references the export of a defined instance.
    pub fn instance_export(
        &mut self,
        instance: u32,
        kind: crate::ItemKind,
        name: &str,
    ) -> &mut Self {
        self.bytes.push(0x00);
        self.bytes.extend(encoders::u32(instance));
        self.bytes.push(kind as u8);
        self.bytes.extend(encoders::str(name));
        self.num_added += 1;
        self
    }

    /// Define an alias that references an outer module's type.
    pub fn outer_type(&mut self, depth: u32, ty: u32) -> &mut Self {
        self.bytes.push(0x01);
        self.bytes.extend(encoders::u32(depth));
        self.bytes.push(0x07);
        self.bytes.extend(encoders::u32(ty));
        self.num_added += 1;
        self
    }

    /// Define an alias that references an outer module's module.
    pub fn outer_module(&mut self, depth: u32, module: u32) -> &mut Self {
        self.bytes.push(0x01);
        self.bytes.extend(encoders::u32(depth));
        self.bytes.push(ItemKind::Module as u8);
        self.bytes.extend(encoders::u32(module));
        self.num_added += 1;
        self
    }
}

impl Section for AliasSection {
    fn id(&self) -> u8 {
        SectionId::Alias.into()
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

use super::*;
use std::convert::TryFrom;

/// An encoder for the type section.
///
/// # Example
///
/// ```
/// use wasm_encoder::{Module, TypeSection, ValType};
///
/// let mut types = TypeSection::new();
/// let params = vec![ValType::I32, ValType::I64];
/// let results = vec![ValType::I32];
/// types.function(params, results);
///
/// let mut module = Module::new();
/// module.section(&types);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct TypeSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl TypeSection {
    /// Create a new type section encoder.
    pub fn new() -> TypeSection {
        TypeSection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many types have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Define a function type.
    pub fn function<P, R>(&mut self, params: P, results: R) -> &mut Self
    where
        P: IntoIterator<Item = ValType>,
        P::IntoIter: ExactSizeIterator,
        R: IntoIterator<Item = ValType>,
        R::IntoIter: ExactSizeIterator,
    {
        let params = params.into_iter();
        let results = results.into_iter();

        self.bytes.push(0x60);

        self.bytes
            .extend(encoders::u32(u32::try_from(params.len()).unwrap()));
        self.bytes.extend(params.map(|ty| u8::from(ty)));

        self.bytes
            .extend(encoders::u32(u32::try_from(results.len()).unwrap()));
        self.bytes.extend(results.map(|ty| u8::from(ty)));

        self.num_added += 1;
        self
    }

    /// Define a module type.
    pub fn module<'a, I, E>(&mut self, imports: I, exports: E) -> &mut Self
    where
        I: IntoIterator<Item = (&'a str, Option<&'a str>, EntityType)>,
        I::IntoIter: ExactSizeIterator,
        E: IntoIterator<Item = (&'a str, EntityType)>,
        E::IntoIter: ExactSizeIterator,
    {
        let exports = exports.into_iter();
        let imports = imports.into_iter();

        self.bytes.push(0x61);

        self.bytes
            .extend(encoders::u32(u32::try_from(imports.len()).unwrap()));
        for (module, name, ty) in imports {
            self.bytes.extend(encoders::str(module));
            match name {
                Some(name) => self.bytes.extend(encoders::str(name)),
                None => self.bytes.extend(&[0x00, 0xff]),
            }
            ty.encode(&mut self.bytes);
        }

        self.bytes
            .extend(encoders::u32(u32::try_from(exports.len()).unwrap()));
        for (name, ty) in exports {
            self.bytes.extend(encoders::str(name));
            ty.encode(&mut self.bytes);
        }

        self.num_added += 1;
        self
    }

    /// Define an instance type.
    pub fn instance<'a, E>(&mut self, exports: E) -> &mut Self
    where
        E: IntoIterator<Item = (&'a str, EntityType)>,
        E::IntoIter: ExactSizeIterator,
    {
        let exports = exports.into_iter();

        self.bytes.push(0x62);

        self.bytes
            .extend(encoders::u32(u32::try_from(exports.len()).unwrap()));
        for (name, ty) in exports {
            self.bytes.extend(encoders::str(name));
            ty.encode(&mut self.bytes);
        }

        self.num_added += 1;
        self
    }
}

impl Section for TypeSection {
    fn id(&self) -> u8 {
        SectionId::Type.into()
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

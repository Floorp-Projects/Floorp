use crate::{ComponentSection, ComponentSectionId, Encode};

/// An encoder for the start section of WebAssembly components.
///
/// # Example
///
/// ```
/// use wasm_encoder::{Component, ComponentStartSection};
///
/// let start = ComponentStartSection { function_index: 0, args: [] };
///
/// let mut component = Component::new();
/// component.section(&start);
///
/// let bytes = component.finish();
/// ```
#[derive(Clone, Debug)]
pub struct ComponentStartSection<A> {
    /// The index to the start function.
    pub function_index: u32,
    /// The arguments to pass to the start function.
    ///
    /// An argument is an index to a value.
    pub args: A,
}

impl<A> Encode for ComponentStartSection<A>
where
    A: AsRef<[u32]>,
{
    fn encode(&self, sink: &mut Vec<u8>) {
        let mut bytes = Vec::new();
        self.function_index.encode(&mut bytes);
        self.args.as_ref().encode(&mut bytes);
        bytes.encode(sink);
    }
}

impl<A> ComponentSection for ComponentStartSection<A>
where
    A: AsRef<[u32]>,
{
    fn id(&self) -> u8 {
        ComponentSectionId::Start.into()
    }
}

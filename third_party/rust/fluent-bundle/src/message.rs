use fluent_syntax::ast;

#[derive(Debug, PartialEq)]
pub struct FluentAttribute<'m> {
    pub id: &'m str,
    pub value: &'m ast::Pattern<&'m str>,
}
/// A single localization unit composed of an identifier,
/// value, and attributes.
#[derive(Debug, PartialEq)]
pub struct FluentMessage<'m> {
    pub value: Option<&'m ast::Pattern<&'m str>>,
    pub attributes: Vec<FluentAttribute<'m>>,
}

impl<'m> FluentMessage<'m> {
    pub fn get_attribute(&self, key: &str) -> Option<&FluentAttribute> {
        self.attributes.iter().find(|attr| attr.id == key)
    }
}

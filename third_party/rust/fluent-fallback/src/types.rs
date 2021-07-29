use fluent_bundle::FluentArgs;
use std::borrow::Cow;

#[derive(Debug)]
pub struct L10nKey<'l> {
    pub id: Cow<'l, str>,
    pub args: Option<FluentArgs<'l>>,
}

impl<'l> From<&'l str> for L10nKey<'l> {
    fn from(id: &'l str) -> Self {
        Self {
            id: id.into(),
            args: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct L10nAttribute<'l> {
    pub name: Cow<'l, str>,
    pub value: Cow<'l, str>,
}

#[derive(Debug, Clone)]
pub struct L10nMessage<'l> {
    pub value: Option<Cow<'l, str>>,
    pub attributes: Vec<L10nAttribute<'l>>,
}

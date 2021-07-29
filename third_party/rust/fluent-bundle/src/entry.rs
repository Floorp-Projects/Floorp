//! `Entry` is used to store Messages, Terms and Functions in `FluentBundle` instances.

use std::borrow::Borrow;

use fluent_syntax::ast;

use crate::args::FluentArgs;
use crate::bundle::FluentBundle;
use crate::resource::FluentResource;
use crate::types::FluentValue;

pub type FluentFunction =
    Box<dyn for<'a> Fn(&[FluentValue<'a>], &FluentArgs) -> FluentValue<'a> + Send + Sync>;

pub enum Entry {
    Message((usize, usize)),
    Term((usize, usize)),
    Function(FluentFunction),
}

pub trait GetEntry {
    fn get_entry_message(&self, id: &str) -> Option<&ast::Message<&str>>;
    fn get_entry_term(&self, id: &str) -> Option<&ast::Term<&str>>;
    fn get_entry_function(&self, id: &str) -> Option<&FluentFunction>;
}

impl<'bundle, R: Borrow<FluentResource>, M> GetEntry for FluentBundle<R, M> {
    fn get_entry_message(&self, id: &str) -> Option<&ast::Message<&str>> {
        self.entries.get(id).and_then(|ref entry| match entry {
            Entry::Message(pos) => {
                let res = self.resources.get(pos.0)?.borrow();
                if let ast::Entry::Message(ref msg) = res.get_entry(pos.1)? {
                    Some(msg)
                } else {
                    None
                }
            }
            _ => None,
        })
    }

    fn get_entry_term(&self, id: &str) -> Option<&ast::Term<&str>> {
        self.entries.get(id).and_then(|ref entry| match entry {
            Entry::Term(pos) => {
                let res = self.resources.get(pos.0)?.borrow();
                if let ast::Entry::Term(ref msg) = res.get_entry(pos.1)? {
                    Some(msg)
                } else {
                    None
                }
            }
            _ => None,
        })
    }

    fn get_entry_function(&self, id: &str) -> Option<&FluentFunction> {
        self.entries.get(id).and_then(|ref entry| match entry {
            Entry::Function(function) => Some(function),
            _ => None,
        })
    }
}

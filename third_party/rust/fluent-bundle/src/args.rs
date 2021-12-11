use std::borrow::Cow;
use std::iter::FromIterator;

use crate::types::FluentValue;

/// A map of arguments passed from the code to
/// the localization to be used for message
/// formatting.
///
/// # Example
///
/// ```
/// use fluent_bundle::{FluentArgs, FluentBundle, FluentResource};
///
/// let mut args = FluentArgs::new();
/// args.set("user", "John");
/// args.set("emailCount", 5);
///
/// let res = FluentResource::try_new(r#"
///
/// msg-key = Hello, { $user }. You have { $emailCount } messages.
///
/// "#.to_string())
///     .expect("Failed to parse FTL.");
///
/// let mut bundle = FluentBundle::default();
///
/// // For this example, we'll turn on BiDi support.
/// // Please, be careful when doing it, it's a risky move.
/// bundle.set_use_isolating(false);
///
/// bundle.add_resource(res)
///     .expect("Failed to add a resource.");
///
/// let mut err = vec![];
///
/// let msg = bundle.get_message("msg-key")
///     .expect("Failed to retrieve a message.");
/// let value = msg.value()
///     .expect("Failed to retrieve a value.");
///
/// assert_eq!(
///     bundle.format_pattern(value, Some(&args), &mut err),
///     "Hello, John. You have 5 messages."
/// );
/// ```
#[derive(Debug, Default)]
pub struct FluentArgs<'args>(Vec<(Cow<'args, str>, FluentValue<'args>)>);

impl<'args> FluentArgs<'args> {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn with_capacity(capacity: usize) -> Self {
        Self(Vec::with_capacity(capacity))
    }

    pub fn get<K>(&self, key: K) -> Option<&FluentValue<'args>>
    where
        K: Into<Cow<'args, str>>,
    {
        let key = key.into();
        if let Ok(idx) = self.0.binary_search_by_key(&&key, |(k, _)| k) {
            Some(&self.0[idx].1)
        } else {
            None
        }
    }

    pub fn set<K, V>(&mut self, key: K, value: V)
    where
        K: Into<Cow<'args, str>>,
        V: Into<FluentValue<'args>>,
    {
        let key = key.into();
        let idx = match self.0.binary_search_by_key(&&key, |(k, _)| k) {
            Ok(idx) => idx,
            Err(idx) => idx,
        };
        self.0.insert(idx, (key, value.into()));
    }

    pub fn iter(&self) -> impl Iterator<Item = (&str, &FluentValue)> {
        self.0.iter().map(|(k, v)| (k.as_ref(), v))
    }
}

impl<'args, K, V> FromIterator<(K, V)> for FluentArgs<'args>
where
    K: Into<Cow<'args, str>>,
    V: Into<FluentValue<'args>>,
{
    fn from_iter<I>(iter: I) -> Self
    where
        I: IntoIterator<Item = (K, V)>,
    {
        let iter = iter.into_iter();
        let mut args = if let Some(size) = iter.size_hint().1 {
            FluentArgs::with_capacity(size)
        } else {
            FluentArgs::new()
        };

        for (k, v) in iter {
            args.set(k, v);
        }

        args
    }
}

impl<'args> IntoIterator for FluentArgs<'args> {
    type Item = (Cow<'args, str>, FluentValue<'args>);
    type IntoIter = std::vec::IntoIter<Self::Item>;

    fn into_iter(self) -> Self::IntoIter {
        self.0.into_iter()
    }
}

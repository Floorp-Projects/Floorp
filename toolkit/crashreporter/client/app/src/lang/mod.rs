/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod language_info;
mod omnijar;

use fluent::{bundle::FluentBundle, FluentArgs, FluentResource};
use intl_memoizer::concurrent::IntlLangMemoizer;
#[cfg(test)]
pub use language_info::LanguageInfo;
use std::borrow::Cow;
use std::collections::BTreeMap;

/// Get the localized string bundle.
pub fn load() -> anyhow::Result<LangStrings> {
    // TODO support langpacks, bug 1873210
    omnijar::read().unwrap_or_else(|e| {
        log::warn!("failed to read localization data from the omnijar ({e}), falling back to bundled content");
        Default::default()
    }).load_strings()
}

/// A bundle of localized strings.
pub struct LangStrings {
    bundle: FluentBundle<FluentResource, IntlLangMemoizer>,
    rtl: bool,
}

/// Arguments to build a localized string.
pub type LangStringsArgs<'a> = BTreeMap<&'a str, Cow<'a, str>>;

impl LangStrings {
    pub fn new(bundle: FluentBundle<FluentResource, IntlLangMemoizer>, rtl: bool) -> Self {
        LangStrings { bundle, rtl }
    }

    /// Return whether the localized language has right-to-left text flow.
    pub fn is_rtl(&self) -> bool {
        self.rtl
    }

    pub fn get(&self, index: &str, args: LangStringsArgs) -> anyhow::Result<String> {
        let mut fluent_args = FluentArgs::with_capacity(args.len());
        for (k, v) in args {
            fluent_args.set(k, v);
        }

        let Some(pattern) = self.bundle.get_message(index).and_then(|m| m.value()) else {
            anyhow::bail!("failed to get fluent message for {index}");
        };
        let mut errs = Vec::new();
        let ret = self
            .bundle
            .format_pattern(pattern, Some(&fluent_args), &mut errs);
        if !errs.is_empty() {
            anyhow::bail!("errors while formatting pattern: {errs:?}");
        }
        Ok(ret.into_owned())
    }

    pub fn builder<'a>(&'a self, index: &'a str) -> LangStringBuilder<'a> {
        LangStringBuilder {
            strings: self,
            index,
            args: Default::default(),
        }
    }
}

/// A localized string builder.
pub struct LangStringBuilder<'a> {
    strings: &'a LangStrings,
    index: &'a str,
    args: LangStringsArgs<'a>,
}

impl<'a> LangStringBuilder<'a> {
    /// Set an argument for the string.
    pub fn arg<V: Into<Cow<'a, str>>>(mut self, key: &'a str, value: V) -> Self {
        self.args.insert(key, value.into());
        self
    }

    /// Get the localized string.
    pub fn get(self) -> anyhow::Result<String> {
        self.strings.get(self.index, self.args)
    }
}

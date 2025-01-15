/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use super::LanguageBundle;
use anyhow::Context;
use fluent::{bundle::FluentBundle, FluentResource};
use unic_langid::LanguageIdentifier;

const FALLBACK_FTL_FILE: &str = include_str!(mozbuild::srcdir_path!(
    "/toolkit/locales/en-US/crashreporter/crashreporter.ftl"
));
const FALLBACK_BRANDING_FILE: &str = include_str!(mozbuild::srcdir_path!(
    "/browser/branding/official/locales/en-US/brand.ftl"
));

/// Localization language information.
#[derive(Debug, Clone)]
pub struct LanguageInfo {
    pub identifier: String,
    pub ftl_definitions: String,
    pub ftl_branding: String,
}

impl Default for LanguageInfo {
    fn default() -> Self {
        Self::fallback()
    }
}

impl LanguageInfo {
    /// Get the fallback bundled language information (en-US).
    pub fn fallback() -> Self {
        LanguageInfo {
            identifier: "en-US".to_owned(),
            ftl_definitions: FALLBACK_FTL_FILE.to_owned(),
            ftl_branding: FALLBACK_BRANDING_FILE.to_owned(),
        }
    }

    /// Load strings from the language info.
    pub(super) fn load_strings(self) -> anyhow::Result<LanguageBundle> {
        let result = self.load_strings_fallible();
        // Panic in debug builds: the identifier and ftl data should always be valid.
        #[cfg(debug_assertions)]
        if let Err(e) = &result {
            panic!("bad localization: {e:#}");
        }
        result
    }

    fn load_strings_fallible(self) -> anyhow::Result<LanguageBundle> {
        let Self {
            identifier: lang,
            ftl_definitions: definitions,
            ftl_branding: branding,
        } = self;

        let langid = lang
            .parse::<LanguageIdentifier>()
            .with_context(|| format!("failed to parse language identifier ({lang})"))?;
        let rtl = langid.character_direction() == unic_langid::CharacterDirection::RTL;
        let mut bundle = FluentBundle::new_concurrent(vec![langid]);

        fn add_ftl<M>(
            bundle: &mut FluentBundle<FluentResource, M>,
            ftl: String,
        ) -> anyhow::Result<()> {
            let resource = FluentResource::try_new(ftl)
                .ok()
                .context("failed to create fluent resource")?;
            bundle
                .add_resource(resource)
                .ok()
                .context("failed to add fluent resource to bundle")?;
            Ok(())
        }

        add_ftl(&mut bundle, branding).context("failed to add branding")?;
        add_ftl(&mut bundle, definitions).context("failed to add localization")?;

        Ok(LanguageBundle::new(bundle, rtl))
    }
}

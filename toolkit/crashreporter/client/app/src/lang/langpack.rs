/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::language_info::LanguageInfo;
use super::zip::{read_archive_file_as_string, read_zip, Archive};
use crate::config::installation_resource_path;
use crate::std::path::{Path, PathBuf};
use anyhow::Context;

const LOCALE_PREF_KEY: &str = r#""intl.locale.requested""#;

/// Use the profile language preferences to determine the localization to use.
pub fn read(profile_dir: &Path, locale: Option<&str>) -> anyhow::Result<Option<LanguageInfo>> {
    let Some((identifier, langpack)) = select_langpack(profile_dir, locale)? else {
        return Ok(None);
    };

    let mut zip = read_zip(&langpack)?;

    let ftl_definitions = read_archive_ftl_definitions(&mut zip, &identifier)?;
    let ftl_branding = read_archive_ftl_branding(&mut zip, &identifier).unwrap_or_else(|e| {
        log::warn!(
            "failed to read branding for {identifier} from {}: {e:#}",
            langpack.display()
        );
        log::info!("using fallback branding info");
        LanguageInfo::default().ftl_branding
    });

    Ok(Some(LanguageInfo {
        identifier,
        ftl_definitions,
        ftl_branding,
    }))
}

/// Select the langpack to use based on profile settings, the useragent locale, and the existence
/// of langpack files.
fn select_langpack(
    profile_dir: &Path,
    locale: Option<&str>,
) -> anyhow::Result<Option<(String, PathBuf)>> {
    if let Some(path) = locale.and_then(|l| find_langpack_extension(profile_dir, l)) {
        Ok(Some((locale.unwrap().to_string(), path)))
    } else {
        let Some(locales) = locales_from_prefs(profile_dir)? else {
            return Ok(None);
        };
        // Use the first language for which we can find a langpack extension.
        locales
            .iter()
            .find_map(|lang| {
                find_langpack_extension(profile_dir, lang).map(|path| (lang.to_string(), path))
            })
            .with_context(|| {
                format!("couldn't locate langpack for requested locales ({locales:?})")
            })
            .map(Some)
    }
}

fn locales_from_prefs(profile_dir: &Path) -> anyhow::Result<Option<Vec<String>>> {
    let prefs = profile_dir.join("prefs.js");
    if !prefs.exists() {
        log::debug!(
            "no prefs.js exists in {}; not reading localization info",
            profile_dir.display()
        );
        return Ok(None);
    }

    let prefs_contents = std::fs::read_to_string(&prefs)
        .with_context(|| format!("while reading {}", prefs.display()))?;

    // Logic dictated by https://firefox-source-docs.mozilla.org/intl/locale.html#requested-locales

    let Some(langs) = parse_requested_locales(&prefs_contents) else {
        // If the pref is unset, the installation locale should be used.
        log::debug!(
            "no locale pref set in {}; using installation locale",
            prefs.display()
        );
        return Ok(None);
    };

    Ok(Some(if langs.is_empty() {
        // If the pref is an empty string, use the OS locale settings.
        let os_locales: Vec<String> = sys_locale::get_locales().collect();
        if os_locales.is_empty() {
            log::debug!("no OS locales found");
            return Ok(None);
        }
        log::debug!(
            "locale pref blank in {}; using OS locale settings ({os_locales:?})",
            prefs.display()
        );
        os_locales
    } else {
        langs.into_iter().map(|s| s.to_owned()).collect()
    }))
}

/// Parse the language pref (if any) from the prefs file.
///
/// This finds the first string match for the regex `"intl.locale.requested"[ \t\n\r\f\v,]*"(.*)"`,
/// and splits and trims the first match group, returning the set of strings that results.
///
/// For example:
/// ```rust
/// let input = r#""intl.locale.requested", "foo , bar,,""#;
/// let expected_output = Some(vec!["foo","bar"]);
/// assert_eq!(parse_requested_locales(input), output);
/// ```
///
/// This will parse the locales out of the user prefs file contents, which looks like
/// `user_pref("intl.locale.requested", "<LOCALE LIST>")`.
fn parse_requested_locales(prefs_content: &str) -> Option<Vec<&str>> {
    let (_, s) = prefs_content.split_once(LOCALE_PREF_KEY)?;
    let s = s.trim_start_matches(|c: char| c.is_whitespace() || c == ',');
    let s = s.strip_prefix('"')?;
    let (v, _) = s.split_once('"')?;
    Some(
        v.split(",")
            .map(|s| s.trim())
            .filter(|s| !s.is_empty())
            .collect(),
    )
}

/// Find the langpack extension for the given locale.
fn find_langpack_extension(profile_dir: &Path, locale: &str) -> Option<PathBuf> {
    let path_tail = format!("extensions/langpack-{locale}@firefox.mozilla.org.xpi");

    // Check profile extensions
    let profile_path = profile_dir.join(&path_tail);
    if profile_path.exists() {
        return Some(profile_path);
    }

    // Check installation extensions
    let install_path = installation_resource_path().join(format!("browser/{}", &path_tail));
    if install_path.exists() {
        return Some(install_path);
    }

    None
}

fn read_archive_ftl_definitions(
    langpack: &mut Archive,
    language_identifier: &str,
) -> anyhow::Result<String> {
    let path = format!("localization/{language_identifier}/crashreporter/crashreporter.ftl");
    read_archive_file_as_string(langpack, &path)
}

fn read_archive_ftl_branding(
    langpack: &mut Archive,
    language_identifier: &str,
) -> anyhow::Result<String> {
    let path = format!("browser/localization/{language_identifier}/branding/brand.ftl");
    read_archive_file_as_string(langpack, &path)
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn parse_locales_none() {
        assert_eq!(parse_requested_locales(""), None);
    }

    #[test]
    fn parse_locales_empty() {
        assert_eq!(
            parse_requested_locales(r#"user_pref("intl.locale.requested","")"#),
            Some(vec![])
        );
    }

    #[test]
    fn parse_locales() {
        assert_eq!(
            parse_requested_locales(r#"user_pref("intl.locale.requested", "fr,en-US")"#),
            Some(vec!["fr", "en-US"])
        );
    }
}

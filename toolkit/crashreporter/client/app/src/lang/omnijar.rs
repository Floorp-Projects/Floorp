/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::language_info::LanguageInfo;
use super::zip::{read_archive_file_as_string, read_zip};
use crate::config::installation_resource_path;
use crate::std::io::{BufRead, BufReader};
use anyhow::Context;
use once_cell::unsync::Lazy;

/// Read the appropriate localization fluent definitions from the omnijar files.
///
/// Returns language information if found in adjacent omnijar files.
pub fn read() -> anyhow::Result<LanguageInfo> {
    let mut path = installation_resource_path().join("omni.ja");

    let mut zip = read_zip(&path)?;
    let buf = BufReader::new(
        zip.by_name("res/multilocale.txt")
            .context("failed to read multilocale file in zip archive")?,
    );
    let line = buf
        .lines()
        .next()
        .ok_or(anyhow::anyhow!("multilocale file was empty"))?
        .context("failed to read first line of multilocale file")?;
    let locales = line.split(",").map(|s| s.trim());

    let (identifier, ftl_definitions) = 'defs: {
        for locale in locales.clone() {
            match read_archive_file_as_string(
                &mut zip,
                &format!("localization/{locale}/crashreporter/crashreporter.ftl"),
            ) {
                Ok(v) => break 'defs (locale.to_owned(), v),
                Err(e) => log::warn!("failed to get localized strings for {locale}: {e:#}"),
            }
        }
        anyhow::bail!("failed to find any usable localized strings in the omnijar")
    };

    // Firefox branding is in the browser omnijar.
    path.pop();
    path.push("browser");
    path.push("omni.ja");

    let mut browser_omnijar = Lazy::new(|| match read_zip(&path) {
        Err(e) => {
            log::debug!("no browser omnijar found at {}: {e:#}", path.display());
            None
        }
        Ok(z) => Some(z),
    });

    let ftl_branding = 'branding: {
        for locale in locales {
            let brand_file = format!("localization/{locale}/branding/brand.ftl");
            // Bug 1895244: Thunderbird branding is in the main omnijar
            let result = read_archive_file_as_string(&mut zip, &brand_file).or_else(|e| {
                match &mut *browser_omnijar {
                    Some(browser_zip) => {
                        log::debug!(
                            "failed to read branding for {locale} from main omnijar ({e:#}), trying browser omnijar"
                        );
                        read_archive_file_as_string(browser_zip, &brand_file)
                    }
                    None => Err(e)
                }
            });
            match result {
                Ok(b) => break 'branding b,
                Err(e) => log::warn!("failed to read branding for {locale} from omnijar: {e:#}"),
            }
        }
        log::info!("using fallback branding info");
        LanguageInfo::default().ftl_branding
    };

    Ok(LanguageInfo {
        identifier,
        ftl_definitions,
        ftl_branding,
    })
}

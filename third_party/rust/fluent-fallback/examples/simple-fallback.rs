//! This is an example of a simple application
//! which calculates the Collatz conjecture.
//!
//! The function itself is trivial on purpose,
//! so that we can focus on understanding how
//! the application can be made localizable
//! via Fluent.
//!
//! To try the app launch `cargo run --example simple-fallback NUM (LOCALES)`
//!
//! NUM is a number to be calculated, and LOCALES is an optional
//! parameter with a comma-separated list of locales requested by the user.
//!
//! Example:
//!
//!   cargo run --example simple-fallback 123 de,pl
//!
//! If the second argument is omitted, `en-US` locale is used as the
//! default one.

use std::{env, fs, io, path::PathBuf, str::FromStr};

use fluent_bundle::{FluentArgs, FluentBundle, FluentResource};
use fluent_fallback::{
    generator::{BundleGenerator, FluentBundleResult},
    types::ResourceId,
    Localization,
};
use fluent_langneg::{negotiate_languages, NegotiationStrategy};

use rustc_hash::FxHashSet;
use unic_langid::{langid, LanguageIdentifier};

/// This helper struct holds the scheme for converting
/// resource paths into full paths. It is used to customise
/// `fluent-fallback::SyncLocalization`.
struct Bundles {
    res_path_scheme: PathBuf,
}

/// This helper function allows us to read the list
/// of available locales by reading the list of
/// directories in `./examples/resources`.
///
/// It is expected that every directory inside it
/// has a name that is a valid BCP47 language tag.
fn get_available_locales() -> io::Result<Vec<LanguageIdentifier>> {
    let mut dir = env::current_dir()?;
    if dir.to_string_lossy().ends_with("fluent-rs") {
        dir.push("fluent-fallback");
    }
    dir.push("examples");
    dir.push("resources");
    let res_dir = fs::read_dir(dir)?;

    let locales = res_dir
        .into_iter()
        .filter_map(|entry| entry.ok())
        .filter(|entry| entry.path().is_dir())
        .filter_map(|dir| {
            let file_name = dir.file_name();
            let name = file_name.to_str()?;
            Some(name.parse().expect("Parsing failed."))
        })
        .collect();
    Ok(locales)
}

fn resolve_app_locales<'l>(args: &[String]) -> Vec<LanguageIdentifier> {
    let default_locale = langid!("en-US");
    let available = get_available_locales().expect("Retrieving available locales failed.");

    let requested: Vec<LanguageIdentifier> = args.get(2).map_or(vec![], |arg| {
        arg.split(",")
            .map(|s| s.parse().expect("Parsing locale failed."))
            .collect()
    });

    negotiate_languages(
        &requested,
        &available,
        Some(&default_locale),
        NegotiationStrategy::Filtering,
    )
    .into_iter()
    .cloned()
    .collect()
}

fn get_resource_manager() -> Bundles {
    let mut res_path_scheme = env::current_dir().expect("Failed to retrieve current dir.");

    if res_path_scheme.to_string_lossy().ends_with("fluent-rs") {
        res_path_scheme.push("fluent-fallback");
    }
    res_path_scheme.push("examples");
    res_path_scheme.push("resources");

    res_path_scheme.push("{locale}");
    res_path_scheme.push("{res_id}");

    Bundles { res_path_scheme }
}

static L10N_RESOURCES: &[&str] = &["simple.ftl"];

fn main() {
    let args: Vec<String> = env::args().collect();

    let app_locales: Vec<LanguageIdentifier> = resolve_app_locales(&args);

    let bundles = get_resource_manager();

    let loc = Localization::with_env(
        L10N_RESOURCES.iter().map(|&res| res.into()),
        true,
        app_locales,
        bundles,
    );
    let bundles = loc.bundles();

    let mut errors = vec![];

    match args.get(1) {
        Some(input) => match isize::from_str(&input) {
            Ok(i) => {
                let mut args = FluentArgs::new();
                args.set("input", i);
                args.set("value", collatz(i));
                let value = bundles
                    .format_value_sync("response-msg", Some(&args), &mut errors)
                    .unwrap()
                    .unwrap();
                println!("{}", value);
            }
            Err(err) => {
                let mut args = FluentArgs::new();
                args.set("input", input.as_str());
                args.set("reason", err.to_string());
                let value = bundles
                    .format_value_sync("input-parse-error-msg", Some(&args), &mut errors)
                    .unwrap()
                    .unwrap();
                println!("{}", value);
            }
        },
        None => {
            let value = bundles
                .format_value_sync("missing-arg-error", None, &mut errors)
                .unwrap()
                .unwrap();
            println!("{}", value);
        }
    }
}

/// Collatz conjecture calculating function.
fn collatz(n: isize) -> isize {
    match n {
        1 => 0,
        _ => match n % 2 {
            0 => 1 + collatz(n / 2),
            _ => 1 + collatz(n * 3 + 1),
        },
    }
}

/// Bundle iterator used by BundleGeneratorSync implementation for Locales.
struct BundleIter {
    res_path_scheme: String,
    locales: <Vec<LanguageIdentifier> as IntoIterator>::IntoIter,
    res_ids: FxHashSet<ResourceId>,
}

impl Iterator for BundleIter {
    type Item = FluentBundleResult<FluentResource>;

    fn next(&mut self) -> Option<Self::Item> {
        let locale = self.locales.next()?;
        let res_path_scheme = self
            .res_path_scheme
            .as_str()
            .replace("{locale}", &locale.to_string());
        let mut bundle = FluentBundle::new(vec![locale]);

        let mut errors = vec![];

        for res_id in &self.res_ids {
            let res_path = res_path_scheme.as_str().replace("{res_id}", &res_id.value);
            let source = fs::read_to_string(res_path).unwrap();
            let res = match FluentResource::try_new(source) {
                Ok(res) => res,
                Err((res, err)) => {
                    errors.extend(err.into_iter().map(Into::into));
                    res
                }
            };
            bundle.add_resource(res).unwrap();
        }

        if errors.is_empty() {
            Some(Ok(bundle))
        } else {
            Some(Err((bundle, errors)))
        }
    }
}

impl futures::Stream for BundleIter {
    type Item = FluentBundleResult<FluentResource>;

    fn poll_next(
        self: std::pin::Pin<&mut Self>,
        _cx: &mut std::task::Context<'_>,
    ) -> std::task::Poll<Option<Self::Item>> {
        todo!()
    }
}

impl BundleGenerator for Bundles {
    type Resource = FluentResource;
    type LocalesIter = std::vec::IntoIter<LanguageIdentifier>;
    type Iter = BundleIter;
    type Stream = BundleIter;

    fn bundles_iter(
        &self,
        locales: std::vec::IntoIter<LanguageIdentifier>,
        res_ids: FxHashSet<ResourceId>,
    ) -> Self::Iter {
        BundleIter {
            res_path_scheme: self.res_path_scheme.to_string_lossy().to_string(),
            locales,
            res_ids,
        }
    }
}

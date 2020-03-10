//! This is an example of a simple application
//! which calculates the Collatz conjecture.
//!
//! The function itself is trivial on purpose,
//! so that we can focus on understanding how
//! the application can be made localizable
//! via Fluent.
//!
//! To try the app launch `cargo run --example simple-app NUM (LOCALES)`
//!
//! NUM is a number to be calculated, and LOCALES is an optional
//! parameter with a comma-separated list of locales requested by the user.
//!
//! Example:
//!   
//!   cargo run --example simple-app 123 de,pl
//!
//! If the second argument is omitted, `en-US` locale is used as the
//! default one.
use fluent_bundle::{FluentArgs, FluentBundle, FluentResource, FluentValue};
use fluent_langneg::{negotiate_languages, NegotiationStrategy};
use std::env;
use std::fs;
use std::fs::File;
use std::io;
use std::io::prelude::*;
use std::path::Path;
use std::str::FromStr;
use unic_langid::{langid, LanguageIdentifier};

/// We need a generic file read helper function to
/// read the localization resource file.
///
/// The resource files are stored in
/// `./examples/resources/{locale}` directory.
fn read_file(path: &Path) -> Result<String, io::Error> {
    let mut f = File::open(path)?;
    let mut s = String::new();
    f.read_to_string(&mut s)?;
    Ok(s)
}

/// This helper function allows us to read the list
/// of available locales by reading the list of
/// directories in `./examples/resources`.
///
/// It is expected that every directory inside it
/// has a name that is a valid BCP47 language tag.
fn get_available_locales() -> Result<Vec<LanguageIdentifier>, io::Error> {
    let mut locales = vec![];

    let mut dir = env::current_dir()?;
    if dir.to_string_lossy().ends_with("fluent-rs") {
        dir.push("fluent-bundle");
    }
    dir.push("examples");
    dir.push("resources");
    let res_dir = fs::read_dir(dir)?;
    for entry in res_dir {
        if let Ok(entry) = entry {
            let path = entry.path();
            if path.is_dir() {
                if let Some(name) = path.file_name() {
                    if let Some(name) = name.to_str() {
                        let langid = name.parse().expect("Parsing failed.");
                        locales.push(langid);
                    }
                }
            }
        }
    }
    return Ok(locales);
}

static L10N_RESOURCES: &[&str] = &["simple.ftl"];

fn main() {
    // 1. Get the command line arguments.
    let args: Vec<String> = env::args().collect();

    // 3. If the argument length is more than 1,
    //    take the second argument as a comma-separated
    //    list of requested locales.
    let requested = args.get(2).map_or(vec![], |arg| {
        arg.split(",")
            .map(|s| -> LanguageIdentifier { s.parse().expect("Parsing locale failed.") })
            .collect()
    });

    // 4. Negotiate it against the available ones
    let default_locale = langid!("en-US");
    let available = get_available_locales().expect("Retrieving available locales failed.");
    let resolved_locales = negotiate_languages(
        &requested,
        &available,
        Some(&default_locale),
        NegotiationStrategy::Filtering,
    );
    let current_locale = resolved_locales
        .get(0)
        .expect("At least one locale should match.");

    // 5. Create a new Fluent FluentBundle using the
    //    resolved locales.
    let mut bundle = FluentBundle::new(resolved_locales.clone());

    // 6. Load the localization resource
    for path in L10N_RESOURCES {
        let mut full_path = env::current_dir().expect("Failed to retireve current dir.");
        if full_path.to_string_lossy().ends_with("fluent-rs") {
            full_path.push("fluent-bundle");
        }
        full_path.push("examples");
        full_path.push("resources");
        full_path.push(current_locale.to_string());
        full_path.push(path);
        let source = read_file(&full_path).expect("Failed to read file.");
        let resource = FluentResource::try_new(source).expect("Could not parse an FTL string.");
        bundle
            .add_resource(resource)
            .expect("Failed to add FTL resources to the bundle.");
    }

    // 7. Check if the input is provided.
    match args.get(1) {
        Some(input) => {
            // 7.1. Cast it to a number.
            match isize::from_str(&input) {
                Ok(i) => {
                    // 7.2. Construct a map of arguments
                    //      to format the message.
                    let mut args = FluentArgs::new();
                    args.insert("input", FluentValue::from(i));
                    args.insert("value", FluentValue::from(collatz(i)));
                    // 7.3. Format the message.
                    let mut errors = vec![];
                    let msg = bundle
                        .get_message("response-msg")
                        .expect("Message doesn't exist.");
                    let pattern = msg.value.expect("Message has no value.");
                    let value = bundle.format_pattern(&pattern, Some(&args), &mut errors);
                    println!("{}", value);
                }
                Err(err) => {
                    let mut args = FluentArgs::new();
                    args.insert("input", FluentValue::from(input.as_str()));
                    args.insert("reason", FluentValue::from(err.to_string()));
                    let mut errors = vec![];
                    let msg = bundle
                        .get_message("input-parse-error")
                        .expect("Message doesn't exist.");
                    let pattern = msg.value.expect("Message has no value.");
                    let value = bundle.format_pattern(&pattern, Some(&args), &mut errors);
                    println!("{}", value);
                }
            }
        }
        None => {
            let mut errors = vec![];
            let msg = bundle
                .get_message("missing-arg-error")
                .expect("Message doesn't exist.");
            let pattern = msg.value.expect("Message has no value.");
            let value = bundle.format_pattern(&pattern, None, &mut errors);
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

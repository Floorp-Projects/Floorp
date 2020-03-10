// This is an example of an application which adds a custom type of value,
// and a function to format it.
//
// In this example we're going to add a new type - DateTime.
//
// We're also going to add a built-in function DATETIME to produce that type
// out of a number argument (epoch).
//
// Lastly, we'll also create a new formatter which will be memoizable.
//
// The type and its options are modelled after ECMA402 Intl.DateTimeFormat.
use intl_memoizer::Memoizable;
use unic_langid::LanguageIdentifier;

use fluent_bundle::types::FluentType;
use fluent_bundle::{FluentArgs, FluentBundle, FluentResource, FluentValue};

// First we're going to define what options our new type is going to accept.
// For the sake of the example, we're only going to allow two options:
//  - dateStyle
//  - timeStyle
//
// with an enum of allowed values.
#[derive(Debug, PartialEq, Eq, Clone, Hash)]
enum DateTimeStyleValue {
    Full,
    Long,
    Medium,
    Short,
    None,
}

impl std::default::Default for DateTimeStyleValue {
    fn default() -> Self {
        Self::None
    }
}

// This is just a helper to make it easier to convert
// a value provided to FluentArgs into an option value.
impl<'l> From<&FluentValue<'l>> for DateTimeStyleValue {
    fn from(input: &FluentValue) -> Self {
        if let FluentValue::String(s) = input {
            match s.as_ref() {
                "full" => Self::Full,
                "long" => Self::Long,
                "medium" => Self::Medium,
                "short" => Self::Short,
                _ => Self::None,
            }
        } else {
            Self::None
        }
    }
}

#[derive(Debug, PartialEq, Eq, Default, Clone, Hash)]
struct DateTimeOptions {
    pub date_style: DateTimeStyleValue,
    pub time_style: DateTimeStyleValue,
}

impl DateTimeOptions {
    // The merge function is going to be used by the Fluent Function
    // to merge localizer provided options into defaults of values
    // provided by the developer.
    //
    // If you want to limit which options the localizer can override,
    // here's the right place to do it.
    pub fn merge(&mut self, input: &FluentArgs) {
        for (key, value) in input {
            match *key {
                "dateStyle" => self.date_style = value.into(),
                "timeStyle" => self.time_style = value.into(),
                _ => {}
            }
        }
    }
}

impl<'l> From<&FluentArgs<'l>> for DateTimeOptions {
    fn from(input: &FluentArgs) -> Self {
        let mut opts = Self::default();
        opts.merge(input);
        opts
    }
}

// Our new custom type will store a value as an epoch number,
// and the options.

#[derive(Debug, PartialEq, Clone)]
struct DateTime {
    epoch: usize,
    options: DateTimeOptions,
}

impl DateTime {
    pub fn new(epoch: usize, options: DateTimeOptions) -> Self {
        Self { epoch, options }
    }
}

impl FluentType for DateTime {
    fn duplicate(&self) -> Box<dyn FluentType> {
        Box::new(DateTime::new(self.epoch, DateTimeOptions::default()))
    }
    fn as_string(&self, intls: &intl_memoizer::IntlLangMemoizer) -> std::borrow::Cow<'static, str> {
        intls
            .with_try_get::<DateTimeFormatter, _, _>((self.options.clone(),), |dtf| {
                dtf.format(self.epoch).into()
            })
            .expect("Failed to format a date.")
    }
    fn as_string_threadsafe(
        &self,
        _: &intl_memoizer::concurrent::IntlLangMemoizer,
    ) -> std::borrow::Cow<'static, str> {
        format!("2020-01-20 {}:00", self.epoch).into()
    }
}

/// Formatter

struct DateTimeFormatter {
    lang: LanguageIdentifier,
    options: DateTimeOptions,
}

impl DateTimeFormatter {
    pub fn new(lang: LanguageIdentifier, options: DateTimeOptions) -> Result<Self, ()> {
        Ok(Self { lang, options })
    }

    pub fn format(&self, epoch: usize) -> String {
        format!(
            "My Custom Formatted Date from epoch: {}, in locale: {}, using options: {:#?}",
            epoch, self.lang, self.options
        )
    }
}

impl Memoizable for DateTimeFormatter {
    type Args = (DateTimeOptions,);
    type Error = ();
    fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error> {
        Self::new(lang, args.0)
    }
}

fn main() {
    // 1. Bootstrap a FluentBundle with a number of messages which use
    //    number formatting in different forms.
    let ftl_string = String::from(
        r#"
key-date = Today is { DATETIME($epoch, dateStyle: "long", timeStyle: "short") }
    "#,
    );
    let res = FluentResource::try_new(ftl_string).expect("Could not parse an FTL string.");

    let lang: LanguageIdentifier = "en".parse().unwrap();
    let mut bundle = FluentBundle::new(&[lang]);
    bundle
        .add_resource(res)
        .expect("Failed to add FTL resources to the bundle.");

    bundle
        .add_function("DATETIME", |positional, named| match positional.get(0) {
            Some(FluentValue::Number(n)) => {
                let epoch = n.value as usize;
                let options = named.into();
                FluentValue::Custom(Box::new(DateTime::new(epoch, options)))
            }
            _ => FluentValue::None,
        })
        .expect("Failed to add a function.");
    bundle.set_use_isolating(false);

    let msg = bundle
        .get_message("key-date")
        .expect("Failed to retrieve the message.");
    let pattern = msg.value.expect("Message has no value.");
    let mut errors = vec![];

    let mut args = FluentArgs::new();
    let epoch: u64 = 1580127760093;
    args.insert("epoch", epoch.into());
    let value = bundle.format_pattern(pattern, Some(&args), &mut errors);
    println!("{}", value);
}

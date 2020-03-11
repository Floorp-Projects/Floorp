// This is an example of an application which uses a custom formatter
// to format selected types of values.
//
// This allows users to plug their own number formatter to Fluent.
use unic_langid::LanguageIdentifier;

use fluent_bundle::memoizer::MemoizerKind;
use fluent_bundle::types::{FluentNumber, FluentNumberOptions};
use fluent_bundle::{FluentArgs, FluentBundle, FluentResource, FluentValue};

fn custom_formatter<M: MemoizerKind>(num: &FluentValue, _intls: &M) -> Option<String> {
    match num {
        FluentValue::Number(n) => Some(format!("CUSTOM({})", n.value).into()),
        _ => None,
    }
}

fn main() {
    // 1. Bootstrap a FluentBundle with a number of messages which use
    //    number formatting in different forms.
    let ftl_string = String::from(
        "
key-implicit = Here is an implicitly encoded number: { 5 }.
key-explicit = Here is an explicitly encoded number: { NUMBER(5) }.
key-var-implicit = Here is an implicitly encoded variable: { $num }.
key-var-explicit = Here is an explicitly encoded variable: { NUMBER($num) }.
key-var-with-arg = Here is a variable formatted with an argument { NUMBER($num, minimumFractionDigits: 5) }.
    ",
    );
    let res = FluentResource::try_new(ftl_string).expect("Could not parse an FTL string.");

    let lang: LanguageIdentifier = "en".parse().unwrap();
    let mut bundle = FluentBundle::new(&[lang]);
    bundle
        .add_resource(res)
        .expect("Failed to add FTL resources to the bundle.");
    bundle
        .add_function("NUMBER", |positional, named| {
            match positional.get(0) {
                Some(FluentValue::Number(n)) => {
                    let mut num = n.clone();
                    // This allows us to merge the arguments provided
                    // as arguments to the function into the new FluentNumber.
                    num.options.merge(named);
                    FluentValue::Number(num)
                }
                _ => FluentValue::None,
            }
        })
        .expect("Failed to add a function.");
    bundle.set_use_isolating(false);

    let mut errors = vec![];

    // 2. First, we're going to format the number using the implicit formatter.
    //    At the moment the number will be formatted in a very dummy way, since
    //    we do not have a locale aware number formatter available yet.
    let msg = bundle
        .get_message("key-implicit")
        .expect("Message doesn't exist.");
    let pattern = msg.value.expect("Message has no value.");
    let value = bundle.format_pattern(&pattern, None, &mut errors);
    assert_eq!(value, "Here is an implicitly encoded number: 5.");
    println!("{}", value);

    // 3. Next, we're going to plug our custom formatter.
    bundle.set_formatter(Some(custom_formatter));

    // 4. Now, when you attempt to format a number, the custom formatter
    //    will be used instead of the default one.
    let msg = bundle
        .get_message("key-implicit")
        .expect("Message doesn't exist.");
    let pattern = msg.value.expect("Message has no value.");
    let value = bundle.format_pattern(&pattern, None, &mut errors);
    assert_eq!(value, "Here is an implicitly encoded number: CUSTOM(5).");
    println!("{}", value);

    // 5. The same custom formatter will be used for explicitly formatter numbers,
    //    and variables of type number.
    let msg = bundle
        .get_message("key-explicit")
        .expect("Message doesn't exist.");
    let pattern = msg.value.expect("Message has no value.");
    let value = bundle.format_pattern(&pattern, None, &mut errors);
    assert_eq!(value, "Here is an explicitly encoded number: CUSTOM(5).");
    println!("{}", value);

    let msg = bundle
        .get_message("key-var-implicit")
        .expect("Message doesn't exist.");
    let pattern = msg.value.expect("Message has no value.");
    let mut args = FluentArgs::new();
    args.insert("num", FluentValue::from(-15));
    let value = bundle.format_pattern(&pattern, Some(&args), &mut errors);
    assert_eq!(
        value,
        "Here is an implicitly encoded variable: CUSTOM(-15)."
    );
    println!("{}", value);

    let msg = bundle
        .get_message("key-var-explicit")
        .expect("Message doesn't exist.");
    let pattern = msg.value.expect("Message has no value.");
    let mut args = FluentArgs::new();
    args.insert("num", FluentValue::from(-15));
    let value = bundle.format_pattern(&pattern, Some(&args), &mut errors);
    assert_eq!(
        value,
        "Here is an explicitly encoded variable: CUSTOM(-15)."
    );
    println!("{}", value);

    // 6. The merging operation on FluentNumber options allows the
    //    options provided from the localizer to be merged into the
    //    default ones and ones provided by the developer.
    let msg = bundle
        .get_message("key-var-explicit")
        .expect("Message doesn't exist.");
    let pattern = msg.value.expect("Message has no value.");
    let mut args = FluentArgs::new();
    let num = FluentNumber::new(
        25.2,
        FluentNumberOptions {
            maximum_fraction_digits: Some(8),
            minimum_fraction_digits: Some(1),
            ..Default::default()
        },
    );
    args.insert("num", num.into());
    let value = bundle.format_pattern(&pattern, Some(&args), &mut errors);

    // Notice, that since we specificed minimum and maximum fraction digits options
    // to be 1 and 8 when construction the argument, and then the minimum fraction
    // digits option has been overridden in the localization the formatter
    // will received options:
    //  - minimum_fraction_digits: Some(5)
    //  - maximum_fraction_digits: Some(8)
    assert_eq!(
        value,
        "Here is an explicitly encoded variable: CUSTOM(25.2)."
    );
    println!("{}", value);
}

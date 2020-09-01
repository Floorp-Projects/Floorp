use fluent_bundle::resolve::Scope;
use fluent_bundle::types::{
    FluentNumber, FluentNumberCurrencyDisplayStyle, FluentNumberOptions, FluentNumberStyle,
};
use fluent_bundle::FluentArgs;
use fluent_bundle::FluentBundle;
use fluent_bundle::FluentResource;
use fluent_bundle::FluentValue;
use intl_pluralrules::operands::PluralOperands;
use unic_langid::langid;

#[test]
fn fluent_value_try_number() {
    let value = FluentValue::try_number("invalid");
    assert_eq!(value, "invalid".into());
}

#[test]
fn fluent_value_matches() {
    // We'll use `ars` locale since it happens to have all
    // plural rules categories.
    let langid_ars = langid!("ars");
    let bundle: FluentBundle<FluentResource> = FluentBundle::new(&[langid_ars]);
    let scope = Scope::new(&bundle, None);

    let string_val = FluentValue::from("string1");
    let string_val_copy = FluentValue::from("string1");
    let string_val2 = FluentValue::from("23.5");

    let number_val = FluentValue::from(-23.5);
    let number_val_copy = FluentValue::from(-23.5);
    let number_val2 = FluentValue::from(23.5);

    assert_eq!(string_val.matches(&string_val_copy, &scope), true);
    assert_eq!(string_val.matches(&string_val2, &scope), false);

    assert_eq!(number_val.matches(&number_val_copy, &scope), true);
    assert_eq!(number_val.matches(&number_val2, &scope), false);

    assert_eq!(string_val2.matches(&number_val2, &scope), false);

    assert_eq!(string_val2.matches(&number_val2, &scope), false);

    let string_cat_zero = FluentValue::from("zero");
    let string_cat_one = FluentValue::from("one");
    let string_cat_two = FluentValue::from("two");
    let string_cat_few = FluentValue::from("few");
    let string_cat_many = FluentValue::from("many");
    let string_cat_other = FluentValue::from("other");

    let number_cat_zero = 0.into();
    let number_cat_one = 1.into();
    let number_cat_two = 2.into();
    let number_cat_few = 3.into();
    let number_cat_many = 11.into();
    let number_cat_other = 101.into();

    assert_eq!(string_cat_zero.matches(&number_cat_zero, &scope), true);
    assert_eq!(string_cat_one.matches(&number_cat_one, &scope), true);
    assert_eq!(string_cat_two.matches(&number_cat_two, &scope), true);
    assert_eq!(string_cat_few.matches(&number_cat_few, &scope), true);
    assert_eq!(string_cat_many.matches(&number_cat_many, &scope), true);
    assert_eq!(string_cat_other.matches(&number_cat_other, &scope), true);
    assert_eq!(string_cat_other.matches(&number_cat_one, &scope), false);

    assert_eq!(string_val2.matches(&number_cat_one, &scope), false);
}

#[test]
fn fluent_value_from() {
    let value_str = FluentValue::from("my str");
    let value_string = FluentValue::from(String::from("my string"));
    let value_f64 = FluentValue::from(23.5);
    let value_isize = FluentValue::from(-23);

    assert_eq!(value_str, "my str".into());
    assert_eq!(value_string, "my string".into());

    assert_eq!(value_f64, FluentValue::from(23.5));
    assert_eq!(value_isize, FluentValue::from(-23));
}

#[test]
fn fluent_number_style() {
    let fns_decimal: FluentNumberStyle = "decimal".into();
    let fns_currency: FluentNumberStyle = "currency".into();
    let fns_percent: FluentNumberStyle = "percent".into();
    let fns_decimal2: FluentNumberStyle = "other".into();
    assert_eq!(fns_decimal, FluentNumberStyle::Decimal);
    assert_eq!(fns_currency, FluentNumberStyle::Currency);
    assert_eq!(fns_percent, FluentNumberStyle::Percent);
    assert_eq!(fns_decimal2, FluentNumberStyle::Decimal);

    let fncds_symbol: FluentNumberCurrencyDisplayStyle = "symbol".into();
    let fncds_code: FluentNumberCurrencyDisplayStyle = "code".into();
    let fncds_name: FluentNumberCurrencyDisplayStyle = "name".into();
    let fncds_symbol2: FluentNumberCurrencyDisplayStyle = "other".into();

    assert_eq!(fncds_symbol, FluentNumberCurrencyDisplayStyle::Symbol);
    assert_eq!(fncds_code, FluentNumberCurrencyDisplayStyle::Code);
    assert_eq!(fncds_name, FluentNumberCurrencyDisplayStyle::Name);
    assert_eq!(fncds_symbol2, FluentNumberCurrencyDisplayStyle::Symbol);

    let mut fno = FluentNumberOptions::default();

    let mut args = FluentArgs::new();
    args.insert("style", "currency".into());
    args.insert("currency", "EUR".into());
    args.insert("currencyDisplay", "code".into());
    args.insert("useGrouping", "true".into());
    args.insert("minimumIntegerDigits", 3.into());
    args.insert("minimumFractionDigits", 3.into());
    args.insert("maximumFractionDigits", 8.into());
    args.insert("minimumSignificantDigits", 1.into());
    args.insert("maximumSignificantDigits", 10.into());
    args.insert("someRandomOption", 10.into());

    fno.merge(&args);

    assert_eq!(fno.style, FluentNumberStyle::Currency);
    assert_eq!(fno.currency, Some("EUR".to_string()));
    assert_eq!(fno.currency_display, FluentNumberCurrencyDisplayStyle::Code);
    assert_eq!(fno.use_grouping, true);

    let num = FluentNumber::new(0.2, FluentNumberOptions::default());
    assert_eq!(num.as_string(), "0.2");

    let opts = FluentNumberOptions {
        minimum_fraction_digits: Some(3),
        ..Default::default()
    };

    let num = FluentNumber::new(0.2, opts.clone());
    assert_eq!(num.as_string(), "0.200");

    let num = FluentNumber::new(2.0, opts.clone());
    assert_eq!(num.as_string(), "2.000");
}

#[test]
fn fluent_number_to_operands() {
    let num = FluentNumber::new(2.81, FluentNumberOptions::default());
    let operands: PluralOperands = (&num).into();

    assert_eq!(
        operands,
        PluralOperands {
            n: 2.81,
            i: 2,
            v: 2,
            w: 2,
            f: 81,
            t: 81,
        }
    );
}

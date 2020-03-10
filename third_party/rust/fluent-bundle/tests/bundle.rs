use fluent_bundle::{FluentBundle, FluentResource};
use unic_langid::LanguageIdentifier;

#[test]
fn add_resource_override() {
    let res = FluentResource::try_new("key = Value".to_string()).unwrap();
    let res2 = FluentResource::try_new("key = Value 2".to_string()).unwrap();

    let en_us: LanguageIdentifier = "en-US"
        .parse()
        .expect("Failed to parse a language identifier");
    let mut bundle = FluentBundle::<&FluentResource>::new(&[en_us]);

    bundle.add_resource(&res).expect("Failed to add a resource");

    assert!(bundle.add_resource(&res2).is_err());

    let mut errors = vec![];

    let value = bundle
        .get_message("key")
        .expect("Failed to retireve a message")
        .value
        .expect("Failed to retireve a value of a message");
    assert_eq!(bundle.format_pattern(value, None, &mut errors), "Value");

    bundle.add_resource_overriding(&res2);

    let value = bundle
        .get_message("key")
        .expect("Failed to retireve a message")
        .value
        .expect("Failed to retireve a value of a message");
    assert_eq!(bundle.format_pattern(value, None, &mut errors), "Value 2");

    assert!(errors.is_empty());
}

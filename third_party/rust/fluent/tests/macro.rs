use fluent::fluent_args;
use fluent::FluentValue;
use std::borrow::Cow;

#[test]
fn test_fluent_args() {
    let args = fluent_args![
        "name" => "John",
        "emailCount" => 5,
        "customValue" => FluentValue::from("My Value")
    ];
    assert_eq!(
        args.get("name"),
        Some(&FluentValue::String(Cow::Borrowed("John")))
    );
    assert_eq!(args.get("emailCount"), Some(&FluentValue::try_number(5)));
    assert_eq!(
        args.get("customValue"),
        Some(&FluentValue::String(Cow::Borrowed("My Value")))
    );
}

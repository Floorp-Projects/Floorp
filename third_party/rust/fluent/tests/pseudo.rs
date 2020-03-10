#[cfg(feature = "fluent-pseudo")]
#[test]
fn test_pseudo() {
    use fluent::{FluentBundle, FluentResource};
    use fluent_pseudo::transform;
    use std::borrow::Cow;

    fn transform_wrapper(s: &str) -> Cow<str> {
        transform(s, false, true)
    }

    let mut bundle = FluentBundle::default();

    let res = FluentResource::try_new(String::from("key = Hello World")).unwrap();
    bundle.add_resource(res).unwrap();

    {
        let msg = bundle.get_message("key").unwrap();
        let mut errors = vec![];
        let val = bundle.format_pattern(msg.value.unwrap(), None, &mut errors);

        assert_eq!(val, "Hello World");
    }

    bundle.set_transform(Some(transform_wrapper));

    {
        let msg = bundle.get_message("key").unwrap();
        let mut errors = vec![];
        let val = bundle.format_pattern(msg.value.unwrap(), None, &mut errors);

        assert_eq!(val, "Ħḗḗŀŀǿǿ Ẇǿǿřŀḓ");
    }
}

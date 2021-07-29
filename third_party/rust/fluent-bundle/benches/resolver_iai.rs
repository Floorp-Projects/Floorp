use fluent_bundle::{FluentArgs, FluentBundle, FluentResource, FluentValue};
use fluent_syntax::ast;
use unic_langid::{langid, LanguageIdentifier};

const LANG_EN: LanguageIdentifier = langid!("en");

fn add_functions<R>(name: &'static str, bundle: &mut FluentBundle<R>) {
    match name {
        "preferences" => {
            bundle
                .add_function("PLATFORM", |_args, _named_args| {
                    return "linux".into();
                })
                .expect("Failed to add a function to the bundle.");
        }
        _ => {}
    }
}

fn get_args(name: &str) -> Option<FluentArgs> {
    match name {
        "preferences" => {
            let mut prefs_args = FluentArgs::new();
            prefs_args.set("name", FluentValue::from("John"));
            prefs_args.set("tabCount", FluentValue::from(5));
            prefs_args.set("count", FluentValue::from(3));
            prefs_args.set("version", FluentValue::from("65.0"));
            prefs_args.set("path", FluentValue::from("/tmp"));
            prefs_args.set("num", FluentValue::from(4));
            prefs_args.set("email", FluentValue::from("john@doe.com"));
            prefs_args.set("value", FluentValue::from(4.5));
            prefs_args.set("unit", FluentValue::from("mb"));
            prefs_args.set("service-name", FluentValue::from("Mozilla Disk"));
            Some(prefs_args)
        }
        _ => None,
    }
}

fn get_ids(res: &FluentResource) -> Vec<String> {
    res.entries()
        .filter_map(|entry| match entry {
            ast::Entry::Message(ast::Message { id, .. }) => Some(id.name.to_owned()),
            _ => None,
        })
        .collect()
}

fn iai_resolve_preferences() {
    let files = &[include_str!("preferences.ftl")];
    for source in files {
        let res = FluentResource::try_new(source.to_string()).expect("failed to parse FTL.");
        let ids = get_ids(&res);
        let mut bundle = FluentBundle::new(vec![LANG_EN]);
        bundle.add_resource(res).expect("Failed to add a resource.");
        add_functions("preferences", &mut bundle);
        let args = get_args("preferences");
        let mut s = String::new();
        for id in &ids {
            let msg = bundle.get_message(id).expect("Message found");
            let mut errors = vec![];
            if let Some(value) = msg.value() {
                bundle
                    .write_pattern(&mut s, value, args.as_ref(), &mut errors)
                    .expect("Failed to write a pattern.");
                s.clear();
            }
            for attr in msg.attributes() {
                bundle
                    .write_pattern(&mut s, attr.value(), args.as_ref(), &mut errors)
                    .expect("Failed to write a pattern.");
                s.clear();
            }
            assert!(errors.len() == 0, "Resolver errors: {:#?}", errors);
        }
    }
}

iai::main!(iai_resolve_preferences);

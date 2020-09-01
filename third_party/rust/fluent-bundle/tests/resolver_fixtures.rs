mod helpers;

use std::borrow::Cow;
use std::collections::HashMap;
use std::fs;
use std::iter;
use std::path::Path;

use fluent_bundle::resolve::ResolverError;
use fluent_bundle::FluentArgs;
use fluent_bundle::FluentError;
use fluent_bundle::{FluentBundle as FluentBundleGeneric, FluentResource, FluentValue};
use rand::distributions::Alphanumeric;
use rand::{thread_rng, Rng};
use unic_langid::LanguageIdentifier;

use helpers::*;

type FluentBundle = FluentBundleGeneric<FluentResource>;

fn transform_example(s: &str) -> Cow<str> {
    s.replace("a", "A").into()
}

#[derive(Clone)]
struct ScopeLevel {
    name: String,
    resources: Vec<TestResource>,
    bundles: Vec<TestBundle>,
}

#[derive(Clone)]
struct Scope(Vec<ScopeLevel>);

impl Scope {
    fn get_path(&self) -> String {
        self.0
            .iter()
            .map(|lvl| lvl.name.as_str())
            .collect::<Vec<&str>>()
            .join(" > ")
    }

    fn get_bundles(&self, defaults: &Option<TestDefaults>) -> HashMap<String, FluentBundle> {
        let mut bundles = HashMap::new();

        let mut available_resources = vec![];

        for lvl in self.0.iter() {
            for r in lvl.resources.iter() {
                available_resources.push(r);
            }

            for b in lvl.bundles.iter() {
                let name = b
                    .name
                    .as_ref()
                    .cloned()
                    .unwrap_or_else(|| generate_random_hash());
                let bundle = create_bundle(Some(b), &defaults, &available_resources);
                bundles.insert(name, bundle);
            }
        }
        if bundles.is_empty() {
            let bundle = create_bundle(None, defaults, &available_resources);
            let name = generate_random_hash();
            bundles.insert(name.clone(), bundle);
        }
        bundles
    }
}

fn generate_random_hash() -> String {
    let mut rng = thread_rng();
    let chars: String = iter::repeat(())
        .map(|()| rng.sample(Alphanumeric))
        .take(7)
        .collect();
    chars
}

fn test_fixture(fixture: &TestFixture, defaults: &Option<TestDefaults>) {
    for suite in &fixture.suites {
        test_suite(&suite, defaults, Scope(vec![]));
    }
}

fn create_bundle(
    b: Option<&TestBundle>,
    defaults: &Option<TestDefaults>,
    resources: &Vec<&TestResource>,
) -> FluentBundle {
    let mut errors = vec![];

    let locales: Vec<LanguageIdentifier> = b
        .and_then(|b| b.locales.as_ref())
        .or_else(|| {
            defaults
                .as_ref()
                .and_then(|defaults| defaults.bundle.locales.as_ref())
        })
        .map(|locs| {
            locs.into_iter()
                .map(|s| s.parse().expect("Parsing failed."))
                .collect()
        })
        .expect("Failed to calculate locales.");
    let mut bundle = FluentBundle::new(&locales);
    let use_isolating = b.and_then(|b| b.use_isolating).or_else(|| {
        defaults
            .as_ref()
            .and_then(|defaults| defaults.bundle.use_isolating)
    });
    if let Some(use_isolating) = use_isolating {
        bundle.set_use_isolating(use_isolating);
    }
    let transform = b.and_then(|b| b.transform.as_ref()).or_else(|| {
        defaults
            .as_ref()
            .and_then(|defaults| defaults.bundle.transform.as_ref())
    });
    if let Some(transform) = transform {
        match transform.as_str() {
            "example" => bundle.set_transform(Some(transform_example)),
            _ => unimplemented!(),
        }
    }
    if let Some(&TestBundle {
        functions: Some(ref fns),
        ..
    }) = b
    {
        for f in fns {
            let result = match f.as_str() {
                "CONCAT" => bundle.add_function(f.as_str(), |args, _name_args| {
                    args.iter()
                        .fold(String::new(), |acc, x| match x {
                            FluentValue::String(s) => acc + &s.to_string(),
                            FluentValue::Number(n) => acc + &n.value.to_string(),
                            _ => acc,
                        })
                        .into()
                }),
                "SUM" => bundle.add_function(f.as_str(), |args, _name_args| {
                    args.iter()
                        .fold(0.0, |acc, x| {
                            if let FluentValue::Number(v) = x {
                                acc + v.value
                            } else {
                                panic!("Type cannot be used in SUM");
                            }
                        })
                        .into()
                }),
                "IDENTITY" => bundle.add_function(f.as_str(), |args, _name_args| {
                    args.get(0).cloned().unwrap_or(FluentValue::None)
                }),
                "NUMBER" => bundle.add_function(f.as_str(), |args, _name_args| {
                    args.get(0).expect("Argument must be passed").clone()
                }),
                _ => unimplemented!("No such function."),
            };
            if let Err(err) = result {
                errors.push(err);
            }
        }
    }
    let res_subset = b.and_then(|b| b.resources.as_ref());

    for res in resources.iter() {
        if let Some(res_subset) = res_subset {
            if let Some(ref name) = res.name {
                if !res_subset.contains(name) {
                    continue;
                }
            }
        }
        let res = get_resource(res);
        if let Err(mut err) = bundle.add_resource(res) {
            errors.append(&mut err);
        }
    }
    test_errors(&errors, b.map(|b| b.errors.as_ref()));
    bundle
}

fn get_resource(resource: &TestResource) -> FluentResource {
    let res = FluentResource::try_new(resource.source.clone());

    if resource.errors.is_empty() {
        res.expect("Failed to parse an FTL resource.")
    } else {
        let (res, errors) = match res {
            Ok(r) => (r, vec![]),
            Err((res, err)) => {
                let err = err.into_iter().map(|err| err.into()).collect();
                (res, err)
            }
        };
        test_errors(&errors, Some(&resource.errors));
        res
    }
}

fn test_suite(suite: &TestSuite, defaults: &Option<TestDefaults>, mut scope: Scope) {
    if suite.skip == Some(true) {
        return;
    }

    scope.0.push(ScopeLevel {
        name: suite.name.clone(),
        bundles: suite.bundles.clone(),
        resources: suite.resources.clone(),
    });

    for test in &suite.tests {
        test_test(test, defaults, scope.clone());
    }

    for sub_suite in &suite.suites {
        test_suite(sub_suite, defaults, scope.clone());
    }
}

fn test_test(test: &Test, defaults: &Option<TestDefaults>, mut scope: Scope) {
    if test.skip == Some(true) {
        return;
    }

    scope.0.push(ScopeLevel {
        name: test.name.clone(),
        bundles: test.bundles.clone(),
        resources: test.resources.clone(),
    });

    for assert in &test.asserts {
        let bundles = scope.get_bundles(defaults);
        let bundle = if let Some(ref bundle_name) = assert.bundle {
            bundles
                .get(bundle_name)
                .expect("Failed to retrieve bundle.")
        } else if bundles.len() == 1 {
            let name = bundles.keys().into_iter().last().unwrap();
            bundles.get(name).expect("Failed to retrieve bundle.")
        } else {
            panic!();
        };
        let mut errors = vec![];

        if let Some(expected_missing) = assert.missing {
            let missing = if let Some(ref attr) = assert.attribute {
                if let Some(msg) = bundle.get_message(&assert.id) {
                    msg.attributes.contains_key(attr.as_str())
                } else {
                    false
                }
            } else {
                !bundle.has_message(&assert.id)
            };
            assert_eq!(
                missing,
                expected_missing,
                "Expected pattern to be `missing: {}` for {} in {}",
                expected_missing,
                assert.id,
                scope.get_path()
            );
        } else {
            if let Some(ref expected_value) = assert.value {
                let msg = bundle.get_message(&assert.id).expect(&format!(
                    "Failed to retrieve message `{}` in {}.",
                    &assert.id,
                    scope.get_path()
                ));
                let val = if let Some(ref attr) = assert.attribute {
                    msg.attributes.get(attr.as_str()).expect(&format!(
                        "Failed to retrieve an attribute of a message {}.{}.",
                        assert.id, attr
                    ))
                } else {
                    msg.value.expect(&format!(
                        "Failed to retrieve a value of a message {}.",
                        assert.id
                    ))
                };

                let args: Option<FluentArgs> = assert.args.as_ref().map(|args| {
                    args.iter()
                        .map(|(k, v)| {
                            let val = match v {
                                TestArgumentValue::String(s) => s.as_str().into(),
                                TestArgumentValue::Number(n) => n.into(),
                            };
                            (k.as_str(), val)
                        })
                        .collect()
                });
                let value = bundle.format_pattern(&val, args.as_ref(), &mut errors);
                assert_eq!(
                    &value,
                    expected_value,
                    "Values don't match in {}",
                    scope.get_path()
                );
                test_errors(&errors, Some(&assert.errors));
            } else {
                panic!("Value field expected.");
            }
        }
    }
}

fn test_errors(errors: &[FluentError], reference: Option<&[TestError]>) {
    let reference = reference.unwrap_or(&[]);
    assert_eq!(errors.len(), reference.len());
    for (error, reference) in errors.into_iter().zip(reference) {
        match error {
            FluentError::ResolverError(err) => match err {
                ResolverError::Reference(desc) => {
                    assert_eq!(reference.desc.as_ref(), Some(desc));
                    assert_eq!(reference.error_type, "Reference");
                }
                ResolverError::Cyclic => {
                    assert_eq!(reference.error_type, "Cyclic");
                }
                ResolverError::TooManyPlaceables => {
                    assert_eq!(reference.error_type, "TooManyPlaceables");
                }
                _ => unimplemented!(),
            },
            FluentError::ParserError(_) => {
                assert_eq!(reference.error_type, "Parser");
            }
            FluentError::Overriding { .. } => {
                assert_eq!(reference.error_type, "Overriding");
            }
        }
    }
}

#[test]
fn resolve_fixtures() {
    let dir = "./tests/fixtures/";
    let mut defaults_path = String::from(dir);
    defaults_path.push_str("defaults.yaml");
    let defaults = if Path::new(&defaults_path).exists() {
        Some(get_defaults(&defaults_path).expect("Failed to read defaults."))
    } else {
        None
    };
    for entry in fs::read_dir(dir).expect("Failed to read glob pattern.") {
        let entry = entry.expect("Entry doesn't exist.");
        let path = entry.path();
        let path_str = path.to_str().expect("Failed to convert path to string.");
        if path_str.contains("defaults.yaml") {
            continue;
        }
        println!("PATH: {:#?}", path_str);
        let fixture = get_fixture(path_str).expect("Loading fixture failed.");

        test_fixture(&fixture, &defaults);
    }
}

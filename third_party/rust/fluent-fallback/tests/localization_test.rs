use std::borrow::Cow;
use std::fs;

use fluent_bundle::{
    resolver::errors::{ReferenceKind, ResolverError},
    FluentArgs, FluentBundle, FluentError, FluentResource,
};
use fluent_fallback::{
    env::LocalesProvider,
    generator::{BundleGenerator, FluentBundleResult},
    types::{L10nKey, ResourceId},
    Localization, LocalizationError,
};
use rustc_hash::FxHashSet;
use std::cell::RefCell;
use std::rc::Rc;
use unic_langid::{langid, LanguageIdentifier};

struct InnerLocales {
    locales: RefCell<Vec<LanguageIdentifier>>,
}

impl InnerLocales {
    pub fn insert(&self, index: usize, element: LanguageIdentifier) {
        self.locales.borrow_mut().insert(index, element);
    }
}

#[derive(Clone)]
struct Locales {
    inner: Rc<InnerLocales>,
}

impl Locales {
    pub fn new(locales: Vec<LanguageIdentifier>) -> Self {
        Self {
            inner: Rc::new(InnerLocales {
                locales: RefCell::new(locales),
            }),
        }
    }

    pub fn insert(&mut self, index: usize, element: LanguageIdentifier) {
        self.inner.insert(index, element);
    }
}

impl LocalesProvider for Locales {
    type Iter = <Vec<LanguageIdentifier> as IntoIterator>::IntoIter;
    fn locales(&self) -> Self::Iter {
        self.inner.locales.borrow().clone().into_iter()
    }
}

// Due to limitation of trait, we need a nameable Iterator type.  Due to the
// lack of GATs, these have to own members instead of taking slices.
struct BundleIter {
    locales: <Vec<LanguageIdentifier> as IntoIterator>::IntoIter,
    res_ids: FxHashSet<ResourceId>,
}

impl Iterator for BundleIter {
    type Item = FluentBundleResult<FluentResource>;

    fn next(&mut self) -> Option<Self::Item> {
        let locale = self.locales.next()?;

        let mut bundle = FluentBundle::new(vec![locale.clone()]);
        bundle.set_use_isolating(false);

        let mut errors = vec![];

        for res_id in &self.res_ids {
            let full_path = format!("./tests/resources/{}/{}", locale, res_id);
            let source = fs::read_to_string(full_path).unwrap();
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
        mut self: std::pin::Pin<&mut Self>,
        _cx: &mut std::task::Context<'_>,
    ) -> std::task::Poll<Option<Self::Item>> {
        if let Some(locale) = self.locales.next() {
            let mut bundle = FluentBundle::new(vec![locale.clone()]);
            bundle.set_use_isolating(false);

            let mut errors = vec![];
            for res_id in &self.res_ids {
                let full_path = format!("./tests/resources/{}/{}", locale, res_id.value);
                let source = fs::read_to_string(full_path).unwrap();
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
                Some(Ok(bundle)).into()
            } else {
                Some(Err((bundle, errors))).into()
            }
        } else {
            None.into()
        }
    }
}

struct ResourceManager;

impl BundleGenerator for ResourceManager {
    type Resource = FluentResource;
    type LocalesIter = std::vec::IntoIter<LanguageIdentifier>;
    type Iter = BundleIter;
    type Stream = BundleIter;

    fn bundles_iter(
        &self,
        locales: Self::LocalesIter,
        res_ids: FxHashSet<ResourceId>,
    ) -> Self::Iter {
        BundleIter { locales, res_ids }
    }

    fn bundles_stream(
        &self,
        locales: Self::LocalesIter,
        res_ids: FxHashSet<ResourceId>,
    ) -> Self::Stream {
        BundleIter { locales, res_ids }
    }
}

#[test]
fn localization_format() {
    let resource_ids: Vec<ResourceId> = vec!["test.ftl".into(), "test2.ftl".into()];
    let locales = Locales::new(vec![langid!("pl"), langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let loc = Localization::with_env(resource_ids, true, locales, res_mgr);
    let bundles = loc.bundles();

    let value = bundles
        .format_value_sync("hello-world", None, &mut errors)
        .unwrap();
    assert_eq!(value, Some(Cow::Borrowed("Hello World [pl]")));

    let value = bundles
        .format_value_sync("missing-message", None, &mut errors)
        .unwrap();
    assert_eq!(value, None);

    let value = bundles
        .format_value_sync("hello-world-3", None, &mut errors)
        .unwrap();
    assert_eq!(value, Some(Cow::Borrowed("Hello World 3 [en]")));

    assert_eq!(errors.len(), 4);
}

#[test]
fn localization_on_change() {
    let resource_ids: Vec<ResourceId> = vec!["test.ftl".into(), "test2.ftl".into()];

    let mut locales = Locales::new(vec![langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let mut loc = Localization::with_env(resource_ids, true, locales.clone(), res_mgr);
    let bundles = loc.bundles();

    let value = bundles
        .format_value_sync("hello-world", None, &mut errors)
        .unwrap();
    assert_eq!(value, Some(Cow::Borrowed("Hello World [en]")));

    locales.insert(0, langid!("pl"));
    loc.on_change();

    let bundles = loc.bundles();
    let value = bundles
        .format_value_sync("hello-world", None, &mut errors)
        .unwrap();
    assert_eq!(value, Some(Cow::Borrowed("Hello World [pl]")));
}

#[test]
fn localization_format_value_missing_errors() {
    let resource_ids: Vec<ResourceId> = vec!["test.ftl".into(), "test2.ftl".into()];

    let locales = Locales::new(vec![langid!("pl"), langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let loc = Localization::with_env(resource_ids, true, locales.clone(), res_mgr);
    let bundles = loc.bundles();

    let _ = bundles
        .format_value_sync("missing-message", None, &mut errors)
        .unwrap();
    assert_eq!(
        errors,
        vec![
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: None
            },
        ]
    );

    errors.clear();

    let _ = bundles
        .format_value_sync("message-3", None, &mut errors)
        .unwrap();
    assert_eq!(
        errors,
        vec![
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: None
            },
        ]
    );
}

#[test]
fn localization_format_value_sync_missing_errors() {
    let resource_ids: Vec<ResourceId> = vec!["test.ftl".into(), "test2.ftl".into()];

    let locales = Locales::new(vec![langid!("pl"), langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let loc = Localization::with_env(resource_ids, true, locales.clone(), res_mgr);
    let bundles = loc.bundles();

    let _ = bundles
        .format_value_sync("missing-message", None, &mut errors)
        .unwrap();
    assert_eq!(
        errors,
        vec![
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: None
            },
        ]
    );

    errors.clear();

    let _ = bundles
        .format_value_sync("message-3", None, &mut errors)
        .unwrap();
    assert_eq!(
        errors,
        vec![
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: None
            },
        ]
    );
}

#[test]
fn localization_format_values_sync_missing_errors() {
    let resource_ids: Vec<ResourceId> = vec!["test.ftl".into(), "test2.ftl".into()];

    let locales = Locales::new(vec![langid!("pl"), langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let loc = Localization::with_env(resource_ids, true, locales.clone(), res_mgr);
    let bundles = loc.bundles();

    let _ = bundles
        .format_values_sync(
            &["missing-message".into(), "missing-message-2".into()],
            &mut errors,
        )
        .unwrap();
    assert_eq!(
        errors,
        vec![
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message-2".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message-2".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: None
            },
            LocalizationError::MissingMessage {
                id: "missing-message-2".to_string(),
                locale: None
            },
        ]
    );

    errors.clear();

    let _ = bundles
        .format_values_sync(&["message-3".into()], &mut errors)
        .unwrap();
    assert_eq!(
        errors,
        vec![
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingValue {
                id: "message-3".to_string(),
                locale: None
            },
        ]
    );
}

#[test]
fn localization_format_messages_sync_missing_errors() {
    let resource_ids: Vec<ResourceId> = vec!["test.ftl".into(), "test2.ftl".into()];

    let locales = Locales::new(vec![langid!("pl"), langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let loc = Localization::with_env(resource_ids, true, locales.clone(), res_mgr);
    let bundles = loc.bundles();

    let _ = bundles
        .format_messages_sync(
            &["missing-message".into(), "missing-message-2".into()],
            &mut errors,
        )
        .unwrap();
    assert_eq!(
        errors,
        vec![
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message-2".to_string(),
                locale: Some(langid!("pl"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message-2".to_string(),
                locale: Some(langid!("en-US"))
            },
            LocalizationError::MissingMessage {
                id: "missing-message".to_string(),
                locale: None
            },
            LocalizationError::MissingMessage {
                id: "missing-message-2".to_string(),
                locale: None
            },
        ]
    );
}

#[test]
fn localization_format_missing_argument_error() {
    let resource_ids: Vec<ResourceId> = vec!["test2.ftl".into()];
    let locales = Locales::new(vec![langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let loc = Localization::with_env(resource_ids, true, locales, res_mgr);
    let bundles = loc.bundles();

    let mut args = FluentArgs::new();
    args.set("userName", "John");
    let keys = vec![L10nKey {
        id: "message-4".into(),
        args: Some(args),
    }];

    let msgs = bundles.format_messages_sync(&keys, &mut errors).unwrap();
    assert_eq!(
        msgs.get(0).unwrap().as_ref().unwrap().value,
        Some(Cow::Borrowed("Hello, John. [en]"))
    );
    assert_eq!(errors.len(), 0);

    let keys = vec![L10nKey {
        id: "message-4".into(),
        args: None,
    }];
    let msgs = bundles.format_messages_sync(&keys, &mut errors).unwrap();
    assert_eq!(
        msgs.get(0).unwrap().as_ref().unwrap().value,
        Some(Cow::Borrowed("Hello, {$userName}. [en]"))
    );
    assert_eq!(
        errors,
        vec![LocalizationError::Resolver {
            id: "message-4".to_string(),
            locale: langid!("en-US"),
            errors: vec![FluentError::ResolverError(ResolverError::Reference(
                ReferenceKind::Variable {
                    id: "userName".to_string(),
                }
            ))],
        },]
    );
}

#[tokio::test]
async fn localization_handle_state_changes_mid_async() {
    let resource_ids: Vec<ResourceId> = vec!["test.ftl".into()];
    let locales = Locales::new(vec![langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let mut loc = Localization::with_env(resource_ids, false, locales, res_mgr);

    let bundles = loc.bundles().clone();

    loc.add_resource_id("test2.ftl".to_string());

    bundles.format_value("key", None, &mut errors).await;
}

#[test]
fn localization_duplicate_resources() {
    let resource_ids: Vec<ResourceId> =
        vec!["test.ftl".into(), "test2.ftl".into(), "test2.ftl".into()];
    let locales = Locales::new(vec![langid!("pl"), langid!("en-US")]);
    let res_mgr = ResourceManager;
    let mut errors = vec![];

    let loc = Localization::with_env(resource_ids, true, locales, res_mgr);
    let bundles = loc.bundles();

    let value = bundles
        .format_value_sync("hello-world", None, &mut errors)
        .unwrap();
    assert_eq!(value, Some(Cow::Borrowed("Hello World [pl]")));

    assert_eq!(errors.len(), 0, "There were no errors");
}

use crate::{
    cache::{AsyncCache, Cache},
    env::LocalesProvider,
    errors::LocalizationError,
    generator::{BundleGenerator, BundleIterator, BundleStream},
    types::{L10nAttribute, L10nKey, L10nMessage, ResourceId},
};
use fluent_bundle::{FluentArgs, FluentBundle, FluentError};
use rustc_hash::FxHashSet;
use std::borrow::Cow;

pub enum BundlesInner<G>
where
    G: BundleGenerator,
{
    Iter(Cache<G::Iter, G::Resource>),
    Stream(AsyncCache<G::Stream, G::Resource>),
}

pub struct Bundles<G>(BundlesInner<G>)
where
    G: BundleGenerator;

impl<G> Bundles<G>
where
    G: BundleGenerator,
    G::Iter: BundleIterator,
{
    pub fn prefetch_sync(&self) {
        match &self.0 {
            BundlesInner::Iter(iter) => iter.prefetch(),
            BundlesInner::Stream(_) => panic!("Can't prefetch a sync bundle set asynchronously"),
        }
    }
}

impl<G> Bundles<G>
where
    G: BundleGenerator,
    G::Stream: BundleStream,
{
    pub async fn prefetch_async(&self) {
        match &self.0 {
            BundlesInner::Iter(_) => panic!("Can't prefetch a async bundle set synchronously"),
            BundlesInner::Stream(stream) => stream.prefetch().await,
        }
    }
}

impl<G> Bundles<G>
where
    G: BundleGenerator,
{
    pub fn new<P>(sync: bool, res_ids: FxHashSet<ResourceId>, generator: &G, provider: &P) -> Self
    where
        G: BundleGenerator<LocalesIter = P::Iter>,
        P: LocalesProvider,
    {
        Self(if sync {
            BundlesInner::Iter(Cache::new(
                generator.bundles_iter(provider.locales(), res_ids),
            ))
        } else {
            BundlesInner::Stream(AsyncCache::new(
                generator.bundles_stream(provider.locales(), res_ids),
            ))
        })
    }

    pub async fn format_value<'l>(
        &'l self,
        id: &'l str,
        args: Option<&'l FluentArgs<'_>>,
        errors: &mut Vec<LocalizationError>,
    ) -> Option<Cow<'l, str>> {
        match &self.0 {
            BundlesInner::Iter(cache) => Self::format_value_from_iter(cache, id, args, errors),
            BundlesInner::Stream(stream) => {
                Self::format_value_from_stream(stream, id, args, errors).await
            }
        }
    }

    pub async fn format_values<'l>(
        &'l self,
        keys: &'l [L10nKey<'l>],
        errors: &mut Vec<LocalizationError>,
    ) -> Vec<Option<Cow<'l, str>>> {
        match &self.0 {
            BundlesInner::Iter(cache) => Self::format_values_from_iter(cache, keys, errors),
            BundlesInner::Stream(stream) => {
                Self::format_values_from_stream(stream, keys, errors).await
            }
        }
    }

    pub async fn format_messages<'l>(
        &'l self,
        keys: &'l [L10nKey<'l>],
        errors: &mut Vec<LocalizationError>,
    ) -> Vec<Option<L10nMessage<'l>>> {
        match &self.0 {
            BundlesInner::Iter(cache) => Self::format_messages_from_iter(cache, keys, errors),
            BundlesInner::Stream(stream) => {
                Self::format_messages_from_stream(stream, keys, errors).await
            }
        }
    }

    pub fn format_value_sync<'l>(
        &'l self,
        id: &'l str,
        args: Option<&'l FluentArgs>,
        errors: &mut Vec<LocalizationError>,
    ) -> Result<Option<Cow<'l, str>>, LocalizationError> {
        match &self.0 {
            BundlesInner::Iter(cache) => Ok(Self::format_value_from_iter(cache, id, args, errors)),
            BundlesInner::Stream(_) => Err(LocalizationError::SyncRequestInAsyncMode),
        }
    }

    pub fn format_values_sync<'l>(
        &'l self,
        keys: &'l [L10nKey<'l>],
        errors: &mut Vec<LocalizationError>,
    ) -> Result<Vec<Option<Cow<'l, str>>>, LocalizationError> {
        match &self.0 {
            BundlesInner::Iter(cache) => Ok(Self::format_values_from_iter(cache, keys, errors)),
            BundlesInner::Stream(_) => Err(LocalizationError::SyncRequestInAsyncMode),
        }
    }

    pub fn format_messages_sync<'l>(
        &'l self,
        keys: &'l [L10nKey<'l>],
        errors: &mut Vec<LocalizationError>,
    ) -> Result<Vec<Option<L10nMessage<'l>>>, LocalizationError> {
        match &self.0 {
            BundlesInner::Iter(cache) => Ok(Self::format_messages_from_iter(cache, keys, errors)),
            BundlesInner::Stream(_) => Err(LocalizationError::SyncRequestInAsyncMode),
        }
    }
}

macro_rules! format_value_from_inner {
    ($step:expr, $id:expr, $args:expr, $errors:expr) => {
        let mut found_message = false;

        while let Some(bundle) = $step {
            let bundle = bundle.as_ref().unwrap_or_else(|(bundle, err)| {
                $errors.extend(err.iter().cloned().map(Into::into));
                bundle
            });

            if let Some(msg) = bundle.get_message($id) {
                found_message = true;
                if let Some(value) = msg.value() {
                    let mut format_errors = vec![];
                    let result = bundle.format_pattern(value, $args, &mut format_errors);
                    if !format_errors.is_empty() {
                        $errors.push(LocalizationError::Resolver {
                            id: $id.to_string(),
                            locale: bundle.locales[0].clone(),
                            errors: format_errors,
                        });
                    }
                    return Some(result);
                } else {
                    $errors.push(LocalizationError::MissingValue {
                        id: $id.to_string(),
                        locale: Some(bundle.locales[0].clone()),
                    });
                }
            } else {
                $errors.push(LocalizationError::MissingMessage {
                    id: $id.to_string(),
                    locale: Some(bundle.locales[0].clone()),
                });
            }
        }
        if found_message {
            $errors.push(LocalizationError::MissingValue {
                id: $id.to_string(),
                locale: None,
            });
        } else {
            $errors.push(LocalizationError::MissingMessage {
                id: $id.to_string(),
                locale: None,
            });
        }
        return None;
    };
}

#[derive(Clone)]
enum Value<'l> {
    Present(Cow<'l, str>),
    Missing,
    None,
}

macro_rules! format_values_from_inner {
    ($step:expr, $keys:expr, $errors:expr) => {
        let mut cells = vec![Value::None; $keys.len()];

        while let Some(bundle) = $step {
            let bundle = bundle.as_ref().unwrap_or_else(|(bundle, err)| {
                $errors.extend(err.iter().cloned().map(Into::into));
                bundle
            });

            let mut has_missing = false;

            for (key, cell) in $keys
                .iter()
                .zip(&mut cells)
                .filter(|(_, cell)| !matches!(cell, Value::Present(_)))
            {
                if let Some(msg) = bundle.get_message(&key.id) {
                    if let Some(value) = msg.value() {
                        let mut format_errors = vec![];
                        *cell = Value::Present(bundle.format_pattern(
                            value,
                            key.args.as_ref(),
                            &mut format_errors,
                        ));
                        if !format_errors.is_empty() {
                            $errors.push(LocalizationError::Resolver {
                                id: key.id.to_string(),
                                locale: bundle.locales[0].clone(),
                                errors: format_errors,
                            });
                        }
                    } else {
                        *cell = Value::Missing;
                        has_missing = true;
                        $errors.push(LocalizationError::MissingValue {
                            id: key.id.to_string(),
                            locale: Some(bundle.locales[0].clone()),
                        });
                    }
                } else {
                    has_missing = true;
                    $errors.push(LocalizationError::MissingMessage {
                        id: key.id.to_string(),
                        locale: Some(bundle.locales[0].clone()),
                    });
                }
            }
            if !has_missing {
                break;
            }
        }

        return $keys
            .iter()
            .zip(cells)
            .map(|(key, value)| match value {
                Value::Present(value) => Some(value),
                Value::Missing => {
                    $errors.push(LocalizationError::MissingValue {
                        id: key.id.to_string(),
                        locale: None,
                    });
                    None
                }
                Value::None => {
                    $errors.push(LocalizationError::MissingMessage {
                        id: key.id.to_string(),
                        locale: None,
                    });
                    None
                }
            })
            .collect();
    };
}

macro_rules! format_messages_from_inner {
    ($step:expr, $keys:expr, $errors:expr) => {
        let mut result = vec![None; $keys.len()];

        let mut is_complete = false;

        while let Some(bundle) = $step {
            let bundle = bundle.as_ref().unwrap_or_else(|(bundle, err)| {
                $errors.extend(err.iter().cloned().map(Into::into));
                bundle
            });

            let mut has_missing = false;
            for (key, cell) in $keys
                .iter()
                .zip(&mut result)
                .filter(|(_, cell)| cell.is_none())
            {
                let mut format_errors = vec![];
                let msg = Self::format_message_from_bundle(bundle, key, &mut format_errors);

                if msg.is_none() {
                    has_missing = true;
                    $errors.push(LocalizationError::MissingMessage {
                        id: key.id.to_string(),
                        locale: Some(bundle.locales[0].clone()),
                    });
                } else if !format_errors.is_empty() {
                    $errors.push(LocalizationError::Resolver {
                        id: key.id.to_string(),
                        locale: bundle.locales.get(0).cloned().unwrap(),
                        errors: format_errors,
                    });
                }

                *cell = msg;
            }
            if !has_missing {
                is_complete = true;
                break;
            }
        }

        if !is_complete {
            for (key, _) in $keys
                .iter()
                .zip(&mut result)
                .filter(|(_, cell)| cell.is_none())
            {
                $errors.push(LocalizationError::MissingMessage {
                    id: key.id.to_string(),
                    locale: None,
                });
            }
        }

        return result;
    };
}

impl<G> Bundles<G>
where
    G: BundleGenerator,
{
    fn format_value_from_iter<'l>(
        cache: &'l Cache<G::Iter, G::Resource>,
        id: &'l str,
        args: Option<&'l FluentArgs>,
        errors: &mut Vec<LocalizationError>,
    ) -> Option<Cow<'l, str>> {
        let mut bundle_iter = cache.into_iter();
        format_value_from_inner!(bundle_iter.next(), id, args, errors);
    }

    async fn format_value_from_stream<'l>(
        stream: &'l AsyncCache<G::Stream, G::Resource>,
        id: &'l str,
        args: Option<&'l FluentArgs<'_>>,
        errors: &mut Vec<LocalizationError>,
    ) -> Option<Cow<'l, str>> {
        use futures::StreamExt;

        let mut bundle_stream = stream.stream();
        format_value_from_inner!(bundle_stream.next().await, id, args, errors);
    }

    async fn format_messages_from_stream<'l>(
        stream: &'l AsyncCache<G::Stream, G::Resource>,
        keys: &'l [L10nKey<'l>],
        errors: &mut Vec<LocalizationError>,
    ) -> Vec<Option<L10nMessage<'l>>> {
        use futures::StreamExt;
        let mut bundle_stream = stream.stream();
        format_messages_from_inner!(bundle_stream.next().await, keys, errors);
    }

    async fn format_values_from_stream<'l>(
        stream: &'l AsyncCache<G::Stream, G::Resource>,
        keys: &'l [L10nKey<'l>],
        errors: &mut Vec<LocalizationError>,
    ) -> Vec<Option<Cow<'l, str>>> {
        use futures::StreamExt;
        let mut bundle_stream = stream.stream();

        format_values_from_inner!(bundle_stream.next().await, keys, errors);
    }

    fn format_message_from_bundle<'l>(
        bundle: &'l FluentBundle<G::Resource>,
        key: &'l L10nKey,
        format_errors: &mut Vec<FluentError>,
    ) -> Option<L10nMessage<'l>> {
        let msg = bundle.get_message(&key.id)?;
        let value = msg
            .value()
            .map(|pattern| bundle.format_pattern(pattern, key.args.as_ref(), format_errors));
        let attributes = msg
            .attributes()
            .map(|attr| {
                let value = bundle.format_pattern(attr.value(), key.args.as_ref(), format_errors);
                L10nAttribute {
                    name: attr.id().into(),
                    value,
                }
            })
            .collect();
        Some(L10nMessage { value, attributes })
    }

    fn format_messages_from_iter<'l>(
        cache: &'l Cache<G::Iter, G::Resource>,
        keys: &'l [L10nKey<'l>],
        errors: &mut Vec<LocalizationError>,
    ) -> Vec<Option<L10nMessage<'l>>> {
        let mut bundle_iter = cache.into_iter();
        format_messages_from_inner!(bundle_iter.next(), keys, errors);
    }

    fn format_values_from_iter<'l>(
        cache: &'l Cache<G::Iter, G::Resource>,
        keys: &'l [L10nKey<'l>],
        errors: &mut Vec<LocalizationError>,
    ) -> Vec<Option<Cow<'l, str>>> {
        let mut bundle_iter = cache.into_iter();
        format_values_from_inner!(bundle_iter.next(), keys, errors);
    }
}

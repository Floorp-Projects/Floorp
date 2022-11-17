use fluent_bundle::{FluentBundle, FluentError, FluentResource};
use futures::Stream;
use rustc_hash::FxHashSet;
use std::borrow::Borrow;
use unic_langid::LanguageIdentifier;

use crate::types::ResourceId;

pub type FluentBundleResult<R> = Result<FluentBundle<R>, (FluentBundle<R>, Vec<FluentError>)>;

pub trait BundleIterator {
    fn prefetch_sync(&mut self) {}
}

#[async_trait::async_trait(?Send)]
pub trait BundleStream {
    async fn prefetch_async(&mut self) {}
}

pub trait BundleGenerator {
    type Resource: Borrow<FluentResource>;
    type LocalesIter: Iterator<Item = LanguageIdentifier>;
    type Iter: Iterator<Item = FluentBundleResult<Self::Resource>>;
    type Stream: Stream<Item = FluentBundleResult<Self::Resource>>;

    fn bundles_iter(
        &self,
        _locales: Self::LocalesIter,
        _res_ids: FxHashSet<ResourceId>,
    ) -> Self::Iter {
        unimplemented!();
    }

    fn bundles_stream(
        &self,
        _locales: Self::LocalesIter,
        _res_ids: FxHashSet<ResourceId>,
    ) -> Self::Stream {
        unimplemented!();
    }
}

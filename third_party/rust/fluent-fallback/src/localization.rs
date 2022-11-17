use crate::{
    bundles::Bundles,
    env::LocalesProvider,
    generator::{BundleGenerator, BundleIterator, BundleStream},
    types::ResourceId,
};
use once_cell::sync::OnceCell;
use rustc_hash::FxHashSet;
use std::rc::Rc;

pub struct Localization<G, P>
where
    G: BundleGenerator<LocalesIter = P::Iter>,
    P: LocalesProvider,
{
    bundles: OnceCell<Rc<Bundles<G>>>,
    generator: G,
    provider: P,
    sync: bool,
    res_ids: FxHashSet<ResourceId>,
}

impl<G, P> Localization<G, P>
where
    G: BundleGenerator<LocalesIter = P::Iter> + Default,
    P: LocalesProvider + Default,
{
    pub fn new<I>(res_ids: I, sync: bool) -> Self
    where
        I: IntoIterator<Item = ResourceId>,
    {
        Self {
            bundles: OnceCell::new(),
            generator: G::default(),
            provider: P::default(),
            sync,
            res_ids: FxHashSet::from_iter(res_ids.into_iter()),
        }
    }
}

impl<G, P> Localization<G, P>
where
    G: BundleGenerator<LocalesIter = P::Iter>,
    P: LocalesProvider,
{
    pub fn with_env<I>(res_ids: I, sync: bool, provider: P, generator: G) -> Self
    where
        I: IntoIterator<Item = ResourceId>,
    {
        Self {
            bundles: OnceCell::new(),
            generator,
            provider,
            sync,
            res_ids: FxHashSet::from_iter(res_ids.into_iter()),
        }
    }

    pub fn is_sync(&self) -> bool {
        self.sync
    }

    pub fn add_resource_id<T: Into<ResourceId>>(&mut self, res_id: T) {
        self.res_ids.insert(res_id.into());
        self.on_change();
    }

    pub fn add_resource_ids(&mut self, res_ids: Vec<ResourceId>) {
        self.res_ids.extend(res_ids);
        self.on_change();
    }

    pub fn remove_resource_id<T: PartialEq<ResourceId>>(&mut self, res_id: T) -> usize {
        self.res_ids.retain(|x| !res_id.eq(x));
        self.on_change();
        self.res_ids.len()
    }

    pub fn remove_resource_ids(&mut self, res_ids: Vec<ResourceId>) -> usize {
        self.res_ids.retain(|x| !res_ids.contains(x));
        self.on_change();
        self.res_ids.len()
    }

    pub fn set_async(&mut self) {
        if self.sync {
            self.sync = false;
            self.on_change();
        }
    }

    pub fn on_change(&mut self) {
        self.bundles.take();
    }
}

impl<G, P> Localization<G, P>
where
    G: BundleGenerator<LocalesIter = P::Iter>,
    G::Iter: BundleIterator,
    P: LocalesProvider,
{
    pub fn prefetch_sync(&mut self) {
        let bundles = self.bundles();
        bundles.prefetch_sync();
    }
}

impl<G, P> Localization<G, P>
where
    G: BundleGenerator<LocalesIter = P::Iter>,
    G::Stream: BundleStream,
    P: LocalesProvider,
{
    pub async fn prefetch_async(&mut self) {
        let bundles = self.bundles();
        bundles.prefetch_async().await
    }
}

impl<G, P> Localization<G, P>
where
    G: BundleGenerator<LocalesIter = P::Iter>,
    P: LocalesProvider,
{
    pub fn bundles(&self) -> &Rc<Bundles<G>> {
        self.bundles.get_or_init(|| {
            Rc::new(Bundles::new(
                self.sync,
                self.res_ids.clone(),
                &self.generator,
                &self.provider,
            ))
        })
    }
}

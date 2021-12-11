//! Traits required to provide environment driven data for [`Localization`](crate::Localization).
//!
//! Since [`Localization`](crate::Localization) is a long-lived structure,
//! the model in which the user provides ability for the system to react to changes
//! is by implementing the given environmental trait and triggering
//! [`Localization::on_change`](crate::Localization::on_change) method.
//!
//! At the moment just a single trait is provided, which allows the
//! environment to feed a selection of locales to be provided to the instance.
//!
//! The locales provided to [`Localization`](crate::Localization) should be
//! already negotiated to ensure that the resources in those locales
//! are available. The list should also be sorted according to the user
//! preference, as the order is significant for how [`Localization`](crate::Localization) performs
//! fallbacking.
use unic_langid::LanguageIdentifier;

/// A trait used to provide a selection of locales to be used by the
/// [`Localization`](crate::Localization) instance for runtime
/// locale fallbacking.
///
/// # Example
/// ```
/// use fluent_fallback::{Localization, env::LocalesProvider};
/// use fluent_resmgr::ResourceManager;
/// use unic_langid::LanguageIdentifier;
/// use std::{
///     rc::Rc,
///     cell::RefCell
/// };
///
/// #[derive(Clone)]
/// struct Env {
///     locales: Rc<RefCell<Vec<LanguageIdentifier>>>,
/// }
///
/// impl Env {
///     pub fn new(locales: Vec<LanguageIdentifier>) -> Self {
///         Self { locales: Rc::new(RefCell::new(locales)) }
///     }
///
///     pub fn set_locales(&mut self, new_locales: Vec<LanguageIdentifier>) {
///         let mut locales = self.locales.borrow_mut();
///         locales.clear();
///         locales.extend(new_locales);
///     }
/// }
///
/// impl LocalesProvider for Env {
///     type Iter = <Vec<LanguageIdentifier> as IntoIterator>::IntoIter;
///     fn locales(&self) -> Self::Iter {
///         self.locales.borrow().clone().into_iter()
///     }
/// }
///
/// let res_mgr = ResourceManager::new("./path/{locale}/".to_string());
///
/// let mut env = Env::new(vec![
///     "en-GB".parse().unwrap()
/// ]);
///
/// let mut loc = Localization::with_env(vec![], true, env.clone(), res_mgr);
///
/// env.set_locales(vec![
///     "de".parse().unwrap(),
///     "en-GB".parse().unwrap(),
/// ]);
///
/// loc.on_change();
///
/// // The next format call will attempt to localize to `de` first and
/// // fallback on `en-GB`.
/// ```
pub trait LocalesProvider {
    type Iter: Iterator<Item = LanguageIdentifier>;
    fn locales(&self) -> Self::Iter;
}

impl LocalesProvider for Vec<LanguageIdentifier> {
    type Iter = <Vec<LanguageIdentifier> as IntoIterator>::IntoIter;
    fn locales(&self) -> Self::Iter {
        self.clone().into_iter()
    }
}

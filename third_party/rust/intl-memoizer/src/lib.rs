use std::cell::RefCell;
use std::collections::hash_map::Entry;
use std::collections::HashMap;
use std::hash::Hash;
use std::rc::{Rc, Weak};
use unic_langid::LanguageIdentifier;

pub mod concurrent;

pub trait Memoizable {
    type Args: 'static + Eq + Hash + Clone;
    type Error;
    fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error>
    where
        Self: std::marker::Sized;
}

#[derive(Debug)]
pub struct IntlLangMemoizer {
    lang: LanguageIdentifier,
    map: RefCell<type_map::TypeMap>,
}

impl IntlLangMemoizer {
    pub fn new(lang: LanguageIdentifier) -> Self {
        Self {
            lang,
            map: RefCell::new(type_map::TypeMap::new()),
        }
    }

    pub fn with_try_get<I, R, U>(&self, args: I::Args, cb: U) -> Result<R, I::Error>
    where
        Self: Sized,
        I: Memoizable + 'static,
        U: FnOnce(&I) -> R,
    {
        let mut map = self
            .map
            .try_borrow_mut()
            .expect("Cannot use memoizer reentrantly");
        let cache = map
            .entry::<HashMap<I::Args, I>>()
            .or_insert_with(HashMap::new);

        let e = match cache.entry(args.clone()) {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => {
                let val = I::construct(self.lang.clone(), args)?;
                entry.insert(val)
            }
        };
        Ok(cb(&e))
    }
}

#[derive(Default)]
pub struct IntlMemoizer {
    map: HashMap<LanguageIdentifier, Weak<IntlLangMemoizer>>,
}

impl IntlMemoizer {
    pub fn get_for_lang(&mut self, lang: LanguageIdentifier) -> Rc<IntlLangMemoizer> {
        match self.map.entry(lang.clone()) {
            Entry::Vacant(empty) => {
                let entry = Rc::new(IntlLangMemoizer::new(lang));
                empty.insert(Rc::downgrade(&entry));
                entry
            }
            Entry::Occupied(mut entry) => {
                if let Some(entry) = entry.get().upgrade() {
                    entry
                } else {
                    let e = Rc::new(IntlLangMemoizer::new(lang));
                    entry.insert(Rc::downgrade(&e));
                    e
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use fluent_langneg::{negotiate_languages, NegotiationStrategy};
    use intl_pluralrules::{PluralCategory, PluralRuleType, PluralRules as IntlPluralRules};

    struct PluralRules(pub IntlPluralRules);

    impl PluralRules {
        pub fn new(
            lang: LanguageIdentifier,
            pr_type: PluralRuleType,
        ) -> Result<Self, &'static str> {
            let default_lang: LanguageIdentifier = "en".parse().unwrap();
            let pr_lang = negotiate_languages(
                &[lang],
                &IntlPluralRules::get_locales(pr_type),
                Some(&default_lang),
                NegotiationStrategy::Lookup,
            )[0]
            .clone();

            Ok(Self(IntlPluralRules::create(pr_lang, pr_type)?))
        }
    }

    impl Memoizable for PluralRules {
        type Args = (PluralRuleType,);
        type Error = &'static str;
        fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error> {
            Self::new(lang, args.0)
        }
    }

    #[test]
    fn it_works() {
        let lang: LanguageIdentifier = "en".parse().unwrap();

        let mut memoizer = IntlMemoizer::default();
        {
            let en_memoizer = memoizer.get_for_lang(lang.clone());

            let result = en_memoizer
                .with_try_get::<PluralRules, _, _>((PluralRuleType::CARDINAL,), |cb| cb.0.select(5))
                .unwrap();
            assert_eq!(result, Ok(PluralCategory::OTHER));
        }

        {
            let en_memoizer = memoizer.get_for_lang(lang.clone());

            let result = en_memoizer
                .with_try_get::<PluralRules, _, _>((PluralRuleType::CARDINAL,), |cb| cb.0.select(5))
                .unwrap();
            assert_eq!(result, Ok(PluralCategory::OTHER));
        }
    }
}

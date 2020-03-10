use super::*;
use std::sync::Mutex;

#[derive(Debug)]
pub struct IntlLangMemoizer {
    lang: LanguageIdentifier,
    map: Mutex<type_map::concurrent::TypeMap>,
}

impl IntlLangMemoizer {
    pub fn new(lang: LanguageIdentifier) -> Self {
        Self {
            lang,
            map: Mutex::new(type_map::concurrent::TypeMap::new()),
        }
    }

    pub fn with_try_get<I, R, U>(&self, args: I::Args, cb: U) -> Result<R, I::Error>
    where
        Self: Sized,
        I: Memoizable + Sync + Send + 'static,
        I::Args: Send + Sync + 'static,
        U: FnOnce(&I) -> R,
    {
        let mut map = self.map.lock().unwrap();
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

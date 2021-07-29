mod fetcher;
pub use fetcher::FileFetcher;

use crate::env::ErrorReporter;
use crate::errors::L10nRegistryError;
use crate::fluent::FluentResource;

use std::{
    borrow::Borrow,
    cell::RefCell,
    fmt,
    hash::{Hash, Hasher},
    pin::Pin,
    rc::Rc,
    task::Poll,
};

use futures::{future::Shared, Future, FutureExt};
use rustc_hash::FxHashMap;
use unic_langid::LanguageIdentifier;

pub type RcResource = Rc<FluentResource>;
pub type ResourceOption = Option<RcResource>;
pub type ResourceFuture = Shared<Pin<Box<dyn Future<Output = ResourceOption>>>>;

#[derive(Debug, Clone)]
pub enum ResourceStatus {
    /// The resource is missing.  Don't bother trying to fetch.
    Missing,
    /// The resource is loading and future will deliver the result.
    Loading(ResourceFuture),
    /// The resource is loaded and parsed.
    Loaded(RcResource),
}

impl From<ResourceOption> for ResourceStatus {
    fn from(input: ResourceOption) -> Self {
        if let Some(res) = input {
            Self::Loaded(res)
        } else {
            Self::Missing
        }
    }
}

impl Future for ResourceStatus {
    type Output = ResourceOption;

    fn poll(mut self: Pin<&mut Self>, cx: &mut std::task::Context<'_>) -> Poll<Self::Output> {
        use ResourceStatus::*;

        let this = &mut *self;

        match this {
            Missing => None.into(),
            Loaded(res) => Some(res.clone()).into(),
            Loading(res) => Pin::new(res).poll(cx),
        }
    }
}

/// `FileSource` provides a generic fetching and caching of fluent resources.
/// The user of `FileSource` provides a [`FileFetcher`](trait.FileFetcher.html)
/// implementation and `FileSource` takes care of the rest.
#[derive(Clone)]
pub struct FileSource {
    pub name: String,
    pub pre_path: String,
    locales: Vec<LanguageIdentifier>,
    shared: Rc<Inner>,
    index: Option<Vec<String>>,
    pub options: FileSourceOptions,
}

struct Inner {
    fetcher: Box<dyn FileFetcher>,
    error_reporter: Option<RefCell<Box<dyn ErrorReporter>>>,
    entries: RefCell<FxHashMap<String, ResourceStatus>>,
}

impl fmt::Display for FileSource {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name)
    }
}

impl PartialEq<FileSource> for FileSource {
    fn eq(&self, other: &Self) -> bool {
        self.name == other.name
    }
}

impl Eq for FileSource {}

impl Hash for FileSource {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.name.hash(state)
    }
}

#[derive(PartialEq, Clone, Debug)]
pub struct FileSourceOptions {
    pub allow_override: bool,
}

impl Default for FileSourceOptions {
    fn default() -> Self {
        Self {
            allow_override: false,
        }
    }
}

impl FileSource {
    /// Create a `FileSource` using the provided [`FileFetcher`](../trait.FileFetcher.html).
    pub fn new(
        name: String,
        locales: Vec<LanguageIdentifier>,
        pre_path: String,
        options: FileSourceOptions,
        fetcher: impl FileFetcher + 'static,
    ) -> Self {
        FileSource {
            name,
            pre_path,
            locales,
            index: None,
            shared: Rc::new(Inner {
                entries: RefCell::new(FxHashMap::default()),
                fetcher: Box::new(fetcher),
                error_reporter: None,
            }),
            options,
        }
    }

    pub fn new_with_index(
        name: String,
        locales: Vec<LanguageIdentifier>,
        pre_path: String,
        options: FileSourceOptions,
        fetcher: impl FileFetcher + 'static,
        index: Vec<String>,
    ) -> Self {
        FileSource {
            name,
            pre_path,
            locales,
            index: Some(index),
            shared: Rc::new(Inner {
                entries: RefCell::new(FxHashMap::default()),
                fetcher: Box::new(fetcher),
                error_reporter: None,
            }),
            options,
        }
    }

    pub fn set_reporter(&mut self, reporter: impl ErrorReporter + 'static) {
        let mut shared = Rc::get_mut(&mut self.shared).unwrap();
        shared.error_reporter = Some(RefCell::new(Box::new(reporter)));
    }
}

fn calculate_pos_in_source(source: &str, idx: usize) -> (usize, usize) {
    let mut ptr = 0;
    let mut result = (1, 1);
    for line in source.lines() {
        let bytes = line.as_bytes().len();
        if ptr + bytes < idx {
            ptr += bytes + 1;
            result.0 += 1;
        } else {
            result.1 = idx - ptr + 1;
            break;
        }
    }
    result
}

impl FileSource {
    fn get_path(&self, locale: &LanguageIdentifier, path: &str) -> String {
        format!(
            "{}{}",
            self.pre_path.replace("{locale}", &locale.to_string()),
            path
        )
    }

    fn fetch_sync(&self, full_path: &str) -> ResourceOption {
        self.shared
            .fetcher
            .fetch_sync(full_path)
            .ok()
            .map(|source| match FluentResource::try_new(source) {
                Ok(res) => Rc::new(res),
                Err((res, errors)) => {
                    if let Some(reporter) = &self.shared.error_reporter {
                        reporter.borrow().report_errors(
                            errors
                                .into_iter()
                                .map(|e| L10nRegistryError::FluentError {
                                    path: full_path.to_string(),
                                    loc: Some(calculate_pos_in_source(res.source(), e.pos.start)),
                                    error: e.into(),
                                })
                                .collect(),
                        );
                    }
                    Rc::new(res)
                }
            })
    }

    /// Attempt to synchronously fetch resource for the combination of `locale`
    /// and `path`. Returns `Some(ResourceResult)` if the resource is available,
    /// else `None`.
    pub fn fetch_file_sync(
        &self,
        locale: &LanguageIdentifier,
        path: &str,
        overload: bool,
    ) -> ResourceOption {
        use ResourceStatus::*;

        if self.has_file(locale, path) == Some(false) {
            return None;
        }

        let full_path = self.get_path(locale, &path);

        let res = self
            .shared
            .lookup_resource(full_path.clone(), || self.fetch_sync(&full_path).into());

        match res {
            Missing => None,
            Loaded(res) => Some(res),
            Loading(..) if overload => {
                // A sync load has been requested for the same resource that has
                // a pending async load in progress. How do we handle this?
                //
                // Ideally, we would sync load and resolve all the pending
                // futures with the result. With the current Futures and
                // combinators, it's unclear how to proceed. One potential
                // solution is to store a oneshot::Sender and
                // Shared<oneshot::Receiver>. When the async loading future
                // resolves it would check that the state is still `Loading`,
                // and if so, send the result. The sync load would do the same
                // send on the oneshot::Sender.
                //
                // For now, we warn and return the resource, paying the cost of
                // duplication of the resource.
                self.fetch_sync(&full_path)
            }
            Loading(..) => {
                panic!("[l10nregistry] Attempting to synchronously load file {} while it's being loaded asynchronously.", &full_path);
            }
        }
    }

    /// Attempt to fetch resource for the combination of `locale` and `path`.
    /// Returns [`ResourceStatus`](enum.ResourceStatus.html) which is
    /// a `Future` that can be polled.
    pub fn fetch_file(&self, locale: &LanguageIdentifier, path: &str) -> ResourceStatus {
        use ResourceStatus::*;

        if self.has_file(locale, path) == Some(false) {
            return ResourceStatus::Missing;
        }

        let full_path = self.get_path(locale, path);

        self.shared.lookup_resource(full_path.clone(), || {
            let shared = self.shared.clone();
            Loading(read_resource(full_path, shared).boxed_local().shared())
        })
    }

    /// Determine if the `FileSource` has a loaded resource for the combination
    /// of `locale` and `path`. Returns `Some(true)` if the file is loaded, else
    /// `Some(false)`. `None` is returned if there is an outstanding async fetch
    /// pending and the status is yet to be determined.
    pub fn has_file<L: Borrow<LanguageIdentifier>>(&self, locale: L, path: &str) -> Option<bool> {
        let locale = locale.borrow();
        if !self.locales.contains(locale) {
            Some(false)
        } else {
            let full_path = self.get_path(locale, path);
            if let Some(index) = &self.index {
                return Some(index.iter().any(|p| p == &full_path));
            }
            self.shared.has_file(&full_path)
        }
    }

    pub fn locales(&self) -> &[LanguageIdentifier] {
        &self.locales
    }

    pub fn get_index(&self) -> Option<&Vec<String>> {
        self.index.as_ref()
    }
}

impl std::fmt::Debug for FileSource {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        if let Some(index) = &self.index {
            f.debug_struct("FileSource")
                .field("name", &self.name)
                .field("locales", &self.locales)
                .field("pre_path", &self.pre_path)
                .field("index", index)
                .finish()
        } else {
            f.debug_struct("FileSource")
                .field("name", &self.name)
                .field("locales", &self.locales)
                .field("pre_path", &self.pre_path)
                .finish()
        }
    }
}

impl Inner {
    fn lookup_resource<F>(&self, path: String, f: F) -> ResourceStatus
    where
        F: FnOnce() -> ResourceStatus,
    {
        let mut lock = self.entries.borrow_mut();
        lock.entry(path).or_insert_with(|| f()).clone()
    }

    fn update_resource(&self, path: String, resource: ResourceOption) -> ResourceOption {
        let mut lock = self.entries.borrow_mut();
        let entry = lock.get_mut(&path);
        match entry {
            Some(entry) => *entry = resource.clone().into(),
            _ => panic!("Expected "),
        }
        resource
    }

    pub fn has_file(&self, full_path: &str) -> Option<bool> {
        match self.entries.borrow().get(full_path) {
            Some(ResourceStatus::Missing) => Some(false),
            Some(ResourceStatus::Loaded(_)) => Some(true),
            Some(ResourceStatus::Loading(_)) | None => None,
        }
    }
}

async fn read_resource(path: String, shared: Rc<Inner>) -> ResourceOption {
    let resource =
        shared.fetcher.fetch(&path).await.ok().map(|source| {
            match FluentResource::try_new(source) {
                Ok(res) => Rc::new(res),
                Err((res, errors)) => {
                    if let Some(reporter) = &shared.error_reporter.borrow() {
                        reporter.borrow().report_errors(
                            errors
                                .into_iter()
                                .map(|e| L10nRegistryError::FluentError {
                                    path: path.clone(),
                                    loc: Some(calculate_pos_in_source(res.source(), e.pos.start)),
                                    error: e.into(),
                                })
                                .collect(),
                        );
                    }
                    Rc::new(res)
                }
            }
        });
    // insert the resource into the cache
    shared.update_resource(path, resource)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn calculate_source_pos() {
        let source = r#"
key = Value

key2 = Value 2
"#
        .trim();
        let result = calculate_pos_in_source(source, 0);
        assert_eq!(result, (1, 1));

        let result = calculate_pos_in_source(source, 1);
        assert_eq!(result, (1, 2));

        let result = calculate_pos_in_source(source, 12);
        assert_eq!(result, (2, 1));

        let result = calculate_pos_in_source(source, 13);
        assert_eq!(result, (3, 1));
    }
}

#[cfg(test)]
#[cfg(feature = "tokio")]
mod tests_tokio {
    use super::*;
    use crate::testing::TestFileFetcher;

    const FTL_RESOURCE_PRESENT: &str = "toolkit/global/textActions.ftl";
    const FTL_RESOURCE_MISSING: &str = "missing.ftl";

    #[tokio::test]
    async fn file_source_fetch() {
        let fetcher = TestFileFetcher::new();
        let en_us: LanguageIdentifier = "en-US".parse().unwrap();
        let fs1 = fetcher.get_test_file_source("toolkit", vec![en_us.clone()], "toolkit/{locale}/");

        let file = fs1.fetch_file(&en_us, FTL_RESOURCE_PRESENT).await;
        assert!(file.is_some());
    }

    #[tokio::test]
    async fn file_source_fetch_missing() {
        let fetcher = TestFileFetcher::new();
        let en_us: LanguageIdentifier = "en-US".parse().unwrap();
        let fs1 = fetcher.get_test_file_source("toolkit", vec![en_us.clone()], "toolkit/{locale}/");

        let file = fs1.fetch_file(&en_us, FTL_RESOURCE_MISSING).await;
        assert!(file.is_none());
    }

    #[tokio::test]
    async fn file_source_already_loaded() {
        let fetcher = TestFileFetcher::new();
        let en_us: LanguageIdentifier = "en-US".parse().unwrap();
        let fs1 = fetcher.get_test_file_source("toolkit", vec![en_us.clone()], "toolkit/{locale}/");

        let file = fs1.fetch_file(&en_us, FTL_RESOURCE_PRESENT).await;
        assert!(file.is_some());
        let file = fs1.fetch_file(&en_us, FTL_RESOURCE_PRESENT).await;
        assert!(file.is_some());
    }

    #[tokio::test]
    async fn file_source_concurrent() {
        let fetcher = TestFileFetcher::new();
        let en_us: LanguageIdentifier = "en-US".parse().unwrap();
        let fs1 = fetcher.get_test_file_source("toolkit", vec![en_us.clone()], "toolkit/{locale}/");

        let file1 = fs1.fetch_file(&en_us, FTL_RESOURCE_PRESENT);
        let file2 = fs1.fetch_file(&en_us, FTL_RESOURCE_PRESENT);
        assert!(file1.await.is_some());
        assert!(file2.await.is_some());
    }

    #[test]
    fn file_source_sync_after_async_fail() {
        let fetcher = TestFileFetcher::new();
        let en_us: LanguageIdentifier = "en-US".parse().unwrap();
        let fs1 = fetcher.get_test_file_source("toolkit", vec![en_us.clone()], "toolkit/{locale}/");

        let _ = fs1.fetch_file(&en_us, FTL_RESOURCE_PRESENT);
        let file2 = fs1.fetch_file_sync(&en_us, FTL_RESOURCE_PRESENT, true);
        assert!(file2.is_some());
    }
}

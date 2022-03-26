use fluent_fallback::types::ResourceId;

pub struct FileSource {
    pub name: String,
    pub locales: Vec<String>,
    pub path_scheme: String,
}

impl Default for FileSource {
    fn default() -> Self {
        Self {
            name: "default".to_string(),
            path_scheme: "{locale}/".to_string(),
            locales: vec!["en-US".to_string()],
        }
    }
}

impl FileSource {
    pub fn new<S: ToString>(name: S, path_scheme: S, locales: Vec<S>) -> Self {
        Self {
            name: name.to_string(),
            path_scheme: path_scheme.to_string(),
            locales: locales
                .iter()
                .map(|l| l.to_string().parse().unwrap())
                .collect(),
        }
    }
}

#[derive(Debug)]
pub struct L10nAttribute {
    pub name: String,
    pub value: String,
}

impl L10nAttribute {
    pub fn new<S: ToString>(name: S, value: S) -> Self {
        Self {
            name: name.to_string(),
            value: value.to_string(),
        }
    }
}

#[derive(Debug)]
pub struct L10nMessage {
    pub value: Option<String>,
    pub attributes: Option<Vec<L10nAttribute>>,
}

impl L10nMessage {
    pub fn new(value: Option<&str>, attributes: Option<Vec<L10nAttribute>>) -> Self {
        Self {
            value: value.map(|v| v.to_string()),
            attributes,
        }
    }
}

impl From<&str> for L10nMessage {
    fn from(value: &str) -> Self {
        Self {
            value: Some(value.to_string()),
            attributes: None,
        }
    }
}

#[derive(Debug)]
pub struct L10nArgument {
    pub id: String,
    pub value: String,
}

impl L10nArgument {
    pub fn new<S: ToString>(id: S, value: S) -> Self {
        Self {
            id: id.to_string(),
            value: value.to_string(),
        }
    }
}

#[derive(Debug)]
pub struct L10nKey {
    pub id: String,
    pub args: Option<Vec<L10nArgument>>,
}

impl L10nKey {
    pub fn new<S: ToString>(id: S, args: Option<Vec<L10nArgument>>) -> Self {
        Self {
            id: id.to_string(),
            args,
        }
    }
}

impl From<&str> for L10nKey {
    fn from(input: &str) -> Self {
        Self {
            id: input.to_string(),
            args: None,
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub enum ExceptionalContext {
    /// There is no exceptional context for this query (happy path).
    None,
    /// A value is missing from a resource and should cause a fallback.
    ValueMissingFromResource,
    /// A value is missing from all resources in all locales.
    ValueMissingFromAllResources,
    /// An optional resource is missing from the top locale.
    OptionalResourceMissingFromLocale,
    /// An optional resource is missing from all locales.
    OptionalResourceMissingFromAllLocales,
    /// A required resource is missing from the top locale.
    RequiredResourceMissingFromLocale,
    /// A required resource is missing from all locales.
    RequiredResourceMissingFromAllLocales,
}

impl ExceptionalContext {
    /// This is a query for a value in a missing required resource.
    pub fn missing_required_resource(self) -> bool {
        matches!(
            self,
            Self::RequiredResourceMissingFromLocale | Self::RequiredResourceMissingFromAllLocales,
        )
    }

    /// This query should cause a format error to be appended to the errors Vec.
    pub fn causes_reported_format_error(self) -> bool {
        matches!(
            self,
            Self::ValueMissingFromResource
                | Self::ValueMissingFromAllResources
                | Self::OptionalResourceMissingFromLocale
                | Self::OptionalResourceMissingFromAllLocales
                | Self::RequiredResourceMissingFromAllLocales,
        )
    }

    /// This query should cause a failed value lookup.
    pub fn causes_failed_value_lookup(self) -> bool {
        matches!(
            self,
            Self::ValueMissingFromAllResources
                | Self::OptionalResourceMissingFromAllLocales
                | Self::RequiredResourceMissingFromAllLocales,
        )
    }

    /// This query should result in no bundles being generated.
    pub fn blocks_bundle_generation(self) -> bool {
        matches!(self, Self::RequiredResourceMissingFromAllLocales,)
    }
}

#[derive(Debug)]
pub struct Query {
    pub input: L10nKey,
    pub output: Option<L10nMessage>,
    pub exceptional_context: ExceptionalContext,
}

impl Query {
    pub fn new<K: Into<L10nKey>>(input: K, output: Option<L10nMessage>) -> Self {
        Self {
            input: input.into(),
            output,
            exceptional_context: ExceptionalContext::None,
        }
    }
}

impl From<(&str, &str)> for Query {
    fn from(i: (&str, &str)) -> Self {
        Self {
            input: i.0.into(),
            output: Some(i.1.into()),
            exceptional_context: ExceptionalContext::None,
        }
    }
}

impl From<(&str, &str, ExceptionalContext)> for Query {
    fn from(i: (&str, &str, ExceptionalContext)) -> Self {
        Self {
            input: i.0.into(),
            output: Some(i.1.into()),
            exceptional_context: i.2,
        }
    }
}

impl From<(&str, L10nMessage)> for Query {
    fn from(i: (&str, L10nMessage)) -> Self {
        Self {
            input: i.0.into(),
            output: Some(i.1),
            exceptional_context: ExceptionalContext::None,
        }
    }
}

impl From<(L10nKey, L10nMessage)> for Query {
    fn from(i: (L10nKey, L10nMessage)) -> Self {
        Self {
            input: i.0,
            output: Some(i.1),
            exceptional_context: ExceptionalContext::None,
        }
    }
}

impl From<&str> for Query {
    fn from(i: &str) -> Self {
        Self {
            input: i.into(),
            output: None,
            exceptional_context: ExceptionalContext::None,
        }
    }
}

impl From<L10nKey> for Query {
    fn from(key: L10nKey) -> Self {
        Self {
            input: key,
            output: None,
            exceptional_context: ExceptionalContext::None,
        }
    }
}

pub struct Queries(pub Vec<Query>);

impl From<Vec<&str>> for Queries {
    fn from(input: Vec<&str>) -> Self {
        Self(input.into_iter().map(|q| q.into()).collect())
    }
}

impl From<Vec<(&str, &str)>> for Queries {
    fn from(input: Vec<(&str, &str)>) -> Self {
        Self(input.into_iter().map(|q| q.into()).collect())
    }
}

impl std::ops::Deref for Queries {
    type Target = Vec<Query>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

pub struct Scenario {
    pub name: String,
    pub file_sources: Vec<FileSource>,
    pub locales: Vec<String>,
    pub res_ids: Vec<ResourceId>,
    pub queries: Queries,
}

impl Scenario {
    pub fn new<S: ToString, R: Into<ResourceId>, Q: Into<Queries>>(
        name: S,
        file_sources: Vec<FileSource>,
        locales: Vec<S>,
        res_ids: Vec<R>,
        queries: Q,
    ) -> Self {
        Self {
            name: name.to_string(),
            file_sources,
            locales: locales
                .into_iter()
                .map(|l| l.to_string().parse().unwrap())
                .collect(),
            res_ids: res_ids.into_iter().map(|id| id.into()).collect(),
            queries: queries.into(),
        }
    }
}

use fluent_bundle::FluentArgs;
use std::borrow::Cow;

#[derive(Debug)]
pub struct L10nKey<'l> {
    pub id: Cow<'l, str>,
    pub args: Option<FluentArgs<'l>>,
}

impl<'l> From<&'l str> for L10nKey<'l> {
    fn from(id: &'l str) -> Self {
        Self {
            id: id.into(),
            args: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct L10nAttribute<'l> {
    pub name: Cow<'l, str>,
    pub value: Cow<'l, str>,
}

#[derive(Debug, Clone)]
pub struct L10nMessage<'l> {
    pub value: Option<Cow<'l, str>>,
    pub attributes: Vec<L10nAttribute<'l>>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ResourceType {
    /// This is a required resource.
    ///
    /// A bundle generator should not consider a solution as valid
    /// if this resource is missing.
    ///
    /// This is the default when creating a [`ResourceId`].
    Required,

    /// This is an optional resource.
    ///
    /// A bundle generator should still populate a partial solution
    /// even if this resource is missing.
    ///
    /// This is intended for experimental and/or under-development
    /// resources that may not have content for all supported locales.
    ///
    /// This should be used sparingly, as it will greatly increase
    /// the state space of the search for valid solutions which can
    /// have a severe impact on performance.
    Optional,
}

/// A resource identifier for a localization resource.
#[derive(Debug, Clone, Hash)]
pub struct ResourceId {
    /// The resource identifier.
    pub value: String,

    /// The [`ResourceType`] for this resource.
    ///
    /// The default value (when converting from another type) is
    /// [`ResourceType::Required`]. You should only set this to
    /// [`ResourceType::Optional`] for experimental or under-development
    /// features that may not yet have content in all eventually-supported locales.
    ///
    /// Setting this value to [`ResourceType::Optional`] for all resources
    /// may have a severe impact on performance due to increasing the state space
    /// of the solver.
    pub resource_type: ResourceType,
}

impl ResourceId {
    pub fn new<S: Into<String>>(value: S, resource_type: ResourceType) -> Self {
        Self {
            value: value.into(),
            resource_type,
        }
    }

    /// Returns [`true`] if the resource has [`ResourceType::Required`],
    /// otherwise returns [`false`].
    pub fn is_required(&self) -> bool {
        matches!(self.resource_type, ResourceType::Required)
    }

    /// Returns [`true`] if the resource has [`ResourceType::Optional`],
    /// otherwise returns [`false`].
    pub fn is_optional(&self) -> bool {
        matches!(self.resource_type, ResourceType::Optional)
    }
}

impl<S: Into<String>> From<S> for ResourceId {
    fn from(id: S) -> Self {
        Self {
            value: id.into(),
            resource_type: ResourceType::Required,
        }
    }
}

impl std::fmt::Display for ResourceId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.value)
    }
}

impl PartialEq<str> for ResourceId {
    fn eq(&self, other: &str) -> bool {
        self.value.as_str().eq(other)
    }
}

impl Eq for ResourceId {}
impl PartialEq for ResourceId {
    fn eq(&self, other: &Self) -> bool {
        self.value.eq(&other.value)
    }
}

/// A trait for creating a [`ResourceId`] from another type.
///
/// This differs from the [`From`] trait in that the [`From`] trait
/// always takes the default resource type of [`ResourceType::Required`].
///
/// If you need to create a resource with a non-default [`ResourceType`],
/// such as [`ResourceType::Optional`], then use this trait.
///
/// This trait is automatically implemented for types that implement [`Into<String>`].
pub trait ToResourceId {
    /// Creates a [`ResourceId`] from [`self`], given a [`ResourceType`].
    fn to_resource_id(self, resource_type: ResourceType) -> ResourceId;
}

impl<S: Into<String>> ToResourceId for S {
    fn to_resource_id(self, resource_type: ResourceType) -> ResourceId {
        ResourceId::new(self.into(), resource_type)
    }
}

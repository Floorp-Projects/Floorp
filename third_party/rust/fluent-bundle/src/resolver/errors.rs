#[derive(Debug, PartialEq, Clone)]
pub enum ResolverError {
    Reference(String),
    MissingDefault,
    Cyclic,
    TooManyPlaceables,
}

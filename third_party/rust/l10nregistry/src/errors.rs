use fluent_bundle::FluentError;
use std::error::Error;
use unic_langid::LanguageIdentifier;

#[derive(Debug, Clone, PartialEq)]
pub enum L10nRegistryError {
    FluentError {
        path: String,
        loc: Option<(usize, usize)>,
        error: FluentError,
    },
    MissingResource {
        locale: LanguageIdentifier,
        res_id: String,
    },
}

impl std::fmt::Display for L10nRegistryError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::MissingResource { locale, res_id } => {
                write!(f, "Missing resource in locale {}: {}", locale, res_id)
            }
            Self::FluentError { path, loc, error } => {
                if let Some(loc) = loc {
                    write!(
                        f,
                        "Fluent Error in {}[line: {}, col: {}]: {}",
                        path, loc.0, loc.1, error
                    )
                } else {
                    write!(f, "Fluent Error in {}: {}", path, error)
                }
            }
        }
    }
}

impl Error for L10nRegistryError {}

#[derive(Debug, Clone, PartialEq)]
pub enum L10nRegistrySetupError {
    RegistryLocked,
    DuplicatedSource { name: String },
    MissingSource { name: String },
}

impl std::fmt::Display for L10nRegistrySetupError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::RegistryLocked => write!(f, "Can't modify a registry when locked."),
            Self::DuplicatedSource { name } => {
                write!(f, "Source with a name {} is already registered.", &name)
            }
            Self::MissingSource { name } => {
                write!(f, "Cannot find a source with a name {}.", &name)
            }
        }
    }
}

impl Error for L10nRegistrySetupError {}

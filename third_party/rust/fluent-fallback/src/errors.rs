use fluent_bundle::FluentError;
use std::error::Error;
use unic_langid::LanguageIdentifier;

#[derive(Debug, PartialEq)]
pub enum LocalizationError {
    Bundle {
        error: FluentError,
    },
    Resolver {
        id: String,
        locale: LanguageIdentifier,
        errors: Vec<FluentError>,
    },
    MissingMessage {
        id: String,
        locale: Option<LanguageIdentifier>,
    },
    MissingValue {
        id: String,
        locale: Option<LanguageIdentifier>,
    },
    SyncRequestInAsyncMode,
}

impl From<FluentError> for LocalizationError {
    fn from(error: FluentError) -> Self {
        Self::Bundle { error }
    }
}

impl std::fmt::Display for LocalizationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Bundle { error } => write!(f, "[fluent][bundle] error: {}", error),
            Self::Resolver { id, locale, errors } => {
                let errors: Vec<String> = errors.iter().map(|err| err.to_string()).collect();
                write!(
                    f,
                    "[fluent][resolver] errors in {}/{}: {}",
                    locale,
                    id,
                    errors.join(", ")
                )
            }
            Self::MissingMessage {
                id,
                locale: Some(locale),
            } => write!(f, "[fluent] Missing message in locale {}: {}", locale, id),
            Self::MissingMessage { id, locale: None } => {
                write!(f, "[fluent] Couldn't find a message: {}", id)
            }
            Self::MissingValue {
                id,
                locale: Some(locale),
            } => write!(
                f,
                "[fluent] Message has no value in locale {}: {}",
                locale, id
            ),
            Self::MissingValue { id, locale: None } => {
                write!(f, "[fluent] Couldn't find a message with value: {}", id)
            }
            Self::SyncRequestInAsyncMode => {
                write!(f, "Triggered synchronous format while in async mode")
            }
        }
    }
}

impl Error for LocalizationError {}

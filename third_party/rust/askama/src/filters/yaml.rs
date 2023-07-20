use crate::error::{Error, Result};
use askama_escape::{Escaper, MarkupDisplay};
use serde::Serialize;

/// Serialize to YAML (requires `serde_yaml` feature)
///
/// ## Errors
///
/// This will panic if `S`'s implementation of `Serialize` decides to fail,
/// or if `T` contains a map with non-string keys.
pub fn yaml<E: Escaper, S: Serialize>(e: E, s: S) -> Result<MarkupDisplay<E, String>> {
    match serde_yaml::to_string(&s) {
        Ok(s) => Ok(MarkupDisplay::new_safe(s, e)),
        Err(e) => Err(Error::from(e)),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use askama_escape::Html;

    #[test]
    fn test_yaml() {
        assert_eq!(yaml(Html, true).unwrap().to_string(), "true\n");
        assert_eq!(yaml(Html, "foo").unwrap().to_string(), "foo\n");
        assert_eq!(yaml(Html, true).unwrap().to_string(), "true\n");
        assert_eq!(yaml(Html, "foo").unwrap().to_string(), "foo\n");
        assert_eq!(
            yaml(Html, &vec!["foo", "bar"]).unwrap().to_string(),
            "- foo\n- bar\n"
        );
    }
}

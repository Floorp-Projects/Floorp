#![allow(unknown_lints)]
use std::{env, ffi::OsStr};

const LC_ALL: &str = "LC_ALL";
const LC_CTYPE: &str = "LC_CTYPE";
const LANG: &str = "LANG";

/// Environment variable access abstraction to allow testing without
/// mutating env variables.
///
/// Use [StdEnv] to query [std::env]
trait EnvAccess {
    /// See also [std::env::var]
    fn get(&self, key: impl AsRef<OsStr>) -> Option<String>;
}

/// Proxy to [std::env]
struct StdEnv;
impl EnvAccess for StdEnv {
    fn get(&self, key: impl AsRef<OsStr>) -> Option<String> {
        env::var(key).ok()
    }
}

pub(crate) fn get() -> impl Iterator<Item = String> {
    _get(&StdEnv).into_iter()
}

fn _get(env: &impl EnvAccess) -> Option<String> {
    let code = env
        .get(LC_ALL)
        .or_else(|| env.get(LC_CTYPE))
        .or_else(|| env.get(LANG))?;

    parse_locale_code(&code)
}

fn parse_locale_code(code: &str) -> Option<String> {
    // Some locales are returned with the char encoding too: `en_US.UTF-8`
    // TODO: Once we bump MSRV >= 1.52, remove this allow and clean up
    #[allow(clippy::manual_split_once)]
    #[allow(clippy::needless_splitn)]
    code.splitn(2, '.').next().map(|s| s.replace('_', "-"))
}

#[cfg(test)]
mod tests {
    use super::{parse_locale_code, EnvAccess, _get, LANG, LC_ALL, LC_CTYPE};
    use std::{
        collections::HashMap,
        ffi::{OsStr, OsString},
    };

    type MockEnv = HashMap<OsString, String>;
    impl EnvAccess for MockEnv {
        fn get(&self, key: impl AsRef<OsStr>) -> Option<String> {
            self.get(key.as_ref()).cloned()
        }
    }

    const PARSE_LOCALE: &str = "fr-FR";
    const LANG_PARSE_LOCALE: &str = "fr_FR";

    #[test]
    fn parse_identifier() {
        let identifier = "fr_FR.UTF-8";
        assert_eq!(parse_locale_code(identifier).as_deref(), Some(PARSE_LOCALE));
    }

    #[test]
    fn parse_non_suffixed_identifier() {
        assert_eq!(
            parse_locale_code(PARSE_LOCALE).as_deref(),
            Some(PARSE_LOCALE)
        );

        assert_eq!(
            parse_locale_code(LANG_PARSE_LOCALE).as_deref(),
            Some(PARSE_LOCALE)
        );
    }

    #[test]
    fn env_priority() {
        let mut env = MockEnv::new();
        assert_eq!(_get(&env), None);

        // These locale names are technically allowed and some systems may still
        // defined aliases such as these but the glibc sources mention that this
        // should be considered deprecated

        env.insert(LANG.into(), "invalid".to_owned());
        assert_eq!(_get(&env).as_deref(), Some("invalid"));

        env.insert(LC_CTYPE.into(), "invalid-also".to_owned());
        assert_eq!(_get(&env).as_deref(), Some("invalid-also"));

        env.insert(LC_ALL.into(), "invalid-again".to_owned());
        assert_eq!(_get(&env).as_deref(), Some("invalid-again"));
    }
}

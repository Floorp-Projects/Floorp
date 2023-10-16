/// Generating build depfiles from parsed bindings.
use std::{collections::BTreeSet, path::PathBuf};

#[derive(Clone, Debug)]
pub(crate) struct DepfileSpec {
    pub output_module: String,
    pub depfile_path: PathBuf,
}

impl DepfileSpec {
    pub fn write(&self, deps: &BTreeSet<String>) -> std::io::Result<()> {
        std::fs::write(&self.depfile_path, self.to_string(deps))
    }

    fn to_string(&self, deps: &BTreeSet<String>) -> String {
        // Transforms a string by escaping spaces and backslashes.
        let escape = |s: &str| s.replace('\\', "\\\\").replace(' ', "\\ ");

        let mut buf = format!("{}:", escape(&self.output_module));
        for file in deps {
            buf = format!("{} {}", buf, escape(file));
        }
        buf
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn escaping_depfile() {
        let spec = DepfileSpec {
            output_module: "Mod Name".to_owned(),
            depfile_path: PathBuf::new(),
        };

        let deps: BTreeSet<String> = vec![
            r"/absolute/path".to_owned(),
            r"C:\win\absolute\path".to_owned(),
            r"../relative/path".to_owned(),
            r"..\win\relative\path".to_owned(),
            r"../path/with spaces/in/it".to_owned(),
            r"..\win\path\with spaces\in\it".to_owned(),
            r"path\with/mixed\separators".to_owned(),
        ]
        .into_iter()
        .collect();
        assert_eq!(
            spec.to_string(&deps),
            "Mod\\ Name: \
            ../path/with\\ spaces/in/it \
            ../relative/path \
            ..\\\\win\\\\path\\\\with\\ spaces\\\\in\\\\it \
            ..\\\\win\\\\relative\\\\path \
            /absolute/path \
            C:\\\\win\\\\absolute\\\\path \
            path\\\\with/mixed\\\\separators"
        );
    }
}

use std::convert::TryInto;
use std::error::Error;
use std::fs::File;
use std::path::Path;

use unic_langid_impl::LanguageIdentifier;

use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
struct LangIdTestInputData {
    string: String,
}

#[derive(Serialize, Deserialize, Debug)]
struct LangIdTestOutputObject {
    language: Option<String>,
    script: Option<String>,
    region: Option<String>,
    #[serde(default)]
    variants: Vec<String>,
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(untagged)]
enum LangIdTestOutput {
    String(String),
    Object(LangIdTestOutputObject),
}

#[derive(Serialize, Deserialize)]
struct LangIdTestSet {
    input: LangIdTestInputData,
    output: LangIdTestOutput,
}

fn read_langid_testsets<P: AsRef<Path>>(path: P) -> Result<Vec<LangIdTestSet>, Box<dyn Error>> {
    let file = File::open(path)?;
    let sets = serde_json::from_reader(file)?;
    Ok(sets)
}

fn test_langid_fixtures(path: &str) {
    let tests = read_langid_testsets(path).unwrap();

    for test in tests {
        let s = test.input.string;

        let langid: LanguageIdentifier = s.parse().expect("Parsing failed.");

        match test.output {
            LangIdTestOutput::Object(o) => {
                let expected = LanguageIdentifier::from_parts(
                    o.language.try_into().unwrap(),
                    o.script.as_ref().map(|s| s.parse().unwrap()),
                    o.region.as_ref().map(|r| r.parse().unwrap()),
                    o.variants
                        .iter()
                        .map(|s| s.parse().unwrap())
                        .collect::<Vec<_>>()
                        .as_ref(),
                );
                assert_eq!(langid, expected);
            }
            LangIdTestOutput::String(s) => {
                assert_eq!(langid.to_string(), s);
            }
        }
    }
}

#[test]
fn parse() {
    test_langid_fixtures("./tests/fixtures/parsing.json");
}

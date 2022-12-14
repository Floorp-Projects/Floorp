use serde_json::Value;
use std::fs;
use tinystr::TinyStr8;
use unic_langid_impl::{subtags, LanguageIdentifier};

type LangIdSubTags = (Option<u64>, Option<u32>, Option<u32>);

fn serialize_val(input: LangIdSubTags) -> String {
    format!(
        "({}, {}, {})",
        serialize_lang_option(input.0),
        serialize_script_option(input.1),
        serialize_region_option(input.2)
    )
}

fn serialize_lang_option(l: Option<u64>) -> String {
    if let Some(l) = l {
        format!("Some({})", l)
    } else {
        String::from("None")
    }
}

fn serialize_script_option(r: Option<u32>) -> String {
    if let Some(r) = r {
        format!("Some({})", r)
    } else {
        String::from("None")
    }
}

fn serialize_region_option(r: Option<u32>) -> String {
    if let Some(r) = r {
        format!("Some({})", r)
    } else {
        String::from("None")
    }
}

fn main() {
    let contents = fs::read_to_string("./data/likelySubtags.json")
        .expect("Something went wrong reading the file");
    let v: Value = serde_json::from_str(&contents).unwrap();
    let values = v["supplemental"]["likelySubtags"].as_object().unwrap();

    let mut lang_only: Vec<(u64, LangIdSubTags)> = vec![];
    let mut lang_region: Vec<(u64, u32, LangIdSubTags)> = vec![];
    let mut lang_script: Vec<(u64, u32, LangIdSubTags)> = vec![];
    let mut script_region: Vec<(u32, u32, LangIdSubTags)> = vec![];
    let mut region_only: Vec<(u32, LangIdSubTags)> = vec![];
    let mut script_only: Vec<(u32, LangIdSubTags)> = vec![];

    let zz_region: subtags::Region = "ZZ".parse().unwrap();

    for (k, v) in values {
        let key_langid: LanguageIdentifier = k.parse().expect("Failed to parse a key.");
        let v: &str = v.as_str().unwrap();
        let mut value_langid: LanguageIdentifier = v.parse().expect("Failed to parse a value.");
        if Some(zz_region) == value_langid.region {
            value_langid.region = None;
        }
        let (val_lang, val_script, val_region, _) = value_langid.into_parts();

        let val_lang: Option<u64> = val_lang.into();
        let val_script: Option<u32> = val_script.map(Into::into);
        let val_region: Option<u32> = val_region.map(Into::into);

        let lang = if key_langid.language.is_empty() {
            None
        } else {
            Some(key_langid.language)
        };
        let script = key_langid.script;
        let region = key_langid.region;

        match (lang, script, region) {
            (None, None, None) => lang_only.push((
                u64::from_le_bytes(*TinyStr8::from_str("und").unwrap().all_bytes()),
                (val_lang, val_script, val_region),
            )),
            (Some(l), None, None) => lang_only.push((
                Into::<Option<u64>>::into(l).unwrap(),
                (val_lang, val_script, val_region),
            )),
            (Some(l), None, Some(r)) => lang_region.push((
                Into::<Option<u64>>::into(l).unwrap(),
                r.into(),
                (val_lang, val_script, val_region),
            )),
            (Some(l), Some(s), None) => lang_script.push((
                Into::<Option<u64>>::into(l).unwrap(),
                s.into(),
                (val_lang, val_script, val_region),
            )),
            (None, Some(s), Some(r)) => {
                script_region.push((s.into(), r.into(), (val_lang, val_script, val_region)))
            }
            (None, Some(s), None) => {
                script_only.push((s.into(), (val_lang, val_script, val_region)))
            }
            (None, None, Some(r)) => {
                region_only.push((r.into(), (val_lang, val_script, val_region)))
            }
            _ => {
                panic!("{:#?}", key_langid);
            }
        }
    }

    println!("#![allow(clippy::type_complexity)]");
    println!("#![allow(clippy::unreadable_literal)]\n");

    let version = v["supplemental"]["version"]["_cldrVersion"]
        .as_str()
        .unwrap();
    println!("pub static CLDR_VERSION: &str = \"{}\";", version);

    println!(
        "pub static LANG_ONLY: [(u64, (Option<u64>, Option<u32>, Option<u32>)); {}] = [",
        lang_only.len()
    );
    lang_only.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
    for (key_lang, val) in lang_only {
        println!("    ({}, {}),", key_lang, serialize_val(val),);
    }
    println!("];");

    println!(
        "pub static LANG_REGION: [(u64, u32, (Option<u64>, Option<u32>, Option<u32>)); {}] = [",
        lang_region.len()
    );
    lang_region.sort_by(|a, b| {
        a.0.partial_cmp(&b.0)
            .unwrap()
            .then_with(|| a.1.partial_cmp(&b.1).unwrap())
    });
    for (key_lang, key_region, val) in lang_region {
        println!(
            "    ({}, {}, {}),",
            key_lang,
            key_region,
            serialize_val(val),
        );
    }
    println!("];");
    println!(
        "pub static LANG_SCRIPT: [(u64, u32, (Option<u64>, Option<u32>, Option<u32>)); {}] = [",
        lang_script.len()
    );
    lang_script.sort_by(|a, b| {
        a.0.partial_cmp(&b.0)
            .unwrap()
            .then_with(|| a.1.partial_cmp(&b.1).unwrap())
    });
    for (key_lang, key_script, val) in lang_script {
        println!(
            "    ({}, {}, {}),",
            key_lang,
            key_script,
            serialize_val(val),
        );
    }
    println!("];");
    println!(
        "pub static SCRIPT_REGION: [(u32, u32, (Option<u64>, Option<u32>, Option<u32>)); {}] = [",
        script_region.len()
    );
    script_region.sort_by(|a, b| {
        a.0.partial_cmp(&b.0)
            .unwrap()
            .then_with(|| a.1.partial_cmp(&b.1).unwrap())
    });
    for (key_script, key_region, val) in script_region {
        println!(
            "    ({}, {}, {}),",
            key_script,
            key_region,
            serialize_val(val),
        );
    }
    println!("];");
    println!(
        "pub static SCRIPT_ONLY: [(u32, (Option<u64>, Option<u32>, Option<u32>)); {}] = [",
        script_only.len()
    );
    script_only.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
    for (key_script, val) in script_only {
        println!("    ({}, {}),", key_script, serialize_val(val),);
    }
    println!("];");
    println!(
        "pub static REGION_ONLY: [(u32, (Option<u64>, Option<u32>, Option<u32>)); {}] = [",
        region_only.len()
    );
    region_only.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
    for (key_region, val) in region_only {
        println!("    ({}, {}),", key_region, serialize_val(val),);
    }
    println!("];");
}

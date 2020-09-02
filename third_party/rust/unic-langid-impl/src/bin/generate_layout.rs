use serde_json::Value;
use std::collections::HashMap;
use std::collections::HashSet;
use std::fs;
use unic_langid_impl::subtags::{Language, Script};
use unic_langid_impl::CharacterDirection;
use unic_langid_impl::LanguageIdentifier;

fn langid_to_direction_map(path: &str) -> HashMap<LanguageIdentifier, CharacterDirection> {
    let mut result = HashMap::new();
    for entry in fs::read_dir(path).unwrap() {
        let entry = entry.unwrap();
        let mut path = entry.path();
        path.push("layout.json");
        let contents = fs::read_to_string(path).expect("Something went wrong reading the file");
        let v: Value = serde_json::from_str(&contents).unwrap();

        let langid_key = v["main"].as_object().unwrap().keys().nth(0).unwrap();

        if langid_key == "root" {
            continue;
        }
        let langid: LanguageIdentifier = langid_key.parse().unwrap();

        let character_order = match v["main"][langid_key]["layout"]["orientation"]["characterOrder"]
            .as_str()
            .unwrap()
        {
            "right-to-left" => CharacterDirection::RTL,
            "left-to-right" => CharacterDirection::LTR,
            _ => unimplemented!("Encountered unknown directionality!"),
        };
        result.insert(langid, character_order);
    }
    result
}

fn check_all_variants_rtl(
    map: &HashMap<LanguageIdentifier, CharacterDirection>,
    lang: Option<Language>,
    script: Option<Script>,
) -> bool {
    for (langid, dir) in map.iter() {
        if let Some(reference_script) = script {
            if let Some(s) = langid.script {
                if reference_script == s && dir != &CharacterDirection::RTL {
                    return false;
                }
            }
        }
        if let Some(reference_lang) = lang {
            if langid.language == reference_lang && dir != &CharacterDirection::RTL {
                println!("{:#?}", langid);
                println!("{:#?}", lang);
                return false;
            }
        }
    }
    true
}

fn main() {
    let path = "./data/cldr-misc-full/main/";
    let map = langid_to_direction_map(path);

    let mut scripts = HashSet::new();
    let mut langs = HashSet::new();

    for (langid, dir) in map.iter() {
        if dir == &CharacterDirection::LTR {
            continue;
        }

        let script = langid.script;

        if let Some(script) = script {
            if scripts.contains(&script) {
                continue;
            }
            assert!(
                check_all_variants_rtl(&map, None, Some(script)),
                "We didn't expect a script with two directionalities!"
            );
            scripts.insert(script);
            continue;
        }

        let lang = langid.language;

        if langs.contains(&lang) {
            continue;
        }

        assert!(
            check_all_variants_rtl(&map, Some(lang), None),
            "We didn't expect a language with two directionalities!"
        );
        langs.insert(lang);
    }

    let mut scripts: Vec<String> = scripts
        .into_iter()
        .map(|s| {
            let v: u32 = s.into();
            v.to_string()
        })
        .collect();
    scripts.sort();
    let mut langs: Vec<String> = langs
        .into_iter()
        .map(|s| {
            let v: Option<u64> = s.into();
            let v: u64 = v.expect("Expected language to not be undefined.");
            v.to_string()
        })
        .collect();
    langs.sort();

    println!(
        "pub const SCRIPTS_CHARACTER_DIRECTION_RTL: [u32; {}] = [{}];",
        scripts.len(),
        scripts.join(", ")
    );

    println!(
        "pub const LANGS_CHARACTER_DIRECTION_RTL: [u64; {}] = [{}];",
        langs.len(),
        langs.join(", ")
    );
}

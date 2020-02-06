use serde_json::Value;
use std::collections::HashMap;
use std::fs;
use std::str::FromStr;
use tinystr::TinyStr8;
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
    lang: &str,
) -> bool {
    for (langid, dir) in map.iter() {
        if langid.language() == lang && dir != &CharacterDirection::RTL {
            return false;
        }
    }
    true
}

fn main() {
    let path = "./data/cldr-misc-modern/main/";
    let map = langid_to_direction_map(path);

    let mut result = vec![];

    for (langid, dir) in map.iter() {
        if dir == &CharacterDirection::LTR {
            continue;
        }

        let lang = langid.language().to_string();

        assert!(
            check_all_variants_rtl(&map, &lang),
            "We didn't expect a language with two directionalities!"
        );
        if !result.contains(&lang) {
            result.push(lang.to_string());
        }
    }

    let list: Vec<String> = result
        .iter()
        .map(|s| {
            let num: u64 = TinyStr8::from_str(s).unwrap().into();
            num.to_string()
        })
        .collect();
    println!(
        "pub const CHARACTER_DIRECTION_RTL: [u64; {}] = [{}];",
        result.len(),
        list.join(", ")
    );
}

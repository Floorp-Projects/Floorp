use serde_json::Value;
use std::collections::HashMap;
use std::collections::HashSet;
use std::fs;
use unic_langid_impl::subtags::Script;
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

        let langid_key = v["main"].as_object().unwrap().keys().next().unwrap();

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
            "top-to-bottom" => CharacterDirection::TTB,
            _ => unimplemented!("Encountered unknown directionality!"),
        };
        result.insert(langid, character_order);
    }
    result
}

fn check_all_script_variants(
    map: &HashMap<LanguageIdentifier, CharacterDirection>,
    exp_dir: CharacterDirection,
    script: Option<Script>,
) -> bool {
    for (langid, dir) in map.iter() {
        if let Some(reference_script) = script {
            if let Some(s) = langid.script {
                if reference_script == s && *dir != exp_dir {
                    return false;
                }
            }
        }
    }
    true
}

fn main() {
    let path = "./data/cldr-misc-full/main/";
    let map = langid_to_direction_map(path);

    let mut scripts_ltr = HashSet::new();
    let mut scripts_rtl = HashSet::new();
    let mut scripts_ttb = HashSet::new();
    let mut langs_rtl = HashSet::new();

    for (langid, dir) in map.iter() {
        let script = langid.script;

        let scripts = match dir {
            CharacterDirection::LTR => &mut scripts_ltr,
            CharacterDirection::RTL => {
                langs_rtl.insert(langid.language);
                &mut scripts_rtl
            }
            CharacterDirection::TTB => &mut scripts_ttb,
        };

        if let Some(script) = script {
            if scripts.contains(&script) {
                continue;
            }
            assert!(
                check_all_script_variants(&map, *dir, Some(script)),
                "We didn't expect a script with two directionalities!"
            );
            scripts.insert(script);
            continue;
        }
    }

    let mut scripts_ltr: Vec<String> = scripts_ltr
        .into_iter()
        .map(|s| {
            let v: u32 = s.into();
            v.to_string()
        })
        .collect();
    scripts_ltr.sort();
    let mut scripts_rtl: Vec<String> = scripts_rtl
        .into_iter()
        .map(|s| {
            let v: u32 = s.into();
            v.to_string()
        })
        .collect();
    scripts_rtl.sort();
    let mut scripts_ttb: Vec<String> = scripts_ttb
        .into_iter()
        .map(|s| {
            let v: u32 = s.into();
            v.to_string()
        })
        .collect();
    scripts_ttb.sort();
    let mut langs_rtl: Vec<String> = langs_rtl
        .into_iter()
        .map(|s| {
            let v: Option<u64> = s.into();
            let v: u64 = v.expect("Expected language to not be undefined.");
            v.to_string()
        })
        .collect();
    langs_rtl.sort();

    println!(
        "pub const SCRIPTS_CHARACTER_DIRECTION_LTR: [u32; {}] = [{}];",
        scripts_ltr.len(),
        scripts_ltr.join(", ")
    );

    println!(
        "pub const SCRIPTS_CHARACTER_DIRECTION_RTL: [u32; {}] = [{}];",
        scripts_rtl.len(),
        scripts_rtl.join(", ")
    );

    println!(
        "pub const SCRIPTS_CHARACTER_DIRECTION_TTB: [u32; {}] = [{}];",
        scripts_ttb.len(),
        scripts_ttb.join(", ")
    );

    println!(
        "pub const LANGS_CHARACTER_DIRECTION_RTL: [u64; {}] = [{}];",
        langs_rtl.len(),
        langs_rtl.join(", ")
    );
}

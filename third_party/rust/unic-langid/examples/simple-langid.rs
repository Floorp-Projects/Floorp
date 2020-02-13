#[cfg(feature = "unic-langid-macros")]
use unic_langid::langid;
use unic_langid::LanguageIdentifier;

#[cfg(feature = "unic-langid-macros")]
const EN_US: LanguageIdentifier = langid!("en-US");

fn main() {
    let langid: LanguageIdentifier = "fr-CA".parse().unwrap();
    println!("{:#?}", langid);
    assert_eq!(langid.language(), "fr");

    #[cfg(feature = "unic-langid-macros")]
    {
        let langid = langid!("de-AT");
        println!("{:#?}", langid);
        assert_eq!(langid.language(), "de");
        assert_eq!(EN_US.language(), "en");
    }
}

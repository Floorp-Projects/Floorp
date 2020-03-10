use intl_memoizer::{IntlMemoizer, Memoizable};
use unic_langid::LanguageIdentifier;

#[derive(Clone, Hash, PartialEq, Eq)]
struct NumberFormatOptions {
    minimum_fraction_digits: usize,
    maximum_fraction_digits: usize,
}

struct NumberFormat {
    lang: LanguageIdentifier,
    options: NumberFormatOptions,
}

impl NumberFormat {
    pub fn new(lang: LanguageIdentifier, options: NumberFormatOptions) -> Result<Self, ()> {
        Ok(Self { lang, options })
    }

    pub fn format(&self, input: isize) -> String {
        format!(
            "{}: {}, MFD: {}",
            self.lang, input, self.options.minimum_fraction_digits
        )
    }
}

impl Memoizable for NumberFormat {
    type Args = (NumberFormatOptions,);
    type Error = ();
    fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error> {
        Self::new(lang, args.0)
    }
}

fn main() {
    let mut memoizer = IntlMemoizer::default();

    let lang: LanguageIdentifier = "en-US".parse().unwrap();
    {
        // Create an en-US memoizer
        let lang_memoizer = memoizer.get_for_lang(lang.clone());
        let options = NumberFormatOptions {
            minimum_fraction_digits: 3,
            maximum_fraction_digits: 5,
        };
        let result = lang_memoizer
            .with_try_get::<NumberFormat, _, _>((options,), |nf| nf.format(2))
            .unwrap();

        assert_eq!(&result, "en-US: 2, MFD: 3");

        // Reuse the same en-US memoizer
        let lang_memoizer = memoizer.get_for_lang(lang.clone());
        let options = NumberFormatOptions {
            minimum_fraction_digits: 3,
            maximum_fraction_digits: 5,
        };
        let result = lang_memoizer
            .with_try_get::<NumberFormat, _, _>((options,), |nf| nf.format(1))
            .unwrap();
        assert_eq!(&result, "en-US: 1, MFD: 3");
    }

    {
        // Here, we will construct a new lang memoizer
        let lang_memoizer = memoizer.get_for_lang(lang.clone());
        let options = NumberFormatOptions {
            minimum_fraction_digits: 3,
            maximum_fraction_digits: 5,
        };
        let result = lang_memoizer
            .with_try_get::<NumberFormat, _, _>((options,), |nf| nf.format(7))
            .unwrap();

        assert_eq!(&result, "en-US: 7, MFD: 3");
    }
}

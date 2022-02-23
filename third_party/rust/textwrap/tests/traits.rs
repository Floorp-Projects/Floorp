use textwrap::word_separators::{AsciiSpace, WordSeparator};
use textwrap::word_splitters::{HyphenSplitter, NoHyphenation, WordSplitter};
use textwrap::wrap_algorithms::{FirstFit, WrapAlgorithm};
use textwrap::Options;

/// Cleaned up type name.
fn type_name<T: ?Sized>(_val: &T) -> String {
    std::any::type_name::<T>().replace("alloc::boxed::Box", "Box")
}

#[test]
#[cfg(not(feature = "smawk"))]
#[cfg(not(feature = "unicode-linebreak"))]
fn static_hyphensplitter() {
    // Inferring the full type.
    let options = Options::new(10);
    assert_eq!(
        type_name(&options),
        format!(
            "textwrap::Options<{}, {}, {}>",
            "textwrap::wrap_algorithms::FirstFit",
            "textwrap::word_separators::AsciiSpace",
            "textwrap::word_splitters::HyphenSplitter"
        )
    );

    // Inferring part of the type.
    let options: Options<_, _, HyphenSplitter> = Options::new(10);
    assert_eq!(
        type_name(&options),
        format!(
            "textwrap::Options<{}, {}, {}>",
            "textwrap::wrap_algorithms::FirstFit",
            "textwrap::word_separators::AsciiSpace",
            "textwrap::word_splitters::HyphenSplitter"
        )
    );

    // Explicitly making all parameters inferred.
    let options: Options<_, _, _> = Options::new(10);
    assert_eq!(
        type_name(&options),
        format!(
            "textwrap::Options<{}, {}, {}>",
            "textwrap::wrap_algorithms::FirstFit",
            "textwrap::word_separators::AsciiSpace",
            "textwrap::word_splitters::HyphenSplitter"
        )
    );
}

#[test]
fn box_static_nohyphenation() {
    // Inferred static type.
    let options = Options::new(10)
        .wrap_algorithm(Box::new(FirstFit))
        .word_splitter(Box::new(NoHyphenation))
        .word_separator(Box::new(AsciiSpace));
    assert_eq!(
        type_name(&options),
        format!(
            "textwrap::Options<{}, {}, {}>",
            "Box<textwrap::wrap_algorithms::FirstFit>",
            "Box<textwrap::word_separators::AsciiSpace>",
            "Box<textwrap::word_splitters::NoHyphenation>"
        )
    );
}

#[test]
fn box_dyn_wordsplitter() {
    // Inferred dynamic type due to default type parameter.
    let options = Options::new(10)
        .wrap_algorithm(Box::new(FirstFit) as Box<dyn WrapAlgorithm>)
        .word_splitter(Box::new(HyphenSplitter) as Box<dyn WordSplitter>)
        .word_separator(Box::new(AsciiSpace) as Box<dyn WordSeparator>);
    assert_eq!(
        type_name(&options),
        format!(
            "textwrap::Options<{}, {}, {}>",
            "Box<dyn textwrap::wrap_algorithms::WrapAlgorithm>",
            "Box<dyn textwrap::word_separators::WordSeparator>",
            "Box<dyn textwrap::word_splitters::WordSplitter>"
        )
    );
}

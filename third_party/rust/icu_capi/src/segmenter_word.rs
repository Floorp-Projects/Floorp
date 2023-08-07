// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::errors::ffi::ICU4XError;
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use core::convert::TryFrom;
    use icu_provider::DataProvider;
    use icu_segmenter::provider::{
        DictionaryForWordLineExtendedV1Marker, DictionaryForWordOnlyAutoV1Marker,
        GraphemeClusterBreakDataV1Marker, LstmForWordLineAutoV1Marker, WordBreakDataV1Marker,
    };
    use icu_segmenter::{
        WordBreakIteratorLatin1, WordBreakIteratorPotentiallyIllFormedUtf8, WordBreakIteratorUtf16,
        WordSegmenter, WordType,
    };

    #[diplomat::enum_convert(WordType, needs_wildcard)]
    #[diplomat::rust_link(icu::segmenter::WordType, Enum)]
    pub enum ICU4XSegmenterWordType {
        None = 0,
        Number = 1,
        Letter = 2,
    }

    #[diplomat::opaque]
    /// An ICU4X word-break segmenter, capable of finding word breakpoints in strings.
    #[diplomat::rust_link(icu::segmenter::WordSegmenter, Struct)]
    pub struct ICU4XWordSegmenter(WordSegmenter);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::WordBreakIterator, Struct)]
    #[diplomat::rust_link(
        icu::segmenter::WordBreakIteratorPotentiallyIllFormedUtf8,
        Typedef,
        hidden
    )]
    pub struct ICU4XWordBreakIteratorUtf8<'a>(WordBreakIteratorPotentiallyIllFormedUtf8<'a, 'a>);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::WordBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::WordBreakIteratorUtf16, Typedef, hidden)]
    pub struct ICU4XWordBreakIteratorUtf16<'a>(WordBreakIteratorUtf16<'a, 'a>);

    #[diplomat::opaque]
    #[diplomat::rust_link(icu::segmenter::WordBreakIterator, Struct)]
    #[diplomat::rust_link(icu::segmenter::WordBreakIteratorLatin1, Typedef, hidden)]
    pub struct ICU4XWordBreakIteratorLatin1<'a>(WordBreakIteratorLatin1<'a, 'a>);

    impl ICU4XWordSegmenter {
        /// Construct an [`ICU4XWordSegmenter`] with automatically selecting the best available LSTM
        /// or dictionary payload data.
        ///
        /// Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
        /// Khmer, Lao, and Thai.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_auto_unstable, FnInStruct)]
        pub fn create_auto(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XWordSegmenter>, ICU4XError> {
            Self::try_new_auto_impl(&provider.0)
        }

        fn try_new_auto_impl<D>(provider: &D) -> Result<Box<ICU4XWordSegmenter>, ICU4XError>
        where
            D: DataProvider<WordBreakDataV1Marker>
                + DataProvider<DictionaryForWordOnlyAutoV1Marker>
                + DataProvider<LstmForWordLineAutoV1Marker>
                + DataProvider<GraphemeClusterBreakDataV1Marker>
                + ?Sized,
        {
            Ok(Box::new(ICU4XWordSegmenter(
                WordSegmenter::try_new_auto_unstable(provider)?,
            )))
        }

        /// Construct an [`ICU4XWordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
        /// Thai.
        ///
        /// Warning: [`ICU4XWordSegmenter`] created by this function doesn't handle Chinese or
        /// Japanese.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::try_new_lstm_unstable, FnInStruct)]
        pub fn create_lstm(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XWordSegmenter>, ICU4XError> {
            Self::try_new_lstm_impl(&provider.0)
        }

        fn try_new_lstm_impl<D>(provider: &D) -> Result<Box<ICU4XWordSegmenter>, ICU4XError>
        where
            D: DataProvider<WordBreakDataV1Marker>
                + DataProvider<LstmForWordLineAutoV1Marker>
                + DataProvider<GraphemeClusterBreakDataV1Marker>
                + ?Sized,
        {
            Ok(Box::new(ICU4XWordSegmenter(
                WordSegmenter::try_new_lstm_unstable(provider)?,
            )))
        }

        /// Construct an [`ICU4XWordSegmenter`] with dictionary payload data for Chinese, Japanese,
        /// Burmese, Khmer, Lao, and Thai.
        #[diplomat::rust_link(
            icu::segmenter::WordSegmenter::try_new_dictionary_unstable,
            FnInStruct
        )]
        pub fn create_dictionary(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XWordSegmenter>, ICU4XError> {
            Self::try_new_dictionary_impl(&provider.0)
        }

        fn try_new_dictionary_impl<D>(provider: &D) -> Result<Box<ICU4XWordSegmenter>, ICU4XError>
        where
            D: DataProvider<WordBreakDataV1Marker>
                + DataProvider<DictionaryForWordOnlyAutoV1Marker>
                + DataProvider<DictionaryForWordLineExtendedV1Marker>
                + DataProvider<GraphemeClusterBreakDataV1Marker>
                + ?Sized,
        {
            Ok(Box::new(ICU4XWordSegmenter(
                WordSegmenter::try_new_dictionary_unstable(provider)?,
            )))
        }

        /// Segments a (potentially ill-formed) UTF-8 string.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::segment_utf8, FnInStruct)]
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::segment_str, FnInStruct, hidden)]
        pub fn segment_utf8<'a>(&'a self, input: &'a str) -> Box<ICU4XWordBreakIteratorUtf8<'a>> {
            let input = input.as_bytes(); // #2520
            Box::new(ICU4XWordBreakIteratorUtf8(self.0.segment_utf8(input)))
        }

        /// Segments a UTF-16 string.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::segment_utf16, FnInStruct)]
        pub fn segment_utf16<'a>(
            &'a self,
            input: &'a [u16],
        ) -> Box<ICU4XWordBreakIteratorUtf16<'a>> {
            Box::new(ICU4XWordBreakIteratorUtf16(self.0.segment_utf16(input)))
        }

        /// Segments a Latin-1 string.
        #[diplomat::rust_link(icu::segmenter::WordSegmenter::segment_latin1, FnInStruct)]
        pub fn segment_latin1<'a>(
            &'a self,
            input: &'a [u8],
        ) -> Box<ICU4XWordBreakIteratorLatin1<'a>> {
            Box::new(ICU4XWordBreakIteratorLatin1(self.0.segment_latin1(input)))
        }
    }

    impl<'a> ICU4XWordBreakIteratorUtf8<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::WordBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }

        /// Return the status value of break boundary.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::word_type, FnInStruct)]
        pub fn word_type(&self) -> ICU4XSegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::is_word_like, FnInStruct)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }

    impl<'a> ICU4XWordBreakIteratorUtf16<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::WordBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }

        /// Return the status value of break boundary.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::word_type, FnInStruct)]
        pub fn word_type(&self) -> ICU4XSegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::is_word_like, FnInStruct)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }

    impl<'a> ICU4XWordBreakIteratorLatin1<'a> {
        /// Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
        /// out of range of a 32-bit signed integer.
        #[allow(clippy::should_implement_trait)]
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::next, FnInStruct)]
        #[diplomat::rust_link(
            icu::segmenter::WordBreakIterator::Item,
            AssociatedTypeInStruct,
            hidden
        )]
        pub fn next(&mut self) -> i32 {
            self.0
                .next()
                .and_then(|u| i32::try_from(u).ok())
                .unwrap_or(-1)
        }

        /// Return the status value of break boundary.
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::word_type, FnInStruct)]
        pub fn word_type(&self) -> ICU4XSegmenterWordType {
            self.0.word_type().into()
        }

        /// Return true when break boundary is word-like such as letter/number/CJK
        #[diplomat::rust_link(icu::segmenter::WordBreakIterator::is_word_like, FnInStruct)]
        pub fn is_word_like(&self) -> bool {
            self.0.is_word_like()
        }
    }
}

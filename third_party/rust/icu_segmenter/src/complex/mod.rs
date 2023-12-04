// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::*;
use alloc::vec::Vec;
use icu_locid::{locale, Locale};
use icu_provider::prelude::*;

mod dictionary;
use dictionary::*;
mod language;
use language::*;
#[cfg(feature = "lstm")]
mod lstm;
#[cfg(feature = "lstm")]
use lstm::*;

#[cfg(not(feature = "lstm"))]
type DictOrLstm = Result<DataPayload<UCharDictionaryBreakDataV1Marker>, core::convert::Infallible>;
#[cfg(not(feature = "lstm"))]
type DictOrLstmBorrowed<'a> =
    Result<&'a DataPayload<UCharDictionaryBreakDataV1Marker>, &'a core::convert::Infallible>;

#[cfg(feature = "lstm")]
type DictOrLstm =
    Result<DataPayload<UCharDictionaryBreakDataV1Marker>, DataPayload<LstmDataV1Marker>>;
#[cfg(feature = "lstm")]
type DictOrLstmBorrowed<'a> =
    Result<&'a DataPayload<UCharDictionaryBreakDataV1Marker>, &'a DataPayload<LstmDataV1Marker>>;

#[derive(Debug)]
pub(crate) struct ComplexPayloads {
    grapheme: DataPayload<GraphemeClusterBreakDataV1Marker>,
    my: Option<DictOrLstm>,
    km: Option<DictOrLstm>,
    lo: Option<DictOrLstm>,
    th: Option<DictOrLstm>,
    ja: Option<DataPayload<UCharDictionaryBreakDataV1Marker>>,
}

impl ComplexPayloads {
    fn select(&self, language: Language) -> Option<DictOrLstmBorrowed> {
        const ERR: DataError = DataError::custom("No segmentation model for language");
        match language {
            Language::Burmese => self.my.as_ref().map(Result::as_ref).or_else(|| {
                ERR.with_display_context("my");
                None
            }),
            Language::Khmer => self.km.as_ref().map(Result::as_ref).or_else(|| {
                ERR.with_display_context("km");
                None
            }),
            Language::Lao => self.lo.as_ref().map(Result::as_ref).or_else(|| {
                ERR.with_display_context("lo");
                None
            }),
            Language::Thai => self.th.as_ref().map(Result::as_ref).or_else(|| {
                ERR.with_display_context("th");
                None
            }),
            Language::ChineseOrJapanese => self.ja.as_ref().map(Ok).or_else(|| {
                ERR.with_display_context("ja");
                None
            }),
            Language::Unknown => None,
        }
    }

    #[cfg(feature = "lstm")]
    #[cfg(feature = "compiled_data")]
    pub(crate) fn new_lstm() -> Self {
        #[allow(clippy::unwrap_used)]
        // try_load is infallible if the provider only returns `MissingLocale`.
        Self {
            grapheme: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_SEGMENTER_GRAPHEME_V1,
            ),
            my: try_load::<LstmForWordLineAutoV1Marker, _>(&crate::provider::Baked, locale!("my"))
                .unwrap()
                .map(DataPayload::cast)
                .map(Err),
            km: try_load::<LstmForWordLineAutoV1Marker, _>(&crate::provider::Baked, locale!("km"))
                .unwrap()
                .map(DataPayload::cast)
                .map(Err),
            lo: try_load::<LstmForWordLineAutoV1Marker, _>(&crate::provider::Baked, locale!("lo"))
                .unwrap()
                .map(DataPayload::cast)
                .map(Err),
            th: try_load::<LstmForWordLineAutoV1Marker, _>(&crate::provider::Baked, locale!("th"))
                .unwrap()
                .map(DataPayload::cast)
                .map(Err),
            ja: None,
        }
    }

    #[cfg(feature = "lstm")]
    pub(crate) fn try_new_lstm<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<GraphemeClusterBreakDataV1Marker>
            + DataProvider<LstmForWordLineAutoV1Marker>
            + ?Sized,
    {
        Ok(Self {
            grapheme: provider.load(Default::default())?.take_payload()?,
            my: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("my"))?
                .map(DataPayload::cast)
                .map(Err),
            km: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("km"))?
                .map(DataPayload::cast)
                .map(Err),
            lo: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("lo"))?
                .map(DataPayload::cast)
                .map(Err),
            th: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("th"))?
                .map(DataPayload::cast)
                .map(Err),
            ja: None,
        })
    }

    #[cfg(feature = "compiled_data")]
    pub(crate) fn new_dict() -> Self {
        #[allow(clippy::unwrap_used)]
        // try_load is infallible if the provider only returns `MissingLocale`.
        Self {
            grapheme: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_SEGMENTER_GRAPHEME_V1,
            ),
            my: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                &crate::provider::Baked,
                locale!("my"),
            )
            .unwrap()
            .map(DataPayload::cast)
            .map(Ok),
            km: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                &crate::provider::Baked,
                locale!("km"),
            )
            .unwrap()
            .map(DataPayload::cast)
            .map(Ok),
            lo: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                &crate::provider::Baked,
                locale!("lo"),
            )
            .unwrap()
            .map(DataPayload::cast)
            .map(Ok),
            th: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                &crate::provider::Baked,
                locale!("th"),
            )
            .unwrap()
            .map(DataPayload::cast)
            .map(Ok),
            ja: try_load::<DictionaryForWordOnlyAutoV1Marker, _>(
                &crate::provider::Baked,
                locale!("ja"),
            )
            .unwrap()
            .map(DataPayload::cast),
        }
    }

    pub(crate) fn try_new_dict<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<GraphemeClusterBreakDataV1Marker>
            + DataProvider<DictionaryForWordLineExtendedV1Marker>
            + DataProvider<DictionaryForWordOnlyAutoV1Marker>
            + ?Sized,
    {
        Ok(Self {
            grapheme: provider.load(Default::default())?.take_payload()?,
            my: try_load::<DictionaryForWordLineExtendedV1Marker, D>(provider, locale!("my"))?
                .map(DataPayload::cast)
                .map(Ok),
            km: try_load::<DictionaryForWordLineExtendedV1Marker, D>(provider, locale!("km"))?
                .map(DataPayload::cast)
                .map(Ok),
            lo: try_load::<DictionaryForWordLineExtendedV1Marker, D>(provider, locale!("lo"))?
                .map(DataPayload::cast)
                .map(Ok),
            th: try_load::<DictionaryForWordLineExtendedV1Marker, D>(provider, locale!("th"))?
                .map(DataPayload::cast)
                .map(Ok),
            ja: try_load::<DictionaryForWordOnlyAutoV1Marker, D>(provider, locale!("ja"))?
                .map(DataPayload::cast),
        })
    }

    #[cfg(feature = "auto")] // Use by WordSegmenter with "auto" enabled.
    #[cfg(feature = "compiled_data")]
    pub(crate) fn new_auto() -> Self {
        #[allow(clippy::unwrap_used)]
        // try_load is infallible if the provider only returns `MissingLocale`.
        Self {
            grapheme: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_SEGMENTER_GRAPHEME_V1,
            ),
            my: try_load::<LstmForWordLineAutoV1Marker, _>(&crate::provider::Baked, locale!("my"))
                .unwrap()
                .map(DataPayload::cast)
                .map(Err),
            km: try_load::<LstmForWordLineAutoV1Marker, _>(&crate::provider::Baked, locale!("km"))
                .unwrap()
                .map(DataPayload::cast)
                .map(Err),
            lo: try_load::<LstmForWordLineAutoV1Marker, _>(&crate::provider::Baked, locale!("lo"))
                .unwrap()
                .map(DataPayload::cast)
                .map(Err),
            th: try_load::<LstmForWordLineAutoV1Marker, _>(&crate::provider::Baked, locale!("th"))
                .unwrap()
                .map(DataPayload::cast)
                .map(Err),
            ja: try_load::<DictionaryForWordOnlyAutoV1Marker, _>(
                &crate::provider::Baked,
                locale!("ja"),
            )
            .unwrap()
            .map(DataPayload::cast),
        }
    }

    #[cfg(feature = "auto")] // Use by WordSegmenter with "auto" enabled.
    pub(crate) fn try_new_auto<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<GraphemeClusterBreakDataV1Marker>
            + DataProvider<LstmForWordLineAutoV1Marker>
            + DataProvider<DictionaryForWordOnlyAutoV1Marker>
            + ?Sized,
    {
        Ok(Self {
            grapheme: provider.load(Default::default())?.take_payload()?,
            my: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("my"))?
                .map(DataPayload::cast)
                .map(Err),
            km: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("km"))?
                .map(DataPayload::cast)
                .map(Err),
            lo: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("lo"))?
                .map(DataPayload::cast)
                .map(Err),
            th: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("th"))?
                .map(DataPayload::cast)
                .map(Err),
            ja: try_load::<DictionaryForWordOnlyAutoV1Marker, D>(provider, locale!("ja"))?
                .map(DataPayload::cast),
        })
    }

    #[cfg(feature = "compiled_data")]
    pub(crate) fn new_southeast_asian() -> Self {
        #[allow(clippy::unwrap_used)]
        // try_load is infallible if the provider only returns `MissingLocale`.
        Self {
            grapheme: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_SEGMENTER_GRAPHEME_V1,
            ),
            my: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                &crate::provider::Baked,
                locale!("my"),
            )
            .unwrap()
            .map(DataPayload::cast)
            .map(Ok),
            km: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                &crate::provider::Baked,
                locale!("km"),
            )
            .unwrap()
            .map(DataPayload::cast)
            .map(Ok),
            lo: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                &crate::provider::Baked,
                locale!("lo"),
            )
            .unwrap()
            .map(DataPayload::cast)
            .map(Ok),
            th: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                &crate::provider::Baked,
                locale!("th"),
            )
            .unwrap()
            .map(DataPayload::cast)
            .map(Ok),
            ja: None,
        }
    }

    pub(crate) fn try_new_southeast_asian<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<DictionaryForWordLineExtendedV1Marker>
            + DataProvider<GraphemeClusterBreakDataV1Marker>
            + ?Sized,
    {
        Ok(Self {
            grapheme: provider.load(Default::default())?.take_payload()?,
            my: try_load::<DictionaryForWordLineExtendedV1Marker, _>(provider, locale!("my"))?
                .map(DataPayload::cast)
                .map(Ok),
            km: try_load::<DictionaryForWordLineExtendedV1Marker, _>(provider, locale!("km"))?
                .map(DataPayload::cast)
                .map(Ok),
            lo: try_load::<DictionaryForWordLineExtendedV1Marker, _>(provider, locale!("lo"))?
                .map(DataPayload::cast)
                .map(Ok),
            th: try_load::<DictionaryForWordLineExtendedV1Marker, _>(provider, locale!("th"))?
                .map(DataPayload::cast)
                .map(Ok),
            ja: None,
        })
    }
}

fn try_load<M: KeyedDataMarker, P: DataProvider<M> + ?Sized>(
    provider: &P,
    locale: Locale,
) -> Result<Option<DataPayload<M>>, DataError> {
    match provider.load(DataRequest {
        locale: &DataLocale::from(locale),
        metadata: {
            let mut m = DataRequestMetadata::default();
            m.silent = true;
            m
        },
    }) {
        Ok(response) => Ok(Some(response.take_payload()?)),
        Err(DataError {
            kind: DataErrorKind::MissingLocale,
            ..
        }) => Ok(None),
        Err(e) => Err(e),
    }
}

/// Return UTF-16 segment offset array using dictionary or lstm segmenter.
pub(crate) fn complex_language_segment_utf16(
    payloads: &ComplexPayloads,
    input: &[u16],
) -> Vec<usize> {
    let mut result = Vec::new();
    let mut offset = 0;
    for (slice, lang) in LanguageIteratorUtf16::new(input) {
        match payloads.select(lang) {
            Some(Ok(dict)) => {
                result.extend(
                    DictionarySegmenter::new(dict.get(), payloads.grapheme.get())
                        .segment_utf16(slice)
                        .map(|n| offset + n),
                );
            }
            #[cfg(feature = "lstm")]
            Some(Err(lstm)) => {
                result.extend(
                    LstmSegmenter::new(lstm.get(), payloads.grapheme.get())
                        .segment_utf16(slice)
                        .map(|n| offset + n),
                );
            }
            #[cfg(not(feature = "lstm"))]
            Some(Err(_infallible)) => {} // should be refutable
            None => {
                result.push(offset + slice.len());
            }
        }
        offset += slice.len();
    }
    result
}

/// Return UTF-8 segment offset array using dictionary or lstm segmenter.
pub(crate) fn complex_language_segment_str(payloads: &ComplexPayloads, input: &str) -> Vec<usize> {
    let mut result = Vec::new();
    let mut offset = 0;
    for (slice, lang) in LanguageIterator::new(input) {
        match payloads.select(lang) {
            Some(Ok(dict)) => {
                result.extend(
                    DictionarySegmenter::new(dict.get(), payloads.grapheme.get())
                        .segment_str(slice)
                        .map(|n| offset + n),
                );
            }
            #[cfg(feature = "lstm")]
            Some(Err(lstm)) => {
                result.extend(
                    LstmSegmenter::new(lstm.get(), payloads.grapheme.get())
                        .segment_str(slice)
                        .map(|n| offset + n),
                );
            }
            #[cfg(not(feature = "lstm"))]
            Some(Err(_infallible)) => {} // should be refutable
            None => {
                result.push(offset + slice.len());
            }
        }
        offset += slice.len();
    }
    result
}

#[cfg(test)]
#[cfg(feature = "serde")]
mod tests {
    use super::*;

    #[test]
    fn thai_word_break() {
        const TEST_STR: &str = "ภาษาไทยภาษาไทย";
        let utf16: Vec<u16> = TEST_STR.encode_utf16().collect();

        let lstm = ComplexPayloads::new_lstm();
        let dict = ComplexPayloads::new_dict();

        assert_eq!(
            complex_language_segment_str(&lstm, TEST_STR),
            [12, 21, 33, 42]
        );
        assert_eq!(
            complex_language_segment_utf16(&lstm, &utf16),
            [4, 7, 11, 14]
        );

        assert_eq!(
            complex_language_segment_str(&dict, TEST_STR),
            [12, 21, 33, 42]
        );
        assert_eq!(
            complex_language_segment_utf16(&dict, &utf16),
            [4, 7, 11, 14]
        );
    }
}

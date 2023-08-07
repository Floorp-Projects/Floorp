// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::dictionary::DictionarySegmenter;
use crate::language::*;
use crate::provider::*;
use alloc::vec::Vec;
use icu_locid::{locale, Locale};
use icu_provider::prelude::*;

#[derive(Debug)]
pub(crate) struct ComplexPayloads {
    grapheme: DataPayload<GraphemeClusterBreakDataV1Marker>,
    burmese_lstm: Option<DataPayload<LstmDataV1Marker>>,
    khmer_lstm: Option<DataPayload<LstmDataV1Marker>>,
    lao_lstm: Option<DataPayload<LstmDataV1Marker>>,
    thai_lstm: Option<DataPayload<LstmDataV1Marker>>,
    burmese_dict: Option<DataPayload<UCharDictionaryBreakDataV1Marker>>,
    khmer_dict: Option<DataPayload<UCharDictionaryBreakDataV1Marker>>,
    lao_dict: Option<DataPayload<UCharDictionaryBreakDataV1Marker>>,
    thai_dict: Option<DataPayload<UCharDictionaryBreakDataV1Marker>>,
    cj_dict: Option<DataPayload<UCharDictionaryBreakDataV1Marker>>,
}

impl ComplexPayloads {
    fn select_lstm(&self, language: Language) -> Option<&DataPayload<LstmDataV1Marker>> {
        match language {
            Language::Burmese => self.burmese_lstm.as_ref(),
            Language::Khmer => self.khmer_lstm.as_ref(),
            Language::Lao => self.lao_lstm.as_ref(),
            Language::Thai => self.thai_lstm.as_ref(),
            Language::ChineseOrJapanese | Language::Unknown => None,
        }
    }

    fn select_dict(
        &self,
        language: Language,
    ) -> Option<&DataPayload<UCharDictionaryBreakDataV1Marker>> {
        match language {
            Language::Burmese => self.burmese_dict.as_ref(),
            Language::Khmer => self.khmer_dict.as_ref(),
            Language::Lao => self.lao_dict.as_ref(),
            Language::Thai => self.thai_dict.as_ref(),
            Language::ChineseOrJapanese => self.cj_dict.as_ref(),
            Language::Unknown => None,
        }
    }

    pub(crate) fn try_new_lstm<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<GraphemeClusterBreakDataV1Marker>
            + DataProvider<LstmForWordLineAutoV1Marker>
            + ?Sized,
    {
        Ok(Self {
            grapheme: provider.load(Default::default())?.take_payload()?,
            burmese_lstm: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("my"))?
                .map(DataPayload::cast),
            khmer_lstm: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("km"))?
                .map(DataPayload::cast),
            lao_lstm: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("lo"))?
                .map(DataPayload::cast),
            thai_lstm: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("th"))?
                .map(DataPayload::cast),
            burmese_dict: None,
            khmer_dict: None,
            lao_dict: None,
            thai_dict: None,
            cj_dict: None,
        })
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
            burmese_lstm: None,
            khmer_lstm: None,
            lao_lstm: None,
            thai_lstm: None,
            burmese_dict: try_load::<DictionaryForWordLineExtendedV1Marker, D>(
                provider,
                locale!("my"),
            )?
            .map(DataPayload::cast),
            khmer_dict: try_load::<DictionaryForWordLineExtendedV1Marker, D>(
                provider,
                locale!("km"),
            )?
            .map(DataPayload::cast),
            lao_dict: try_load::<DictionaryForWordLineExtendedV1Marker, D>(
                provider,
                locale!("lo"),
            )?
            .map(DataPayload::cast),
            thai_dict: try_load::<DictionaryForWordLineExtendedV1Marker, D>(
                provider,
                locale!("th"),
            )?
            .map(DataPayload::cast),
            cj_dict: try_load::<DictionaryForWordOnlyAutoV1Marker, D>(provider, locale!("ja"))?
                .map(DataPayload::cast),
        })
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
            burmese_lstm: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("my"))?
                .map(DataPayload::cast),
            khmer_lstm: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("km"))?
                .map(DataPayload::cast),
            lao_lstm: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("lo"))?
                .map(DataPayload::cast),
            thai_lstm: try_load::<LstmForWordLineAutoV1Marker, D>(provider, locale!("th"))?
                .map(DataPayload::cast),
            burmese_dict: None,
            khmer_dict: None,
            lao_dict: None,
            thai_dict: None,
            cj_dict: try_load::<DictionaryForWordOnlyAutoV1Marker, D>(provider, locale!("ja"))?
                .map(DataPayload::cast),
        })
    }

    pub(crate) fn try_new_southeast_asian<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<DictionaryForWordLineExtendedV1Marker>
            + DataProvider<GraphemeClusterBreakDataV1Marker>
            + ?Sized,
    {
        Ok(Self {
            grapheme: provider.load(Default::default())?.take_payload()?,
            burmese_lstm: None,
            khmer_lstm: None,
            lao_lstm: None,
            thai_lstm: None,
            burmese_dict: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                provider,
                locale!("my"),
            )?
            .map(DataPayload::cast),
            khmer_dict: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                provider,
                locale!("km"),
            )?
            .map(DataPayload::cast),
            lao_dict: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                provider,
                locale!("lo"),
            )?
            .map(DataPayload::cast),
            thai_dict: try_load::<DictionaryForWordLineExtendedV1Marker, _>(
                provider,
                locale!("th"),
            )?
            .map(DataPayload::cast),
            cj_dict: None,
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
#[allow(unused_variables)]
pub(crate) fn complex_language_segment_utf16(
    payloads: &ComplexPayloads,
    input: &[u16],
) -> Vec<usize> {
    let mut result: Vec<usize> = Vec::new();
    let lang_iter = LanguageIteratorUtf16::new(input);
    let mut offset = 0;
    for (str_per_lang, lang) in lang_iter {
        if lang == Language::Unknown {
            offset += str_per_lang.len();
            result.push(offset);
        } else if let Some(lstm) = payloads.select_lstm(lang) {
            #[cfg(feature = "lstm")]
            {
                let segmenter = crate::lstm::LstmSegmenter::new(lstm, &payloads.grapheme);
                let breaks = segmenter.segment_utf16(str_per_lang);
                result.extend(breaks.map(|n| offset + n));
                offset += str_per_lang.len();
            }
        } else if let Some(dict) = payloads.select_dict(lang) {
            let segmenter = DictionarySegmenter::new(dict, &payloads.grapheme);
            let breaks = segmenter.segment_utf16(str_per_lang);
            result.extend(breaks.map(|n| offset + n));
            offset += str_per_lang.len();
        } else {
            // Create error for logging
            DataError::custom("No segmentation model for language").with_display_context(
                match lang {
                    Language::Thai => "th",
                    Language::Lao => "lo",
                    Language::Burmese => "my",
                    Language::Khmer => "km",
                    Language::ChineseOrJapanese => "ja",
                    Language::Unknown => unreachable!(),
                },
            );
            offset += str_per_lang.len();
            result.push(offset);
        }
    }
    result
}

/// Return UTF-8 segment offset array using dictionary or lstm segmenter.
#[allow(unused_variables)]
pub(crate) fn complex_language_segment_str(payloads: &ComplexPayloads, input: &str) -> Vec<usize> {
    let mut result: Vec<usize> = Vec::new();
    let lang_iter = LanguageIterator::new(input);
    let mut offset = 0;
    for (str_per_lang, lang) in lang_iter {
        if lang == Language::Unknown {
            offset += str_per_lang.len();
            result.push(offset);
        } else if let Some(lstm) = payloads.select_lstm(lang) {
            #[cfg(feature = "lstm")]
            {
                let segmenter = crate::lstm::LstmSegmenter::new(lstm, &payloads.grapheme);
                let breaks = segmenter.segment_str(str_per_lang);
                result.extend(breaks.map(|n| offset + n));
                offset += str_per_lang.len();
            }
        } else if let Some(dict) = payloads.select_dict(lang) {
            let segmenter = DictionarySegmenter::new(dict, &payloads.grapheme);
            let breaks = segmenter.segment_str(str_per_lang);
            result.extend(breaks.map(|n| offset + n));
            offset += str_per_lang.len();
        } else {
            // Create error for logging
            DataError::custom("No segmentation model for language").with_display_context(
                match lang {
                    Language::Thai => "th",
                    Language::Lao => "lo",
                    Language::Burmese => "my",
                    Language::Khmer => "km",
                    Language::ChineseOrJapanese => "ja",
                    Language::Unknown => unreachable!(),
                },
            );
            offset += str_per_lang.len();
            result.push(offset);
        }
    }
    result
}

#[cfg(test)]
#[cfg(feature = "serde")]
mod tests {
    use super::*;
    use icu_locid::locale;

    #[test]
    fn thai_word_break() {
        const TEST_STR: &str = "ภาษาไทยภาษาไทย";
        let grapheme = try_load::<GraphemeClusterBreakDataV1Marker, _>(
            &icu_testdata::buffer().as_deserializing(),
            Locale::UND,
        )
        .unwrap()
        .unwrap();
        let dict = try_load::<DictionaryForWordLineExtendedV1Marker, _>(
            &icu_testdata::buffer().as_deserializing(),
            locale!("th"),
        )
        .unwrap()
        .unwrap();
        let lstm = try_load::<LstmForWordLineAutoV1Marker, _>(
            &icu_testdata::buffer().as_deserializing(),
            locale!("th"),
        )
        .unwrap()
        .unwrap();
        let payloads = ComplexPayloads {
            grapheme,
            burmese_lstm: None,
            khmer_lstm: None,
            lao_lstm: None,
            thai_lstm: Some(lstm.cast()),
            burmese_dict: None,
            khmer_dict: None,
            lao_dict: None,
            thai_dict: Some(dict.cast()),
            cj_dict: None,
        };
        let breaks = complex_language_segment_str(&payloads, TEST_STR);
        assert_eq!(breaks, [12, 21, 33, 42], "Thai test by UTF-8");

        let utf16: Vec<u16> = TEST_STR.encode_utf16().collect();
        let breaks = complex_language_segment_utf16(&payloads, &utf16);
        assert_eq!(breaks, [4, 7, 11, 14], "Thai test by UTF-16");
    }
}

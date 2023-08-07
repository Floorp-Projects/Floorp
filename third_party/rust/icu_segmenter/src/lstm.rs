// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::grapheme::GraphemeClusterSegmenter;
use crate::math_helper::{MatrixBorrowedMut, MatrixOwned, MatrixZero};
use crate::provider::*;
use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::char::{decode_utf16, REPLACEMENT_CHARACTER};
use icu_provider::DataPayload;
use zerovec::{maps::ZeroMapBorrowed, ule::UnvalidatedStr};

// A word break iterator using LSTM model. Input string have to be same language.

pub struct LstmSegmenterIterator<'s> {
    input: &'s str,
    bies_str: Box<[Bies]>,
    pos: usize,
    pos_utf8: usize,
}

impl Iterator for LstmSegmenterIterator<'_> {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        #[allow(clippy::indexing_slicing)] // pos_utf8 in range
        loop {
            let bies = *self.bies_str.get(self.pos)?;
            self.pos_utf8 += self.input[self.pos_utf8..].chars().next()?.len_utf8();
            self.pos += 1;
            if bies == Bies::E || self.pos == self.bies_str.len() {
                return Some(self.pos_utf8);
            }
        }
    }
}

pub struct LstmSegmenterIteratorUtf16 {
    bies_str: Box<[Bies]>,
    pos: usize,
}

impl Iterator for LstmSegmenterIteratorUtf16 {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let bies = *self.bies_str.get(self.pos)?;
            self.pos += 1;
            if bies == Bies::E || self.pos == self.bies_str.len() {
                return Some(self.pos);
            }
        }
    }
}

pub(crate) struct LstmSegmenter<'l> {
    dic: ZeroMapBorrowed<'l, UnvalidatedStr, u16>,
    embedding: MatrixZero<'l, 2>,
    fw_w: MatrixZero<'l, 3>,
    fw_u: MatrixZero<'l, 3>,
    fw_b: MatrixZero<'l, 2>,
    bw_w: MatrixZero<'l, 3>,
    bw_u: MatrixZero<'l, 3>,
    bw_b: MatrixZero<'l, 2>,
    time_w: MatrixZero<'l, 3>,
    time_b: MatrixZero<'l, 1>,
    grapheme: Option<&'l RuleBreakDataV1<'l>>,
}

impl<'l> LstmSegmenter<'l> {
    /// Returns `Err` if grapheme data is required but not present
    pub fn new(
        lstm: &'l DataPayload<LstmDataV1Marker>,
        grapheme: &'l DataPayload<GraphemeClusterBreakDataV1Marker>,
    ) -> Self {
        let LstmDataV1::Float32(lstm) = lstm.get();
        Self {
            dic: lstm.dic.as_borrowed(),
            embedding: lstm.embedding.as_matrix_zero(),
            fw_w: lstm.fw_w.as_matrix_zero(),
            fw_u: lstm.fw_u.as_matrix_zero(),
            fw_b: lstm.fw_b.as_matrix_zero(),
            bw_w: lstm.bw_w.as_matrix_zero(),
            bw_u: lstm.bw_u.as_matrix_zero(),
            bw_b: lstm.bw_b.as_matrix_zero(),
            time_w: lstm.time_w.as_matrix_zero(),
            time_b: lstm.time_b.as_matrix_zero(),
            grapheme: (lstm.model == ModelType::GraphemeClusters).then(|| grapheme.get()),
        }
    }

    /// Create an LSTM based break iterator for an `str` (a UTF-8 string).
    pub fn segment_str<'s>(&self, input: &'s str) -> LstmSegmenterIterator<'s> {
        let lstm_output = self.produce_bies(input);
        LstmSegmenterIterator {
            input,
            bies_str: lstm_output,
            pos: 0,
            pos_utf8: 0,
        }
    }

    /// Create an LSTM based break iterator for a UTF-16 string.
    pub fn segment_utf16(&self, input: &[u16]) -> LstmSegmenterIteratorUtf16 {
        let input: String = decode_utf16(input.iter().copied())
            .map(|r| r.unwrap_or(REPLACEMENT_CHARACTER))
            .collect();
        let lstm_output = self.produce_bies(&input);
        LstmSegmenterIteratorUtf16 {
            bies_str: lstm_output,
            pos: 0,
        }
    }

    /// `produce_bies` is a function that gets a "clean" unsegmented string as its input and returns a BIES (B: Beginning, I: Inside, E: End,
    /// S: Single) sequence for grapheme clusters. The boundaries of words can be found easily using this BIES sequence.
    fn produce_bies(&self, input: &str) -> Box<[Bies]> {
        // input_seq is a sequence of id numbers that represents grapheme clusters or code points in the input line. These ids are used later
        // in the embedding layer of the model.
        // Already checked that the name of the model is either "codepoints" or "graphclsut"
        let input_seq: Vec<u16> = if let Some(grapheme) = self.grapheme {
            GraphemeClusterSegmenter::new_and_segment_str(input, grapheme)
                .collect::<Vec<usize>>()
                .windows(2)
                .map(|chunk| {
                    let range = if let [first, second, ..] = chunk {
                        *first..*second
                    } else {
                        unreachable!()
                    };
                    self.dic
                        .get_copied(UnvalidatedStr::from_str(input.get(range).unwrap_or(input)))
                        .unwrap_or_else(|| self.dic.len() as u16)
                })
                .collect()
        } else {
            input
                .chars()
                .map(|c| {
                    self.dic
                        .get_copied(UnvalidatedStr::from_str(c.encode_utf8(&mut [0; 4])))
                        .unwrap_or_else(|| self.dic.len() as u16)
                })
                .collect()
        };

        /// `compute_hc1` implemens the evaluation of one LSTM layer.
        fn compute_hc<'a>(
            x_t: MatrixZero<'a, 1>,
            mut h_tm1: MatrixBorrowedMut<'a, 1>,
            mut c_tm1: MatrixBorrowedMut<'a, 1>,
            w: MatrixZero<'a, 3>,
            u: MatrixZero<'a, 3>,
            b: MatrixZero<'a, 2>,
        ) {
            #[cfg(debug_assertions)]
            {
                let hunits = h_tm1.dim();
                let embedd_dim = x_t.dim();
                c_tm1.as_borrowed().debug_assert_dims([hunits]);
                w.debug_assert_dims([4, hunits, embedd_dim]);
                u.debug_assert_dims([4, hunits, hunits]);
                b.debug_assert_dims([4, hunits]);
            }

            let mut s_t = b.to_owned();

            s_t.as_mut().add_dot_3d_2(x_t, w);
            s_t.as_mut().add_dot_3d_1(h_tm1.as_borrowed(), u);

            #[allow(clippy::unwrap_used)] // first dimension is 4
            s_t.submatrix_mut::<1>(0).unwrap().sigmoid_transform();
            #[allow(clippy::unwrap_used)] // first dimension is 4
            s_t.submatrix_mut::<1>(1).unwrap().sigmoid_transform();
            #[allow(clippy::unwrap_used)] // first dimension is 4
            s_t.submatrix_mut::<1>(2).unwrap().tanh_transform();
            #[allow(clippy::unwrap_used)] // first dimension is 4
            s_t.submatrix_mut::<1>(3).unwrap().sigmoid_transform();

            #[allow(clippy::unwrap_used)] // first dimension is 4
            c_tm1.convolve(
                s_t.as_borrowed().submatrix(0).unwrap(),
                s_t.as_borrowed().submatrix(2).unwrap(),
                s_t.as_borrowed().submatrix(1).unwrap(),
            );

            #[allow(clippy::unwrap_used)] // first dimension is 4
            h_tm1.mul_tanh(s_t.as_borrowed().submatrix(3).unwrap(), c_tm1.as_borrowed());
        }

        let hunits = self.fw_u.dim().1;

        // Forward LSTM
        let mut c_fw = MatrixOwned::<1>::new_zero([hunits]);
        let mut all_h_fw = MatrixOwned::<2>::new_zero([input_seq.len(), hunits]);
        for (i, &g_id) in input_seq.iter().enumerate() {
            #[allow(clippy::unwrap_used)]
            // embedding has shape (dict.len() + 1, hunit), g_id is at most dict.len()
            let x_t = self.embedding.submatrix::<1>(g_id as usize).unwrap();
            if i > 0 {
                all_h_fw.as_mut().copy_submatrix::<1>(i - 1, i);
            }
            #[allow(clippy::unwrap_used)]
            compute_hc(
                x_t,
                all_h_fw.submatrix_mut(i).unwrap(), // shape (input_seq.len(), hunits)
                c_fw.as_mut(),
                self.fw_w,
                self.fw_u,
                self.fw_b,
            );
        }

        // Backward LSTM
        let mut c_bw = MatrixOwned::<1>::new_zero([hunits]);
        let mut all_h_bw = MatrixOwned::<2>::new_zero([input_seq.len(), hunits]);
        for (i, &g_id) in input_seq.iter().enumerate().rev() {
            #[allow(clippy::unwrap_used)]
            // embedding has shape (dict.len() + 1, hunit), g_id is at most dict.len()
            let x_t = self.embedding.submatrix::<1>(g_id as usize).unwrap();
            if i + 1 < input_seq.len() {
                all_h_bw.as_mut().copy_submatrix::<1>(i + 1, i);
            }
            #[allow(clippy::unwrap_used)]
            compute_hc(
                x_t,
                all_h_bw.submatrix_mut(i).unwrap(), // shape (input_seq.len(), hunits)
                c_bw.as_mut(),
                self.bw_w,
                self.bw_u,
                self.bw_b,
            );
        }

        #[allow(clippy::unwrap_used)] // shape (2, 4, hunits)
        let timew_fw = self.time_w.submatrix(0).unwrap();
        #[allow(clippy::unwrap_used)] // shape (2, 4, hunits)
        let timew_bw = self.time_w.submatrix(1).unwrap();

        // Combining forward and backward LSTMs using the dense time-distributed layer
        (0..input_seq.len())
            .map(|i| {
                #[allow(clippy::unwrap_used)] // shape (input_seq.len(), hunits)
                let curr_fw = all_h_fw.submatrix::<1>(i).unwrap();
                #[allow(clippy::unwrap_used)] // shape (input_seq.len(), hunits)
                let curr_bw = all_h_bw.submatrix::<1>(i).unwrap();
                let mut weights = [0.0; 4];
                let mut curr_est = MatrixBorrowedMut {
                    data: &mut weights,
                    dims: [4],
                };
                curr_est.add_dot_2d(curr_fw, timew_fw);
                curr_est.add_dot_2d(curr_bw, timew_bw);
                #[allow(clippy::unwrap_used)] // both shape (4)
                curr_est.add(self.time_b).unwrap();
                curr_est.softmax_transform();
                Bies::from_probabilities(weights)
            })
            .collect()
    }
}

// TODO(#421): Use common BIES normalizer code
#[derive(Debug, PartialEq, Copy, Clone)]
pub enum Bies {
    B,
    I,
    E,
    S,
}

impl Bies {
    /// Returns the value the largest probability
    fn from_probabilities(arr: [f32; 4]) -> Bies {
        let [b, i, e, s] = arr;
        let mut result = Bies::B;
        let mut max = b;
        if i > max {
            result = Bies::I;
            max = i;
        }
        if e > max {
            result = Bies::E;
            max = e;
        }
        if s > max {
            result = Bies::S;
            // max = s;
        }
        result
    }

    #[cfg(test)]
    fn as_char(&self) -> char {
        match self {
            Bies::B => 'b',
            Bies::I => 'i',
            Bies::E => 'e',
            Bies::S => 's',
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::provider::{LstmDataV1Marker, LstmForWordLineAutoV1Marker};
    use icu_locid::locale;
    use icu_provider::prelude::*;
    use serde::Deserialize;
    use std::fs::File;
    use std::io::BufReader;

    /// `TestCase` is a struct used to store a single test case.
    /// Each test case has two attributs: `unseg` which denots the unsegmented line, and `true_bies` which indicates the Bies
    /// sequence representing the true segmentation.
    #[derive(PartialEq, Debug, Deserialize)]
    pub struct TestCase {
        pub unseg: String,
        pub expected_bies: String,
        pub true_bies: String,
    }

    /// `TestTextData` is a struct to store a vector of `TestCase` that represents a test text.
    #[derive(PartialEq, Debug, Deserialize)]
    pub struct TestTextData {
        pub testcases: Vec<TestCase>,
    }

    #[derive(Debug)]
    pub struct TestText {
        pub data: TestTextData,
    }

    fn load_test_text(filename: &str) -> TestTextData {
        let file = File::open(filename).expect("File should be present");
        let reader = BufReader::new(file);
        serde_json::from_reader(reader).expect("JSON syntax error")
    }

    #[test]
    fn segment_file_by_lstm() {
        let lstm: DataPayload<LstmDataV1Marker> =
            DataProvider::<LstmForWordLineAutoV1Marker>::load(
                &icu_testdata::buffer().as_deserializing(),
                DataRequest {
                    locale: &locale!("th").into(),
                    metadata: Default::default(),
                },
            )
            .unwrap()
            .take_payload()
            .unwrap()
            .cast();
        let grapheme: DataPayload<GraphemeClusterBreakDataV1Marker> = icu_testdata::buffer()
            .as_deserializing()
            .load(Default::default())
            .unwrap()
            .take_payload()
            .unwrap();
        let lstm = LstmSegmenter::new(&lstm, &grapheme);

        // Importing the test data
        let test_text_data = load_test_text(&format!(
            "tests/testdata/test_text_{}.json",
            if lstm.grapheme.is_some() {
                "grapheme"
            } else {
                "codepoints"
            }
        ));
        let test_text = TestText {
            data: test_text_data,
        };

        // Testing
        for test_case in test_text.data.testcases {
            let lstm_output = lstm.produce_bies(&test_case.unseg);
            println!("Test case      : {}", test_case.unseg);
            println!("Expected bies  : {}", test_case.expected_bies);
            println!("Estimated bies : {lstm_output:?}");
            println!("True bies      : {}", test_case.true_bies);
            println!("****************************************************");
            assert_eq!(
                test_case.expected_bies,
                lstm_output.iter().map(Bies::as_char).collect::<String>()
            );
        }
    }
}

// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! Macros and utilities for testing.

use std::borrow::ToOwned;
use types::{RawDecoder, RawEncoder};

pub struct TestResult<'a, Output: 'a + ?Sized + ToOwned> {
    pub expected_return: (usize, Option<isize>),
    pub expected_push: &'a Output,
    pub actual_return: (usize, Option<isize>),
    pub actual_push: Output::Owned,
}

pub trait Testable {
    type Input: ?Sized;
    type Output: ?Sized + ToOwned;

    fn process_feed_ok<'a>(&mut self, processed: &Self::Input, unprocessed: &Self::Input,
                           output: &'a Self::Output) -> TestResult<'a, Self::Output>;
    fn process_feed_err<'a>(&mut self, backup: isize,
                            processed: &Self::Input, problem: &Self::Input, remaining: &Self::Input,
                            output: &'a Self::Output) -> TestResult<'a, Self::Output>;
    fn process_finish_ok<'a>(&mut self, output: &'a Self::Output) -> TestResult<'a, Self::Output>;
    fn process_finish_err<'a>(&mut self, backup: isize,
                              output: &'a Self::Output) -> TestResult<'a, Self::Output>;
}

impl Testable for RawDecoder {
    type Input = [u8];
    type Output = str;

    fn process_feed_ok<'a>(&mut self, processed: &[u8], unprocessed: &[u8],
                           output: &'a str) -> TestResult<'a, str> {
        let mut input = Vec::with_capacity(processed.len() + unprocessed.len());
        input.extend(processed.iter().cloned());
        input.extend(unprocessed.iter().cloned());

        let mut buf = String::new();
        let (nprocessed, err) = self.raw_feed(&input, &mut buf);
        TestResult {
            expected_return: (processed.len(), None),
            expected_push: output,
            actual_return: (nprocessed, err.map(|e| e.upto)),
            actual_push: buf,
        }
    }

    fn process_feed_err<'a>(&mut self, backup: isize, processed: &[u8], problem: &[u8],
                            remaining: &[u8], output: &'a str) -> TestResult<'a, str> {
        let mut input = Vec::with_capacity(processed.len() + problem.len() + remaining.len());
        input.extend(processed.iter().cloned());
        input.extend(problem.iter().cloned());
        input.extend(remaining.iter().cloned());

        let mut buf = String::new();
        let (nprocessed, err) = self.raw_feed(&input[-backup as usize..], &mut buf);
        TestResult {
            expected_return: (processed.len(), Some(processed.len() as isize +
                                                    problem.len() as isize + backup)),
            expected_push: output,
            actual_return: (nprocessed, err.map(|e| e.upto)),
            actual_push: buf,
        }
    }

    fn process_finish_ok<'a>(&mut self, output: &'a str) -> TestResult<'a, str> {
        let mut buf = String::new();
        let err = self.raw_finish(&mut buf);
        TestResult {
            expected_return: (0, None),
            expected_push: output,
            actual_return: (0, err.map(|e| e.upto)),
            actual_push: buf,
        }
    }

    fn process_finish_err<'a>(&mut self, backup: isize, output: &'a str) -> TestResult<'a, str> {
        let mut buf = String::new();
        let err = self.raw_finish(&mut buf);
        TestResult {
            expected_return: (0, Some(backup)),
            expected_push: output,
            actual_return: (0, err.map(|e| e.upto)),
            actual_push: buf,
        }
    }
}

impl Testable for RawEncoder {
    type Input = str;
    type Output = [u8];

    fn process_feed_ok<'a>(&mut self, processed: &str, unprocessed: &str,
                           output: &'a [u8]) -> TestResult<'a, [u8]> {
        let mut input = String::with_capacity(processed.len() + unprocessed.len());
        input.push_str(processed);
        input.push_str(unprocessed);

        let mut buf = Vec::new();
        let (nprocessed, err) = self.raw_feed(&input, &mut buf);
        TestResult {
            expected_return: (processed.len(), None),
            expected_push: output,
            actual_return: (nprocessed, err.map(|e| e.upto)),
            actual_push: buf,
        }
    }

    fn process_feed_err<'a>(&mut self, backup: isize, processed: &str, problem: &str,
                            remaining: &str, output: &'a [u8]) -> TestResult<'a, [u8]> {
        let mut input = String::with_capacity(processed.len() + problem.len() + remaining.len());
        input.push_str(processed);
        input.push_str(problem);
        input.push_str(remaining);

        let mut buf = Vec::new();
        let (nprocessed, err) = self.raw_feed(&input[-backup as usize..], &mut buf);
        TestResult {
            expected_return: (processed.len(), Some(processed.len() as isize +
                                                    problem.len() as isize + backup)),
            expected_push: output,
            actual_return: (nprocessed, err.map(|e| e.upto)),
            actual_push: buf,
        }
    }

    fn process_finish_ok<'a>(&mut self, output: &'a [u8]) -> TestResult<'a, [u8]> {
        let mut buf = Vec::new();
        let err = self.raw_finish(&mut buf);
        TestResult {
            expected_return: (0, None),
            expected_push: output,
            actual_return: (0, err.map(|e| e.upto)),
            actual_push: buf,
        }
    }

    fn process_finish_err<'a>(&mut self, backup: isize, output: &'a [u8]) -> TestResult<'a, [u8]> {
        let mut buf = Vec::new();
        let err = self.raw_finish(&mut buf);
        TestResult {
            expected_return: (0, Some(backup)),
            expected_push: output,
            actual_return: (0, err.map(|e| e.upto)),
            actual_push: buf,
        }
    }
}

macro_rules! assert_expected {
    ($result:expr, $func:expr, $filter:expr) => ({
        use testutils::Testable;
        match $result {
            result => {
                assert!(result.expected_return == result.actual_return,
                        "{} should return {:?}, but instead returned {:?}",
                        $func, $filter(result.expected_return), $filter(result.actual_return));
                assert!(&result.expected_push[..] == &result.actual_push[..],
                        "{} should push {:?}, but instead pushed {:?}",
                        $func, result.expected_push, result.actual_push);
            }
        }
    });
}

macro_rules! assert_feed_ok {
    ($this:expr, $processed:expr, $unprocessed:expr, $output:expr) => (
        assert_expected!($this.process_feed_ok(&$processed, &$unprocessed, &$output),
                         "raw_feed", |r| r)
    );
}

macro_rules! assert_feed_err {
    ($this:expr, $backup:expr, $processed:expr, $problem:expr, $remaining:expr, $output:expr) => (
        assert_expected!($this.process_feed_err($backup, &$processed, &$problem, &$remaining,
                                                &$output),
                         "raw_feed", |r| r)
    );
    ($this:expr, $processed:expr, $problem:expr, $remaining:expr, $output:expr) => (
        assert_feed_err!($this, 0, $processed, $problem, $remaining, $output)
    );
}

macro_rules! assert_finish_ok {
    ($this:expr, $output:expr) => (
        assert_expected!($this.process_finish_ok(&$output),
                         "raw_finish", |r: (usize, Option<isize>)| r.0)
    );
}

macro_rules! assert_finish_err {
    ($this:expr, $backup:expr, $output:expr) => (
        assert_expected!($this.process_finish_err($backup, &$output),
                         "raw_finish", |r: (usize, Option<isize>)| r.0)
    );
    ($this:expr, $output:expr) => (
        assert_finish_err!($this, 0, $output)
    );
}

/// Some ASCII-only text to test.
//
// the first paragraphs of the article "English Language" from English Wikipedia.
// https://en.wikipedia.org/w/index.php?title=English_language&oldid=608500518
pub static ASCII_TEXT: &'static str =
    "English is a West Germanic language that was first spoken in early medieval England \
     and is now a global lingua franca. It is spoken as a first language by \
     the majority populations of several sovereign states, including the United Kingdom, \
     the United States, Canada, Australia, Ireland, New Zealand and a number of Caribbean nations; \
     and it is an official language of almost 60 sovereign states. It is the third-most-common \
     native language in the world, after Mandarin Chinese and Spanish. It is widely learned as \
     a second language and is an official language of the European Union, many Commonwealth \
     countries and the United Nations, as well as in many world organisations.";

/// Some Korean text to test.
//
// the first paragraphs of the article "Korean Language" from Korean Wikipedia.
// https://ko.wikipedia.org/w/index.php?title=%ED%95%9C%EA%B5%AD%EC%96%B4&oldid=12331875
pub static KOREAN_TEXT: &'static str =
    "한국어(韓國語)는 주로 한반도(韓半島)와 한민족(韓民族) 거주 지역에서 쓰이는 언어로, \
     대한민국에서는 한국어, 한국말이라고 부르고, 조선민주주의인민공화국과 중국, 일본에서는 \
     조선어(朝鮮語), 조선말이라고 불린다. 우즈베키스탄, 러시아 등 구 소련의 고려인들 사이에서는 \
     고려말(高麗語)로 불린다. 19세기 중반 이후 한반도와 주변 정세의 혼란, 20세기 전반 \
     일본 제국주의의 침략, 20세기 후반 대한민국의 해외 이민에 의해 중국 동북 지방, 일본, \
     러시아 연해주와 사할린, 우즈베키스탄, 미국, 캐나다, 오스트레일리아, 필리핀, 베트남, 브라질 등 \
     세계 곳곳에 한민족이 이주하면서 한국어가 쓰이고 있다. 한국어 쓰는 인구는 전 세계를 통틀어 \
     약 8천250만 명으로 추산된다.";

/// Some Japanese text to test.
//
// the first paragraphs of the article "Japanese Language" from Japanese Wikipedia.
// https://ja.wikipedia.org/w/index.php?title=%E6%97%A5%E6%9C%AC%E8%AA%9E&oldid=51443986
pub static JAPANESE_TEXT: &'static str =
    "日本語（にほんご、にっぽんご）とは、主に日本国内や日本人同士の間で使われている言語である。\
     日本は法令によって「公用語」を規定していないが、法令その他の公用文は日本語で記述され、\
     各種法令（裁判所法第74条、会社計算規則第57条、特許法施行規則第2条など）において\
     日本語を用いることが定められるなど事実上の公用語となっており、学校教育の「国語」でも\
     教えられる。使用人口について正確な統計はないが、日本国内の人口、および日本国外に住む\
     日本人や日系人、日本がかつて統治した地域の一部の住民など、約1億3千万人以上と考えられる。\
     統計によって前後する可能性はあるが、この数は世界の母語話者数で上位10位以内に入る人数である。";

/// Some simplified Chinese text to test.
//
// the first paragraphs of the article "Chinese Language" from Chinese Wikipedia.
// https://zh.wikipedia.org/w/index.php?title=%E6%B1%89%E8%AF%AD&variant=zh-cn&oldid=31224104
pub static SIMPLIFIED_CHINESE_TEXT: &'static str =
    "汉语，又称中文、华语（东南亚）、国语（中华民国国语）、中国语（日本、韩国等），\
     其他名称有汉文（通常指文言文）、华文、唐文、唐话、中国话等，是属汉藏语系的分析语，具有声调。\
     汉语的文字系统——汉字是一种意音文字，表意的同时也具一定的表音功能。\
     汉语包含书面语以及口语两部分，古代书面汉语称为文言文，现代书面汉语一般指使用现代标准汉语语法，\
     词汇的中文通行文体。目前全球有六分之一人口使用汉语作为母语。现代汉语书面语高度统一，\
     口语则有官话、粤语、吴语、湘语、赣语、客家语、闽语等七种主要汉语言\
     （也有人认为晋语和（或）徽语和（或）平话（广西平话）也应为独立汉语言，\
     也有其他人认为闽语其实是一个语族，下辖闽南语、闽东语、闽中语以及莆仙语，\
     国际标准化组织即持此观点，部分资料将其中的一至六种也算成单独的汉语言，\
     这就是八至十三种汉语言的由来）。";

/// Some traditional Chinese text to test.
//
// the first paragraphs of the article "Chinese Language" from Chinese Wikipedia.
// https://zh.wikipedia.org/w/index.php?title=%E6%B1%89%E8%AF%AD&variant=zh-tw&oldid=31224104
pub static TRADITIONAL_CHINESE_TEXT: &'static str =
    "漢語，又稱中文、華語（東南亞）、國語（中華民國國語）、中國語（日本、韓國等），\
     其他名稱有漢文（通常指文言文）、華文、唐文、唐話、中國話等，是屬漢藏語系的分析語，具有聲調。\
     漢語的文字系統——漢字是一種意音文字，表意的同時也具一定的表音功能。\
     漢語包含書面語以及口語兩部分，古代書面漢語稱為文言文，現代書面漢語一般指使用現代標準漢語語法，\
     詞彙的中文通行文體。目前全球有六分之一人口使用漢語作為母語。現代漢語書面語高度統一，\
     口語則有官話、粵語、吳語、湘語、贛語、客家語、閩語等七種主要漢語言\
     （也有人認為晉語和（或）徽語和（或）平話（廣西平話）也應為獨立漢語言，\
     也有其他人認為閩語其實是一個語族，下轄閩南語、閩東語、閩中語以及莆仙語，\
     國際標準化組織即持此觀點，部分資料將其中的一至六種也算成單獨的漢語言，\
     這就是八至十三種漢語言的由來）。";

/// Some text with various invalid UTF-8 sequences.
//
// Markus Kuhn's UTF-8 decoder capability and stress test.
// http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
pub static INVALID_UTF8_TEXT: &'static [u8] = include_bytes!("examples/UTF-8-test.txt");

/// Returns a longer text used for external data benchmarks.
/// This can be overriden with an environment variable `EXTERNAL_BENCH_DATA`,
/// or it will use a built-in sample data (of about 100KB).
pub fn get_external_bench_data() -> Vec<u8> {
    use std::env;
    use std::io::Read;
    use std::fs::File;
    use std::path::Path;

    // An HTML file derived from the Outer Space Treaty of 1967, in six available languages.
    // http://www.unoosa.org/oosa/SpaceLaw/outerspt.html
    static LONGER_TEXT: &'static [u8] = include_bytes!("examples/outer-space-treaty.html");

    match env::var("EXTERNAL_BENCH_DATA") {
        Ok(path) => {
            let path = Path::new(&path);
            let mut file = File::open(&path).ok().expect("cannot read an external bench data");
            let mut ret = Vec::new();
            file.read_to_end(&mut ret).ok().expect("cannot read an external bench data");
            ret
        }
        Err(..) => {
            LONGER_TEXT.to_vec()
        }
    }
}


#![feature(test)]

extern crate test;

mod map {
    use std::collections::{BTreeMap, HashMap};
    use test::Bencher;
    use phf::phf_map;

    macro_rules! map_and_match {
        ($map:ident, $f:ident, $($key:expr => $value:expr,)+) => {
            static $map: phf::Map<&'static str, usize> = phf_map! {
                $($key => $value),+
            };

            fn $f(key: &str) -> Option<usize> {
                match key {
                    $($key => Some($value),)+
                    _ => None
                }
            }
        }
    }

    map_and_match! { MAP, match_get,
        "apple" => 0,
        "banana" => 1,
        "carrot" => 2,
        "doughnut" => 3,
        "eggplant" => 4,
        "frankincene" => 5,
        "grapes" => 6,
        "haggis" => 7,
        "ice cream" => 8,
        "jelly beans" => 9,
        "kaffir lime leaves" => 10,
        "lemonade" => 11,
        "mashmallows" => 12,
        "nectarines" => 13,
        "oranges" => 14,
        "pineapples" => 15,
        "quinoa" => 16,
        "rosemary" => 17,
        "sourdough" => 18,
        "tomatoes" => 19,
        "unleavened bread" => 20,
        "vanilla" => 21,
        "watermelon" => 22,
        "xinomavro grapes" => 23,
        "yogurt" => 24,
        "zucchini" => 25,
    }

    #[bench]
    fn bench_match_some(b: &mut Bencher) {
        b.iter(|| {
            assert_eq!(match_get("zucchini").unwrap(), 25);
        })
    }

    #[bench]
    fn bench_match_none(b: &mut Bencher) {
        b.iter(|| {
            assert_eq!(match_get("potato"), None);
        })
    }

    #[bench]
    fn bench_btreemap_some(b: &mut Bencher) {
        let mut map = BTreeMap::new();
        for (key, value) in MAP.entries() {
            map.insert(*key, *value);
        }

        b.iter(|| {
            assert_eq!(map.get("zucchini").unwrap(), &25);
        })
    }

    #[bench]
    fn bench_hashmap_some(b: &mut Bencher) {
        let mut map = HashMap::new();
        for (key, value) in MAP.entries() {
            map.insert(*key, *value);
        }

        b.iter(|| {
            assert_eq!(map.get("zucchini").unwrap(), &25);
        })
    }

    #[bench]
    fn bench_phf_some(b: &mut Bencher) {
        b.iter(|| {
            assert_eq!(MAP.get("zucchini").unwrap(), &25);
        })
    }

    #[bench]
    fn bench_btreemap_none(b: &mut Bencher) {
        let mut map = BTreeMap::new();
        for (key, value) in MAP.entries() {
            map.insert(*key, *value);
        }

        b.iter(|| {
            assert_eq!(map.get("potato"), None);
        })
    }


    #[bench]
    fn bench_hashmap_none(b: &mut Bencher) {
        let mut map = HashMap::new();
        for (key, value) in MAP.entries() {
            map.insert(*key, *value);
        }

        b.iter(|| {
            assert_eq!(map.get("potato"), None);
        })
    }

    #[bench]
    fn bench_phf_none(b: &mut Bencher) {
        b.iter(|| {
            assert_eq!(MAP.get("potato"), None);
        })
    }
}

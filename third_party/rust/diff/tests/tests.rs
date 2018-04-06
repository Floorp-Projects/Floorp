#![feature(plugin)]
#![plugin(speculate)]

extern crate diff;
extern crate quickcheck;

use diff::Result::*;

pub fn undiff<T: Clone>(diff: &[::diff::Result<&T>]) -> (Vec<T>, Vec<T>) {
    let (mut left, mut right) = (vec![], vec![]);
    for d in diff {
        match *d {
            Left(l) => left.push(l.clone()),
            Both(l, r) => {
                left.push(l.clone());
                right.push(r.clone());
            }
            Right(r) => right.push(r.clone()),
        }
    }
    (left, right)
}

pub fn undiff_str<'a>(diff: &[::diff::Result<&'a str>]) -> (Vec<&'a str>, Vec<&'a str>) {
    let (mut left, mut right) = (vec![], vec![]);
    for d in diff {
        match *d {
            Left(l) => left.push(l),
            Both(l, r) => {
                left.push(l);
                right.push(r);
            }
            Right(r) => right.push(r),
        }
    }
    (left, right)
}

pub fn undiff_chars(diff: &[::diff::Result<char>]) -> (String, String) {
    let (mut left, mut right) = (vec![], vec![]);
    for d in diff {
        match *d {
            Left(l) => left.push(l),
            Both(l, r) => {
                left.push(l);
                right.push(r);
            }
            Right(r) => right.push(r),
        }
    }
    (
        left.iter().cloned().collect(),
        right.iter().cloned().collect(),
    )
}

speculate! {
    describe "slice" {
        fn go<T>(left: &[T], right: &[T], len: usize) where
            T: Clone + ::std::fmt::Debug + PartialEq
        {
            let diff = ::diff::slice(&left, &right);
            assert_eq!(diff.len(), len);
            let (left_, right_) = undiff(&diff);
            assert_eq!(left, &left_[..]);
            assert_eq!(right, &right_[..]);
        }

        test "empty slices" {
            let slice: &[()] = &[];
            go(&slice, &slice, 0);
        }

        test "equal + non-empty slices" {
            let slice = [1, 2, 3];
            go(&slice, &slice, 3);
        }

        test "left empty, right non-empty" {
            let slice = [1, 2, 3];
            go(&slice, &[], 3);
        }

        test "left non-empty, right empty" {
            let slice = [1, 2, 3];
            go(&[], &slice, 3);
        }

        test "misc 1" {
            let left = [1, 2, 3, 4, 1, 3];
            let right = [1, 4, 1, 1];
            go(&left, &right, 7);
        }

        test "misc 2" {
            let left = [1, 2, 1, 2, 3, 2, 2, 3, 1, 3];
            let right = [3, 3, 1, 2, 3, 1, 2, 3, 4, 1];
            go(&left, &right, 14);
        }

        test "misc 3" {
            let left = [1, 3, 4];
            let right = [2, 3, 4];
            go(&left, &right, 4);
        }

        test "quickcheck" {
            fn prop(left: Vec<i32>, right: Vec<i32>) -> bool {
                let diff = ::diff::slice(&left, &right);
                let (left_, right_) = undiff(&diff);
                left == &left_[..] && right == &right_[..]
            }

            ::quickcheck::quickcheck(prop as fn(Vec<i32>, Vec<i32>) -> bool);
        }
    }

    describe "lines" {
        fn go(left: &str, right: &str, len: usize) {
            let diff = ::diff::lines(&left, &right);
            assert_eq!(diff.len(), len);
            let (left_, right_) = undiff_str(&diff);
            assert_eq!(left, left_.join("\n"));
            assert_eq!(right, right_.join("\n"));
        }

        test "both empty" {
            go("", "", 0);
        }

        test "one empty" {
            go("foo", "", 1);
            go("", "foo", 1);
        }

        test "both equal and non-empty" {
            go("foo\nbar", "foo\nbar", 2);
        }

        test "misc 1" {
            go("foo\nbar\nbaz", "foo\nbaz\nquux", 4);
        }

        test "#10" {
            go("a\nb\nc", "a\nb\nc\n", 4);
            go("a\nb\nc\n", "a\nb\nc", 4);
            let left = "a\nb\n\nc\n\n\n";
            let right = "a\n\n\nc\n\n";
            go(left, right, 8);
            go(right, left, 8);
        }
    }

    describe "chars" {
        fn go(left: &str, right: &str, len: usize) {
            let diff = ::diff::chars(&left, &right);
            assert_eq!(diff.len(), len);
            let (left_, right_) = undiff_chars(&diff);
            assert_eq!(left, left_);
            assert_eq!(right, right_);
        }

        test "both empty" {
            go("", "", 0);
        }

        test "one empty" {
            go("foo", "", 3);
            go("", "foo", 3);
        }

        test "both equal and non-empty" {
            go("foo bar", "foo bar", 7);
        }

        test "misc 1" {
            go("foo bar baz", "foo baz quux", 16);
        }
    }

    describe "issues" {
        test "#4" {
            assert_eq!(::diff::slice(&[1], &[2]), vec![Left(&1),
                                                       Right(&2)]);
            assert_eq!(::diff::lines("a", "b"), vec![Left("a"),
                                                     Right("b")]);
        }

        test "#6" {
            // This produced an overflow in the lines computation because it
            // was not accounting for the fact that the "right" length was
            // less than the "left" length.
            let expected = r#"
BacktraceNode {
    parents: [
        BacktraceNode {
            parents: []
        },
        BacktraceNode {
            parents: [
                BacktraceNode {
                    parents: []
                }
            ]
        }
    ]
}"#;
            let actual = r#"
BacktraceNode {
    parents: [
        BacktraceNode {
            parents: []
        },
        BacktraceNode {
            parents: [
                BacktraceNode {
                    parents: []
                },
                BacktraceNode {
                    parents: []
                }
            ]
        }
    ]
}"#;
            ::diff::lines(actual, expected);
        }
    }
}

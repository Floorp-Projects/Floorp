#![deny(clippy::all, clippy::pedantic)]
#![allow(clippy::option_if_let_else)]

use std::fmt::Display;
use thiserror::Error;

// Some of the elaborate cases from the rcc codebase, which is a C compiler in
// Rust. https://github.com/jyn514/rcc/blob/0.8.0/src/data/error.rs
#[derive(Error, Debug)]
pub enum CompilerError {
    #[error("cannot shift {} by {maximum} or more bits (got {current})", if *.is_left { "left" } else { "right" })]
    TooManyShiftBits {
        is_left: bool,
        maximum: u64,
        current: u64,
    },

    #[error("#error {}", (.0).iter().copied().collect::<Vec<_>>().join(" "))]
    User(Vec<&'static str>),

    #[error("overflow while parsing {}integer literal",
        if let Some(signed) = .is_signed {
            if *signed { "signed "} else { "unsigned "}
        } else {
            ""
        }
    )]
    IntegerOverflow { is_signed: Option<bool> },

    #[error("overflow while parsing {}integer literal", match .is_signed {
        Some(true) => "signed ",
        Some(false) => "unsigned ",
        None => "",
    })]
    IntegerOverflow2 { is_signed: Option<bool> },
}

// Examples drawn from Rustup.
#[derive(Error, Debug)]
pub enum RustupError {
    #[error(
        "toolchain '{name}' does not contain component {component}{}",
        .suggestion
            .as_ref()
            .map_or_else(String::new, |s| format!("; did you mean '{}'?", s)),
    )]
    UnknownComponent {
        name: String,
        component: String,
        suggestion: Option<String>,
    },
}

fn assert<T: Display>(expected: &str, value: T) {
    assert_eq!(expected, value.to_string());
}

#[test]
fn test_rcc() {
    assert(
        "cannot shift left by 32 or more bits (got 50)",
        CompilerError::TooManyShiftBits {
            is_left: true,
            maximum: 32,
            current: 50,
        },
    );

    assert("#error A B C", CompilerError::User(vec!["A", "B", "C"]));

    assert(
        "overflow while parsing signed integer literal",
        CompilerError::IntegerOverflow {
            is_signed: Some(true),
        },
    );
}

#[test]
fn test_rustup() {
    assert(
        "toolchain 'nightly' does not contain component clipy; did you mean 'clippy'?",
        RustupError::UnknownComponent {
            name: "nightly".to_owned(),
            component: "clipy".to_owned(),
            suggestion: Some("clippy".to_owned()),
        },
    );
}

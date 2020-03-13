extern crate glsl;

use glsl::parser::Parse;
use glsl::syntax::TranslationUnit;

#[test]
fn missing_zero_float_is_valid() {
  let r = TranslationUnit::parse(
    "
    void main() {
      float x = 1. * .5;
    }",
  );

  assert!(r.is_ok());
}

#[test]
fn float_exp_is_valid() {
  let r = TranslationUnit::parse(
    "
    void main() {
      float x = 1e-5;
    }",
  );

  assert!(r.is_ok());
}

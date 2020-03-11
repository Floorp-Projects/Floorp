use glsl::parser::Parse;
use glsl::syntax;

#[test]
fn incorrect_statement() {
  let r = syntax::TranslationUnit::parse(
    "
    int fetch_transform(int id) {
      return id;
    }

    bool ray_plane() {
      if 1 {
    }
    ",
  );

  assert!(r.is_err());
}

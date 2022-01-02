extern crate version_sync;

use version_sync::{
    assert_contains_regex, assert_html_root_url_updated, assert_markdown_deps_updated,
};

#[test]
fn test_readme_deps() {
    assert_markdown_deps_updated!("README.md");
}

#[test]
fn test_readme_deps_in_lib() {
    assert_contains_regex!("src/lib.rs", r#"^//! version = "{version}""#);
}

#[test]
fn test_html_root_url() {
    assert_html_root_url_updated!("src/lib.rs");
}

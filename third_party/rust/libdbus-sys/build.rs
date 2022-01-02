extern crate pkg_config;

fn main() {
    // See https://github.com/joshtriplett/metadeps/issues/9 for why we don't use
    // metadeps here, but instead keep this manually in sync with Cargo.toml.
    pkg_config::Config::new().atleast_version("1.6").probe("dbus-1").unwrap();
}

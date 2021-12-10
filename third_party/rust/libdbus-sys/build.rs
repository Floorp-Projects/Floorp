extern crate pkg_config;

fn main() {
    // See https://github.com/joshtriplett/metadeps/issues/9 for why we don't use
    // metadeps here, but instead keep this manually in sync with Cargo.toml.
    if let Err(e) = pkg_config::Config::new().atleast_version("1.6").probe("dbus-1") {
        eprintln!("pkg_config failed: {}", e);
        eprintln!(
            "One possible solution is to check whether packages\n\
            'libdbus-1-dev' and 'pkg-config' are installed:\n\
            On Ubuntu:\n\
            sudo apt install libdbus-1-dev pkg-config\n\
            On Fedora:\n\
            sudo dnf install dbus-devel pkgconf-pkg-config\n"
        );
        panic!();
    }
}

fn main() {
    if std::env::var_os("CARGO_CFG_UNIX").is_some() {
        cc::Build::new()
            .file("src/sys/unix/cmsghdr.c")
            .compile("cmsghdr");
    }
}

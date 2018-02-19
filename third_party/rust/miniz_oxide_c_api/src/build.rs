#[cfg(any(feature = "build_stub_miniz", feature = "build_orig_miniz"))]
extern crate cc;

#[cfg(not(any(feature = "build_stub_miniz", feature = "build_orig_miniz")))]
fn main() {}

#[cfg(feature = "build_stub_miniz")]
fn main() {
    cc::Build::new()
        .files(
            &[
                "miniz_stub/miniz.c",
                "miniz_stub/miniz_zip.c",
                "miniz_stub/miniz_tinfl.c",
                "miniz_stub/miniz_tdef.c",
            ],
        )
        .compile("miniz");
}

#[cfg(feature = "build_orig_miniz")]
fn main() {
    cc::Build::new()
        .files(
            &[
                "miniz/miniz.c",
                "miniz/miniz_zip.c",
                "miniz/miniz_tinfl.c",
                "miniz/miniz_tdef.c",
            ],
        )
        .compile("miniz");
}

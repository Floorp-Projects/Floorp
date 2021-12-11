# Runs all examples.

# FIXME: https://github.com/rust-lang-nursery/packed_simd/issues/55
# All examples fail to build for `armv7-apple-ios`.
if [[ ${TARGET} == "armv7-apple-ios" ]]; then
    exit 0
fi

# FIXME: travis exceeds 50 minutes on these targets
# Skipping the examples is an attempt at preventing travis from timing-out
if [[ ${TARGET} == "arm-linux-androidabi" ]] || [[ ${TARGET} == "aarch64-linux-androidabi" ]] \
    || [[ ${TARGET} == "sparc64-unknown-linux-gnu" ]]; then
    exit 0
fi

if [[ ${TARGET} == "wasm32-unknown-unknown" ]]; then
    exit 0
fi

cp -r examples/aobench target/aobench
cargo_test --manifest-path=target/aobench/Cargo.toml --release --no-default-features
cargo_test --manifest-path=target/aobench/Cargo.toml --release --features=256bit

cp -r examples/dot_product target/dot_product
cargo_test --manifest-path=target/dot_product/Cargo.toml --release

cp -r examples/fannkuch_redux target/fannkuch_redux
cargo_test --manifest-path=target/fannkuch_redux/Cargo.toml --release

# FIXME: https://github.com/rust-lang-nursery/packed_simd/issues/56
if [[ ${TARGET} != "i586-unknown-linux-gnu" ]]; then
    cp -r examples/mandelbrot target/mandelbrot
    cargo_test --manifest-path=target/mandelbrot/Cargo.toml --release
fi

cp -r examples/matrix_inverse target/matrix_inverse
cargo_test --manifest-path=target/matrix_inverse/Cargo.toml --release

cp -r examples/nbody target/nbody
cargo_test --manifest-path=target/nbody/Cargo.toml --release

cp -r examples/spectral_norm target/spectral_norm
cargo_test --manifest-path=target/spectral_norm/Cargo.toml --release

if [[ ${TARGET} != "i586-unknown-linux-gnu" ]]; then
    cp -r examples/stencil target/stencil
    cargo_test --manifest-path=target/stencil/Cargo.toml --release
fi

cp -r examples/triangle_xform target/triangle_xform
cargo_test --manifest-path=target/triangle_xform/Cargo.toml --release

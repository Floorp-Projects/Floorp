set -ex

main() {
    cross test --target $TARGET
    cross test --target $TARGET --release

    [ "$TRAVIS_RUST_VERSION" -eq "nightly" ] && cross test --feature x128 --target $TARGET
    [ "$TRAVIS_RUST_VERSION" -eq "nightly" ] && cross test --feature x128 --target $TARGET --release

    cross test --no-default-features --target $TARGET
    cross test --no-default-features --target $TARGET --release
}

main

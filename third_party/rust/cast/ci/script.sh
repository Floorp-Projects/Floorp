set -euxo pipefail

main() {
    # not MSRV
    if [ $TRAVIS_RUST_VERSION != 1.13.0 ]; then
        cargo check --target $TARGET --no-default-features

        cargo test --features x128 --target $TARGET
        cargo test --features x128 --target $TARGET --release
    else
        cargo build --target $TARGET --no-default-features
        cargo build --target $TARGET
    fi
}

# fake Travis variables to be able to run this on a local machine
if [ -z ${TRAVIS_BRANCH-} ]; then
    TRAVIS_BRANCH=staging
fi

if [ -z ${TRAVIS_PULL_REQUEST-} ]; then
    TRAVIS_PULL_REQUEST=false
fi

if [ -z ${TRAVIS_RUST_VERSION-} ]; then
    case $(rustc -V) in
        *nightly*)
            TRAVIS_RUST_VERSION=nightly
            ;;
        *beta*)
            TRAVIS_RUST_VERSION=beta
            ;;
        *)
            TRAVIS_RUST_VERSION=stable
            ;;
    esac
fi

if [ -z ${TARGET-} ]; then
    TARGET=$(rustc -Vv | grep host | cut -d ' ' -f2)
fi

main

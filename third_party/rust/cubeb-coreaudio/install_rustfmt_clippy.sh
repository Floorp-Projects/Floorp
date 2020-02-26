# https://github.com/rust-lang/rustup-components-history/blob/master/README.md
# https://github.com/rust-lang/rust-clippy/blob/27acea0a1baac6cf3ac6debfdbce04f91e15d772/.travis.yml#L40-L46
if ! rustup component add rustfmt; then
    TARGET=$(rustc -Vv | awk '/host/{print $2}')
    NIGHTLY=$(curl -s "https://rust-lang.github.io/rustup-components-history/${TARGET}/rustfmt")
    curl -sSL "https://static.rust-lang.org/dist/${NIGHTLY}/rustfmt-nightly-${TARGET}.tar.xz" | \
    tar -xJf - --strip-components=3 -C ~/.cargo/bin
    rm -rf ~/.cargo/bin/doc
fi

if ! rustup component add clippy; then
    TARGET=$(rustc -Vv | awk '/host/{print $2}')
    NIGHTLY=$(curl -s "https://rust-lang.github.io/rustup-components-history/${TARGET}/clippy")
    rustup default nightly-${NIGHTLY}
    rustup component add rustfmt
    rustup component add clippy
fi

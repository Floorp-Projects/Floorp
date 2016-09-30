set -ex

print_versions() {
    rustc -V
    cargo -V
}

rustup_install() {
    export PATH="$PATH:$HOME/.cargo/bin"
    curl https://sh.rustup.rs -sSf | sh -s -- -y --default-toolchain=$1
}

# Add provided target to current Rust toolchain if it is not already
# the default or installed.
rustup_target_add() {
    if ! rustup target list | grep -E "$1 \((default|installed)\)"
    then
        rustup target add $1
    fi
}

setup_docker() {
    apt-get -qq -y install zip
    cd /mnt/host
}

# Configure rustc target for cross compilation.  Provided with a build
# target, this will determine which linker to use for cross compilation.
cargo_config() {
    local prefix

    case "$TARGET" in
    aarch64-unknown-linux-gnu)
        prefix=aarch64-linux-gnu
        ;;
    arm*-unknown-linux-gnueabihf)
        prefix=arm-linux-gnueabihf
        ;;
    arm-unknown-linux-gnueabi)
        prefix=arm-linux-gnueabi
        ;;
    mipsel-unknown-linux-musl)
        prefix=mipsel-openwrt-linux
        ;;
    x86_64-pc-windows-gnu)
        prefix=x86_64-w64-mingw32
        ;;
    i686-pc-windows-gnu)
        prefix=i686-w64-mingw32
        ;;
    *)
        return
        ;;
    esac

    mkdir -p ~/.cargo
    cat >~/.cargo/config <<EOF
[target.$TARGET]
linker = "$prefix-gcc"
EOF

    cat ~/.cargo/config
}

# Build current crate for given target and print file type information.
# If the second argument is set, a release build will be made.
cargo_build() {
    local mode
    if [ -z "$2" ]
    then
        mode=debug
    else
        mode=release
    fi

    local modeflag
    if [ "$mode" == "release" ]
    then
        modeflag=--release
    fi

    cargo build --target $1 $modeflag

    file $(get_binary $1 $mode)
}

# Run current crate's tests if the current system supports it.
cargo_test() {
    if echo "$1" | grep -E "(i686|x86_64)-unknown-linux-(gnu|musl)|darwin"
    then
        cargo test --target $1
    fi
}

# Returns relative path to binary
# based on build target and type ("release"/"debug").
get_binary() {
    local ext
    if [[ "$1" =~ "windows" ]]
    then
        ext=".exe"
    fi
    echo "target/$1/$2/geckodriver$ext"
}

# Create a compressed archive of the binary
# for the given given git tag, build target, and build type.
package_binary() {
    local bin
    bin=$(get_binary $2 $4)
    cp $bin .

    if [[ "$2" =~ "windows" ]]
    then
        filename="geckodriver-$1-$3.zip"
        zip "$filename" geckodriver.exe
        file "$filename"
    else
        filename="geckodriver-$1-$3.tar.gz"
        tar zcvf "$filename" geckodriver
        file "$filename"
    fi
    if [ ! -z "$USE_DOCKER" ]
    then
        chown "$USER_ID:$GROUP_ID" "$filename"
    fi
}

main() {
    TOOLCHAIN=${TOOLCHAIN:=beta}

    if [ ! -z "$USE_DOCKER" ]
    then
        setup_docker
        print_versions
    else
        rustup_install $TOOLCHAIN
        print_versions
        rustup_target_add $TARGET
    fi

    cargo_config $TARGET
    cargo_build $TARGET
    cargo_test $TARGET

    # when something is tagged,
    # also create a release build and package it
    if [ ! -z "$TRAVIS_TAG" ]
    then
        cargo_build $TARGET 1
        package_binary $TRAVIS_TAG $TARGET $NAME "release"
    fi
}

main

set -ex

# Add provided target to current Rust toolchain if it is not already
# the default or installed.
rustup_target_add() {
	if ! rustup target list | grep -E "$1 \((default|installed)\)"
	then
		rustup target add $1
	fi
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
	*)
		return
		;;
	esac

	mkdir -p ~/.cargo
	cat >>~/.cargo/config <<EOF
[target.$TARGET]
linker = "$prefix-gcc"
EOF
}

# Build current crate and print file type information.
cargo_build() {
	cargo build --target $1

	if [[ "$1" =~ "windows" ]]
	then
		file target/$1/debug/geckodriver.exe
	else
		file target/$1/debug/geckodriver
	fi
}

# Run current crate's tests if the current system supports it.
cargo_test() {
	if echo "$1" | grep -E "(i686|x86_64)-unknown-linux-(gnu|musl)"
	then
		cargo test --target $1
	else
		>&2 echo "not running tests on $1"
	fi
}

main() {
	rustup_target_add $TARGET
	cargo_config $TARGET
	cargo_build $TARGET
	cargo_test $TARGET
}

main

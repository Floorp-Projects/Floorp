# Arguments: fuzz target-id, fuzzer name
fuzzer_target="$1"
fuzzer_binary="$2"

ORG_NAME="image-rs"
FUZZIT_VERSION=2.4.46

# Get cargo-fuzz
cargo install cargo-fuzz
# Build the fuzzer binary
cargo fuzz run "$fuzzer_binary" -- -runs=0

wget -O fuzzit "https://github.com/fuzzitdev/fuzzit/releases/download/v$FUZZIT_VERSION/fuzzit_Linux_x86_64"
chmod a+x fuzzit
./fuzzit --version
./fuzzit create job --type local-regression "$ORG_NAME/$fuzzer_target" "./fuzz/target/x86_64-unknown-linux-gnu/debug/$fuzzer_binary"

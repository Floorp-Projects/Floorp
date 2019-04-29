################################################################################
#                                   Justfile                                   #
#                                                                              #
# Set of routines to execute for development work. As of 2019-03-31, `just`    #
# does not work on Windows.                                                    #
################################################################################

# Cargo features
features = "serdes,std,testing"

# Builds the library.
build:
	cargo build --features {{features}}
	cargo build --features {{features}} --example sieve
	cargo build --features {{features}} --example tour
	cargo build --features {{features}} --example serdes
	cargo build --features {{features}} --example readme

# Checks the library for syntax and HIR errors.
check:
	cargo check --features {{features}}

# Runs all of the recipes necessary for pre-publish.
checkout: check clippy build doc test package

# Continually runs the development routines.
ci:
	just loop dev

# Removes all build artifacts.
clean:
	cargo clean

# Runs clippy.
clippy: check
	cargo clippy --features {{features}}

# Runs the development routines.
dev: clippy doc test

# Builds the crate documentation.
doc:
	cargo doc --features {{features}} --document-private-items

# Continually runs some recipe from this file.
loop action:
	cargo watch -s "just {{action}}"

# Packages the crate in preparation for publishing on crates.io
package:
	cargo package --allow-dirty

# Publishes the crate to crates.io
publish: checkout
	cargo publish

# Runs the test suites.
test: check
	cargo test --features {{features}}

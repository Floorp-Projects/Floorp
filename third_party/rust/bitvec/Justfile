checkout:
	cargo check
	cargo doc --features testing --document-private-items
	cargo build
	cargo build --example sieve
	cargo build --example tour
	cargo test --features testing
	cargo package --allow-dirty

dev:
	# cargo check
	cargo test --no-default-features --features testing
	cargo doc --features testing --document-private-items

ci:
	watchexec -- just dev

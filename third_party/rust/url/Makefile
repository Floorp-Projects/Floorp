test:
	cargo test --features "query_encoding serde rustc-serialize heapsize"
	(cd idna && cargo test)
	(cd url_serde && cargo test)

.PHONY: test

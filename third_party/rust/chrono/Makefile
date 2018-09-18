# this Makefile is mostly for the packaging convenience.
# casual users should use `cargo` to retrieve the appropriate version of Chrono.

.PHONY: all
all:
	@echo 'Try `cargo build` instead.'

.PHONY: authors
authors:
	echo 'Chrono is mainly written by Kang Seonghoon <public+rust@mearie.org>,' > AUTHORS.txt
	echo 'and also the following people (in ascending order):' >> AUTHORS.txt
	echo >> AUTHORS.txt
	git log --format='%aN <%aE>' | grep -v 'Kang Seonghoon' | sort -u >> AUTHORS.txt

.PHONY: readme README.md
readme: README.md

README.md: src/lib.rs
	( ./ci/fix-readme.sh $< ) > $@

.PHONY: test
test:
	TZ=UTC0 cargo test --features 'serde rustc-serialize bincode' --lib
	TZ=ACST-9:30 cargo test --features 'serde rustc-serialize bincode' --lib
	TZ=EST4 cargo test --features 'serde rustc-serialize bincode'

.PHONY: doc
doc: authors readme
	cargo doc --features 'serde rustc-serialize bincode'


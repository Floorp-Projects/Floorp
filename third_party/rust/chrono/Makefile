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

.PHONY: readme
readme: README.md

README.md: src/lib.rs
	# really, really sorry for this mess.
	awk '/^\/\/! # Chrono /{print "[Chrono][doc]",$$4}' $< > $@
	awk '/^\/\/! # Chrono /{print "[Chrono][doc]",$$4}' $< | sed 's/./=/g' >> $@
	echo >> $@
	echo '[![Chrono on Travis CI][travis-image]][travis]' >> $@
	echo >> $@
	echo '[travis-image]: https://travis-ci.org/lifthrasiir/rust-chrono.png' >> $@
	echo '[travis]: https://travis-ci.org/lifthrasiir/rust-chrono' >> $@
	awk '/^\/\/! # Chrono /,/^\/\/! ## /' $< | cut -b 5- | grep -v '^#' | \
		sed 's/](\.\//](https:\/\/lifthrasiir.github.io\/rust-chrono\/chrono\//g' >> $@
	echo '[Complete Documentation][doc]' >> $@
	echo >> $@
	echo '[doc]: https://lifthrasiir.github.io/rust-chrono/' >> $@
	echo >> $@
	awk '/^\/\/! ## /,!/^\/\/!/' $< | cut -b 5- | grep -v '^# ' | \
		sed 's/](\.\//](https:\/\/lifthrasiir.github.io\/rust-chrono\/chrono\//g' >> $@

.PHONY: test
test:
	cargo test --features 'serde rustc-serialize'

.PHONY: doc
doc: authors readme
	cargo doc --features 'serde rustc-serialize'

.PHONY: doc-publish
doc-publish: doc
	( \
		PKGID="$$(cargo pkgid)"; \
		PKGNAMEVER="$${PKGID#*#}"; \
		PKGNAME="$${PKGNAMEVER%:*}"; \
		REMOTE="$$(git config --get remote.origin.url)"; \
		cd target/doc && \
		rm -rf .git && \
		git init && \
		git checkout --orphan gh-pages && \
		echo '<!doctype html><html><head><meta http-equiv="refresh" content="0;URL='$$PKGNAME'/index.html"></head><body></body></html>' > index.html && \
		git add . && \
		git commit -m 'updated docs.' && \
		git push "$$REMOTE" gh-pages -f; \
	)


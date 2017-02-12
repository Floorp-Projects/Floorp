.PHONY: all
all:
	@echo 'Try `cargo build` instead.'

.PHONY: authors
authors:
	echo 'Encoding is mainly written by Kang Seonghoon <public+rust@mearie.org>,' > AUTHORS.txt
	echo 'and also the following people (in ascending order):' >> AUTHORS.txt
	echo >> AUTHORS.txt
	git log --format='%aN <%aE>' | grep -v 'Kang Seonghoon' | sort -u >> AUTHORS.txt

.PHONY: test
test:
	cargo test -v
	cargo test -v -p encoding-index-singlebyte
	cargo test -v -p encoding-index-korean
	cargo test -v -p encoding-index-japanese
	cargo test -v -p encoding-index-simpchinese
	cargo test -v -p encoding-index-tradchinese

.PHONY: readme
readme: README.md

README.md: src/lib.rs
	# really, really sorry for this mess.
	awk '/^\/\/! # Encoding /{print "[Encoding][doc]",$$4}' $< > $@
	awk '/^\/\/! # Encoding /{print "[Encoding][doc]",$$4}' $< | sed 's/./=/g' >> $@
	echo >> $@
	echo '[![Encoding on Travis CI][travis-image]][travis]' >> $@
	echo >> $@
	echo '[travis-image]: https://travis-ci.org/lifthrasiir/rust-encoding.png' >> $@
	echo '[travis]: https://travis-ci.org/lifthrasiir/rust-encoding' >> $@
	awk '/^\/\/! # Encoding /,/^\/\/! ## /' $< | cut -b 5- | grep -v '^# ' >> $@
	echo >> $@
	echo '[Complete Documentation][doc]' >> $@
	echo >> $@
	echo '[doc]: https://lifthrasiir.github.io/rust-encoding/' >> $@
	echo >> $@
	awk '/^\/\/! ## /,!/^\/\/!/' $< | cut -b 5- | grep -v '^# ' >> $@

.PHONY: doc
doc: authors readme
	cargo doc

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


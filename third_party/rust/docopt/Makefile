all:
	@echo Nothing to do

docs: $(LIB_FILES)
	cargo doc
	# WTF is rustdoc doing?
	in-dir ./target/doc fix-perms
	rscp ./target/doc/* gopher:~/www/burntsushi.net/rustdoc/

src/test/testcases.rs: src/test/testcases.docopt scripts/mk-testcases
	./scripts/mk-testcases ./src/test/testcases.docopt > ./src/test/testcases.rs

ctags:
	ctags --recurse --options=ctags.rust --languages=Rust

push:
	git push github master
	git push origin master

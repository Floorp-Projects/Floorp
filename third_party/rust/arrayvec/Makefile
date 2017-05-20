DOCCRATES = arrayvec nodrop nodrop_union odds

# deps to delete the generated docs
RMDOCS = 

FEATURES = "odds/unstable"

VERSIONS = $(patsubst %,target/VERS/%,$(DOCCRATES))

docs: mkdocs subst $(RMDOCS)

# https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html
$(VERSIONS): Cargo.toml
	mkdir -p $(@D)
	cargo pkgid $(@F) | sed -e "s/.*#\(\|.*:\)//" > "$@"

$(DOCCRATES): %: target/VERS/%
	# Put in the crate version into the docs
	find ./doc/$@ -name "*.html" -exec sed -i -e "s/<title>\(.*\) - Rust/<title>$@ $(shell cat $<) - \1 - Rust/g" {} \;

subst: $(DOCCRATES)

mkdocs: Cargo.toml
	cargo doc --features=$(FEATURES)
	cargo doc --features=use_union -p nodrop-union
	rm -rf ./doc
	cp -r ./target/doc ./doc
	-cat ./custom.css >> doc/main.css

$(RMDOCS): mkdocs
	rm -r ./doc/$@
	sed -i "/searchIndex\['$@'\]/d" doc/search-index.js


.PHONY: docs mkdocs subst $(DOCCRATES) $(RMDOCS)

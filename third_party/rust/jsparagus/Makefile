PY_OUT = js_parser/parser_tables.py
HANDLER_FILE = crates/generated_parser/src/ast_builder.rs
HANDLER_INFO_OUT = jsparagus/emit/collect_handler_info/info.json
RS_TABLES_OUT = crates/generated_parser/src/parser_tables_generated.rs
RS_AST_OUT = crates/ast/src/types_generated.rs \
	crates/ast/src/type_id_generated.rs \
	crates/ast/src/dump_generated.rs \
	crates/ast/src/visit_generated.rs \
	crates/ast/src/source_location_accessor_generated.rs \
	crates/generated_parser/src/stack_value_generated.rs

JSPARAGUS_DIR := $(dir $(firstword $(MAKEFILE_LIST)))
VENV_BIN_DIR := $(JSPARAGUS_DIR)jsparagus_build_venv/bin
PYTHON := $(VENV_BIN_DIR)/python
PIP := $(VENV_BIN_DIR)/pip

all: $(PY_OUT) rust

init-venv:
	python3 -m venv jsparagus_build_venv &&\
	$(PIP) install --upgrade pip &&\
	$(PIP) install -r requirements.txt

init: init-venv
	git config core.hooksPath .githooks

ECMA262_SPEC_HTML = ../tc39/ecma262/spec.html
STANDARD_ES_GRAMMAR_OUT = js_parser/es.esgrammar

# List of files which have a grammar_extension! Rust macro. The macro content is
# scrapped to patch the extracted grammar.
EXTENSION_FILES = \

# Incomplete list of files that contribute to the dump file.
SOURCE_FILES = $(EXTENSION_FILES) \
jsparagus/gen.py \
jsparagus/grammar.py \
jsparagus/rewrites.py \
jsparagus/lr0.py \
jsparagus/parse_table.py \
jsparagus/extension.py \
jsparagus/utils.py \
jsparagus/actions.py \
jsparagus/aps.py \
jsparagus/types.py \
js_parser/esgrammar.pgen \
js_parser/generate_js_parser_tables.py \
js_parser/parse_esgrammar.py \
js_parser/load_es_grammar.py \
js_parser/es-simplified.esgrammar

EMIT_FILES = $(SOURCE_FILES) \
jsparagus/emit/__init__.py \
jsparagus/emit/python.py \
jsparagus/emit/rust.py

DUMP_FILE = js_parser/parser_generated.jsparagus_dump

$(DUMP_FILE): $(SOURCE_FILES)
	$(PYTHON) -m js_parser.generate_js_parser_tables --progress -o $@ $(EXTENSION_FILES:%=--extend %)

$(PY_OUT): $(EMIT_FILES) $(DUMP_FILE)
	$(PYTHON) -m js_parser.generate_js_parser_tables --progress -o $@ $(DUMP_FILE)

$(HANDLER_INFO_OUT): jsparagus/emit/collect_handler_info/src/main.rs $(HANDLER_FILE)
	(cd jsparagus/emit/collect_handler_info/; cargo run --bin collect_handler_info ../../../$(HANDLER_FILE) $(subst jsparagus/emit/collect_handler_info/,,$(HANDLER_INFO_OUT)))

$(RS_AST_OUT): crates/ast/ast.json crates/ast/generate_ast.py
	(cd crates/ast && $(abspath $(PYTHON)) generate_ast.py)

$(RS_TABLES_OUT): $(EMIT_FILES) $(DUMP_FILE) $(HANDLER_INFO_OUT)
	$(PYTHON) -m js_parser.generate_js_parser_tables --progress -o $@ $(DUMP_FILE) $(HANDLER_INFO_OUT)

# This isn't part of the `all` target because it relies on a file that might
# not be there -- it lives in a different git respository.
$(STANDARD_ES_GRAMMAR_OUT): $(ECMA262_SPEC_HTML)
	$(PYTHON) -m js_parser.extract_es_grammar $(ECMA262_SPEC_HTML) > $@ || rm $@

rust: $(RS_AST_OUT) $(RS_TABLES_OUT)
	cargo build --all

jsparagus/parse_pgen_generated.py:
	$(PYTHON) -m jsparagus.parse_pgen --regenerate > $@

check: all static-check
	./test.sh
	cargo fmt
	cargo test --all

static-check:
	$(VENV_BIN_DIR)/mypy -p jsparagus -p tests -p js_parser

jsdemo: $(PY_OUT)
	$(PYTHON) -m js_parser.try_it

update-opcodes-m-u:
	$(PYTHON) crates/emitter/scripts/update_opcodes.py \
		../mozilla-unified ./

.PHONY: all check static-check jsdemo rust update-opcodes-m-u

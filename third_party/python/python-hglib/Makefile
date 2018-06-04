PYTHON=python
help:
	@echo 'Commonly used make targets:'
	@echo '  tests - run all tests in the automatic test suite'

all: help

.PHONY: tests

MANIFEST.in:
	hg manifest | sed -e 's/^/include /' > MANIFEST.in

dist: MANIFEST.in
	TAR_OPTIONS="--owner=root --group=root --mode=u+w,go-w,a+rX-s" $(PYTHON) setup.py -q sdist

tests:
	$(PYTHON) test.py --with-doctest

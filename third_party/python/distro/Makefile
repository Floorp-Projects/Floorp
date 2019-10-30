# Copyright 2015,2016 Nir Cohen
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Name of this package
PACKAGENAME = distro

# Additional options for Sphinx
SPHINXOPTS = -v

# Paper format for the Sphinx LaTex/PDF builder.
# Valid values: a4, letter
SPHINXPAPER = a4

# Sphinx build subtree.
SPHINXBUILDDIR = build_docs

# Directory where conf.py is located
SPHINXCONFDIR = docs

# Directory where input files for Sphinx are located
SPHINXSOURCEDIR = .

# Sphinx build command (Use 'pip install sphinx' to get it)
SPHINXBUILD = sphinx-build

# Internal variables for Sphinx
SPHINXPAPEROPT_a4     = -D latex_paper_size=a4
SPHINXPAPEROPT_letter = -D latex_paper_size=letter
ALLSPHINXOPTS = -d $(SPHINXBUILDDIR)/doctrees -c $(SPHINXCONFDIR) \
                $(SPHINXPAPEROPT_$(SPHINXPAPER)) $(SPHINXOPTS) \
                $(SPHINXSOURCEDIR)

.PHONY: help
help:
	@echo 'Please use "make <target>" where <target> is one of'
	@echo "  release   - build a release and publish it"
	@echo "  dev       - prepare a development environment (includes tests)"
	@echo "  instdev   - prepare a development environment (no tests)"
	@echo "  install   - install into current Python environment"
	@echo "  html      - generate docs as standalone HTML files in: $(SPHINXBUILDDIR)/html"
	@echo "  pdf       - generate docs as PDF (via LaTeX) for paper format: $(SPHINXPAPER) in: $(SPHINXBUILDDIR)/pdf"
	@echo "  man       - generate docs as manual pages in: $(SPHINXBUILDDIR)/man"
	@echo "  docchanges   - generate an overview of all changed/added/deprecated items in docs"
	@echo "  doclinkcheck - check all external links in docs for integrity"
	@echo "  doccoverage  - run coverage check of the documentation"
	@echo "  clobber   - remove any build products"
	@echo "  build     - build the package"
	@echo "  test      - test from this directory using tox, including test coverage"
	@echo "  publish   - upload to PyPI"
	@echo "  clean     - remove any temporary build products"
	@echo "  dry-run   - perform all action required for a release without actually releasing"

.PHONY: release
release: test clean build publish
	@echo "$@ done."

.PHONY: test
test:
	pip install 'tox>=1.7.2'
	tox
	@echo "$@ done."

.PHONY: clean
clean:
	rm -rf dist build $(PACKAGENAME).egg-info
	@echo "$@ done."

.PHONY: build
build:
	python setup.py sdist bdist_wheel

.PHONY: publish
publish:
	twine upload -r pypi dist/$(PACKAGENAME)-*
	@echo "$@ done."

.PHONY: dry-run
dry-run: test clean build
	@echo "$@ done."

.PHONY: dev
dev: instdev test
	@echo "$@ done."

.PHONY: instdev
instdev:
	pip install -r dev-requirements.txt
	python setup.py develop
	@echo "$@ done."

.PHONY: install
install:
	python setup.py install
	@echo "$@ done."

.PHONY: html
html:
	$(SPHINXBUILD) -b html $(ALLSPHINXOPTS) $(SPHINXBUILDDIR)/html
	@echo "$@ done; the HTML pages are in $(SPHINXBUILDDIR)/html."

.PHONY: pdf
pdf:
	$(SPHINXBUILD) -b latex $(ALLSPHINXOPTS) $(SPHINXBUILDDIR)/pdf
	@echo "Running LaTeX files through pdflatex..."
	$(MAKE) -C $(SPHINXBUILDDIR)/pdf all-pdf
	@echo "$@ done; the PDF files are in $(SPHINXBUILDDIR)/pdf."

.PHONY: man
man:
	$(SPHINXBUILD) -b man $(ALLSPHINXOPTS) $(SPHINXBUILDDIR)/man
	@echo "$@ done; the manual pages are in $(SPHINXBUILDDIR)/man."

.PHONY: docchanges
docchanges:
	$(SPHINXBUILD) -b changes $(ALLSPHINXOPTS) $(SPHINXBUILDDIR)/changes
	@echo
	@echo "$@ done; the doc changes overview file is in $(SPHINXBUILDDIR)/changes."

.PHONY: doclinkcheck
doclinkcheck:
	$(SPHINXBUILD) -b linkcheck $(ALLSPHINXOPTS) $(SPHINXBUILDDIR)/linkcheck
	@echo
	@echo "$@ done; look for any errors in the above output " \
	      "or in $(SPHINXBUILDDIR)/linkcheck/output.txt."

.PHONY: doccoverage
doccoverage:
	$(SPHINXBUILD) -b coverage $(ALLSPHINXOPTS) $(SPHINXBUILDDIR)/coverage
	@echo "$@ done; the doc coverage results are in $(SPHINXBUILDDIR)/coverage/python.txt."

.PHONY: clobber
clobber: clean
	rm -rf $(SPHINXBUILDDIR)
	@echo "$@ done."

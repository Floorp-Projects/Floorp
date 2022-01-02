# Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
# For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

# Makefile for utility work on coverage.py.

help:					## Show this help.
	@echo "Available targets:"
	@grep '^[a-zA-Z]' $(MAKEFILE_LIST) | sort | awk -F ':.*?## ' 'NF==2 {printf "  %-26s%s\n", $$1, $$2}'

clean_platform:                         ## Remove files that clash across platforms.
	rm -f *.so */*.so
	rm -rf __pycache__ */__pycache__ */*/__pycache__ */*/*/__pycache__ */*/*/*/__pycache__ */*/*/*/*/__pycache__
	rm -f *.pyc */*.pyc */*/*.pyc */*/*/*.pyc */*/*/*/*.pyc */*/*/*/*/*.pyc
	rm -f *.pyo */*.pyo */*/*.pyo */*/*/*.pyo */*/*/*/*.pyo */*/*/*/*/*.pyo

clean: clean_platform                   ## Remove artifacts of test execution, installation, etc.
	-pip uninstall -y coverage
	rm -f *.pyd */*.pyd
	rm -rf build coverage.egg-info dist htmlcov
	rm -f *.bak */*.bak */*/*.bak */*/*/*.bak */*/*/*/*.bak */*/*/*/*/*.bak
	rm -f *$$py.class */*$$py.class */*/*$$py.class */*/*/*$$py.class */*/*/*/*$$py.class */*/*/*/*/*$$py.class
	rm -f coverage/*,cover
	rm -f MANIFEST
	rm -f .coverage .coverage.* coverage.xml .metacov*
	rm -f .tox/*/lib/*/site-packages/zzz_metacov.pth
	rm -f */.coverage */*/.coverage */*/*/.coverage */*/*/*/.coverage */*/*/*/*/.coverage */*/*/*/*/*/.coverage
	rm -f tests/covmain.zip tests/zipmods.zip
	rm -rf tests/eggsrc/build tests/eggsrc/dist tests/eggsrc/*.egg-info
	rm -f setuptools-*.egg distribute-*.egg distribute-*.tar.gz
	rm -rf doc/_build doc/_spell doc/sample_html_beta
	rm -rf .cache .pytest_cache .hypothesis
	rm -rf $$TMPDIR/coverage_test
	-make -C tests/gold/html clean

sterile: clean                          ## Remove all non-controlled content, even if expensive.
	rm -rf .tox
	-docker image rm -f quay.io/pypa/manylinux1_i686 quay.io/pypa/manylinux1_x86_64


CSS = coverage/htmlfiles/style.css
SCSS = coverage/htmlfiles/style.scss

css: $(CSS)				## Compile .scss into .css.
$(CSS): $(SCSS)
	sass --style=compact --sourcemap=none --no-cache $(SCSS) $@
	cp $@ tests/gold/html/styled

LINTABLE = coverage tests igor.py setup.py __main__.py

lint:					## Run linters and checkers.
	tox -e lint

todo:
	-grep -R --include=*.py TODO $(LINTABLE)

pep8:
	pycodestyle --filename=*.py --repeat $(LINTABLE)

test:
	tox -e py27,py35 $(ARGS)

PYTEST_SMOKE_ARGS = -n 6 -m "not expensive" --maxfail=3 $(ARGS)

smoke: 					## Run tests quickly with the C tracer in the lowest supported Python versions.
	COVERAGE_NO_PYTRACER=1 tox -q -e py27,py35 -- $(PYTEST_SMOKE_ARGS)

pysmoke: 				## Run tests quickly with the Python tracer in the lowest supported Python versions.
	COVERAGE_NO_CTRACER=1 tox -q -e py27,py35 -- $(PYTEST_SMOKE_ARGS)

DOCKER_RUN = docker run -it --init --rm -v `pwd`:/io
RUN_MANYLINUX_X86 = $(DOCKER_RUN) quay.io/pypa/manylinux1_x86_64 /io/ci/manylinux.sh
RUN_MANYLINUX_I686 = $(DOCKER_RUN) quay.io/pypa/manylinux1_i686 /io/ci/manylinux.sh

test_linux:				## Run the tests in Linux under Docker.
	# The Linux .pyc files clash with the host's because of file path
	# changes, so clean them before and after running tests.
	make clean_platform
	$(RUN_MANYLINUX_X86) test $(ARGS)
	make clean_platform

meta_linux:				## Run meta-coverage in Linux under Docker.
	ARGS="meta $(ARGS)" make test_linux

# Coverage measurement of coverage.py itself (meta-coverage). See metacov.ini
# for details.

metacov:				## Run meta-coverage, measuring ourself.
	COVERAGE_COVERAGE=yes tox $(ARGS)

metahtml:				## Produce meta-coverage HTML reports.
	python igor.py combine_html

# Kitting

kit:					## Make the source distribution.
	python setup.py sdist

wheel:					## Make the wheels for distribution.
	tox -c tox_wheels.ini $(ARGS)

kit_linux:				## Make the Linux wheels.
	$(RUN_MANYLINUX_X86) build
	$(RUN_MANYLINUX_I686) build

kit_upload:				## Upload the built distributions to PyPI.
	twine upload --verbose dist/*

test_upload:				## Upload the distrubutions to PyPI's testing server.
	twine upload --verbose --repository testpypi dist/*

kit_local:
	# pip.conf looks like this:
	#   [global]
	#   find-links = file:///Users/ned/Downloads/local_pypi
	cp -v dist/* `awk -F "//" '/find-links/ {print $$2}' ~/.pip/pip.conf`
	# pip caches wheels of things it has installed. Clean them out so we
	# don't go crazy trying to figure out why our new code isn't installing.
	find ~/Library/Caches/pip/wheels -name 'coverage-*' -delete

download_appveyor:			## Download the latest Windows artifacts from AppVeyor.
	python ci/download_appveyor.py nedbat/coveragepy

build_ext:
	python setup.py build_ext

# Documentation

DOCBIN = .tox/doc/bin
SPHINXOPTS = -aE
SPHINXBUILD = $(DOCBIN)/sphinx-build $(SPHINXOPTS)
SPHINXAUTOBUILD = $(DOCBIN)/sphinx-autobuild -p 9876 --ignore '.git/**' --open-browser
WEBHOME = ~/web/stellated/
WEBSAMPLE = $(WEBHOME)/files/sample_coverage_html
WEBSAMPLEBETA = $(WEBHOME)/files/sample_coverage_html_beta

docreqs:
	tox -q -e doc --notest

dochtml: docreqs			## Build the docs HTML output.
	$(DOCBIN)/python doc/check_copied_from.py doc/*.rst
	$(SPHINXBUILD) -b html doc doc/_build/html

docdev: dochtml				## Build docs, and auto-watch for changes.
	PATH=$(DOCBIN):$(PATH) $(SPHINXAUTOBUILD) -b html doc doc/_build/html

docspell: docreqs
	$(SPHINXBUILD) -b spelling doc doc/_spell

publish:
	rm -f $(WEBSAMPLE)/*.*
	mkdir -p $(WEBSAMPLE)
	cp doc/sample_html/*.* $(WEBSAMPLE)

publishbeta:
	rm -f $(WEBSAMPLEBETA)/*.*
	mkdir -p $(WEBSAMPLEBETA)
	cp doc/sample_html_beta/*.* $(WEBSAMPLEBETA)

upload_relnotes: docreqs		## Upload parsed release notes to Tidelift.
	$(SPHINXBUILD) -b rst doc /tmp/rst_rst
	pandoc -frst -tmarkdown_strict --atx-headers /tmp/rst_rst/changes.rst > /tmp/rst_rst/changes.md
	python ci/upload_relnotes.py /tmp/rst_rst/changes.md pypi/coverage

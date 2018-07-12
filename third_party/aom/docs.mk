##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##



INSTALL_MAPS += docs/%    docs/%
INSTALL_MAPS += src/%     %
INSTALL_MAPS += %         %

# Static documentation authored in doxygen
CODEC_DOX :=    mainpage.dox \
		keywords.dox \
		usage.dox \
		usage_cx.dox \
		usage_dx.dox \

# Other doxy files sourced in Markdown
TXT_DOX = $(call enabled,TXT_DOX)

EXAMPLE_PATH += $(SRC_PATH_BARE) #for CHANGELOG, README, etc
EXAMPLE_PATH += $(SRC_PATH_BARE)/examples

doxyfile: $(if $(findstring examples, $(ALL_TARGETS)),examples.doxy)
doxyfile: libs.doxy_template libs.doxy
	@echo "    [CREATE] $@"
	@cat $^ > $@
	@echo "STRIP_FROM_PATH += $(SRC_PATH_BARE) $(BUILD_ROOT)" >> $@
	@echo "INPUT += $(addprefix $(SRC_PATH_BARE)/,$(CODEC_DOX))" >> $@;
	@echo "INPUT += $(TXT_DOX)" >> $@;
	@echo "EXAMPLE_PATH += $(EXAMPLE_PATH)" >> $@

CLEAN-OBJS += doxyfile $(wildcard docs/html/*)
docs/html/index.html: doxyfile $(CODEC_DOX) $(TXT_DOX)
	@echo "    [DOXYGEN] $<"
	@doxygen $<
DOCS-yes += docs/html/index.html

DIST-DOCS-yes = $(wildcard docs/html/*)
DIST-DOCS-$(CONFIG_CODEC_SRCS) += $(addprefix src/,$(CODEC_DOX))
DIST-DOCS-$(CONFIG_CODEC_SRCS) += src/libs.doxy_template
DIST-DOCS-yes                  += CHANGELOG
DIST-DOCS-yes                  += README

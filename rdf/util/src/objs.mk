# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

RDF_UTIL_SRC_LCPPSRCS = \
	nsRDFResource.cpp \
	$(NULL)

RDF_UTIL_SRC_CPPSRCS = $(addprefix $(topsrcdir)/rdf/util/src/, $(RDF_UTIL_SRC_LCPPSRCS))

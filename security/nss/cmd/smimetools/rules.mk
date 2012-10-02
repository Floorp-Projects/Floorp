#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# $Id: rules.mk,v 1.5 2012/03/20 14:47:20 gerv%gerv.net Exp $

install::
	$(INSTALL) -m 755 $(SCRIPTS) $(SOURCE_BIN_DIR)

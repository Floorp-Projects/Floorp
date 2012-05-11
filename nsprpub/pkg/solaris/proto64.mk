# 
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#ident  "$Id: proto64.mk,v 1.4 2012/03/06 13:13:40 gerv%gerv.net Exp $"
#

ifeq ($(USE_64), 1)
  # Remove 64 tag
  sed_proto64='s/\#64\#//g'
else
  # Strip 64 lines
  sed_proto64='/\#64\#/d'
endif

#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident  "$Id: proto64.mk,v 1.1 2004/10/08 04:16:06 christophe.ravel.bugs%sun.com Exp $"
#

ifeq ($(USE_64), 1)
  # Remove 64 tag
  sed_proto64='s/\#64\#//g'
else
  # Strip 64 lines
  sed_proto64='/\#64\#/d'
endif

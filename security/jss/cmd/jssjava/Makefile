#! gmake
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Security Services for Java.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

all::
	gmake -f jnigen.mk
	gmake -f csrcs.mk

import::
	gmake -f jnigen.mk import
	gmake -f csrcs.mk import

clean::
	gmake -f jnigen.mk clean
	gmake -f csrcs.mk clean

export::
	gmake -f jnigen.mk export
	gmake -f csrcs.mk export

private_export::
	gmake -f jnigen.mk private_export
	gmake -f csrcs.mk private_export

libs::
	gmake -f jnigen.mk libs
	gmake -f csrcs.mk libs

program::
	gmake -f jnigen.mk program
	gmake -f csrcs.mk program

install::
	gmake -f jnigen.mk install
	gmake -f csrcs.mk install

release::
	gmake -f jnigen.mk release
	gmake -f csrcs.mk release

release_clean::
	gmake -f jnigen.mk release_clean
	gmake -f csrcs.mk release_clean

release_export::
	gmake -f jnigen.mk release_export
	gmake -f csrcs.mk release_export

release_classes::
	gmake -f jnigen.mk release_classes
	gmake -f csrcs.mk release_classes

ifneq ($(POLICY),)
release_policy::
	gmake -f jnigen.mk release_policy
	gmake -f csrcs.mk release_policy
endif

release_md::
	gmake -f jnigen.mk release_md
	gmake -f csrcs.mk release_md

release_jars::
	gmake -f jnigen.mk release_jars
	gmake -f csrcs.mk release_jars

release_cpdistdir::
	gmake -f jnigen.mk release_cpdistdir
	gmake -f csrcs.mk release_cpdistdir

tests::
	gmake -f jnigen.mk tests
	gmake -f csrcs.mk tests

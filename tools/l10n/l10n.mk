# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla Localization code.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Axel Hecht <l10n@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

include client.mk

.PHONY : check-l10n

check-l10n:
	for loc in $(MOZ_CO_LOCALES); do \
	for mod in $(sort $(foreach project,$(MOZ_PROJECT_LIST),$(LOCALES_$(project)))); do \
	  echo Comparing $(TOPSRCDIR)/$$mod/locales/en-US with $(TOPSRCDIR)/../l10n/$$loc/$$mod; \
	  perl $(TOPSRCDIR)/toolkit/locales/compare-locales.pl $(TOPSRCDIR)/$$mod/locales/en-US $(TOPSRCDIR)/../l10n/$$loc/$$mod; \
	done; \
	done;

create-%:
	for mod in $(sort $(foreach project,$(MOZ_PROJECT_LIST),$(LOCALES_$(project)))); do \
	  if test -d $(TOPSRCDIR)/../l10n/$*/$$mod; then \
	    echo $(TOPSRCDIR)/../l10n/$*/$$mod already exists; \
	  else \
	    echo Creating $(TOPSRCDIR)/../l10n/$*/$$mod from $(TOPSRCDIR)/$$mod/locales/en-US; \
	    mkdir -p ../l10n/$*/$$mod; \
	    cp -r $(TOPSRCDIR)/$$mod/locales/en-US/* $(TOPSRCDIR)/../l10n/$*/$$mod; \
	    find $(TOPSRCDIR)/../l10n/$*/$$mod -name CVS | xargs rm -rf; \
	  fi; \
	done;

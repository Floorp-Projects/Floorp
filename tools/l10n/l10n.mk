# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

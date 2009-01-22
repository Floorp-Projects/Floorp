dashes   = ----------
msg = @echo "$(1)": $(dashes) $(2) $(dashes)
prepend = sed 's/^/$(1): /'
html2text = lynx --dump $(1)
TEST_PROFILE?=TEST

dumpvars = echo TARGETS=$$targets && for var in `echo $${!TEST_*}`; do echo $${var}=$${!var}; done



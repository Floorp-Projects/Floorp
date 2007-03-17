# This file contains a single hash named %strings, which is used by the
# installation code to display strings before Template-Toolkit can safely
# be loaded.
#
# Each string supports a very simple substitution system, where you can
# have variables named like ##this## and they'll be replaced by the string
# variable with that name.
#
# Please keep the strings in alphabetical order by their name.

%strings = (
    version_and_os => "* This is Bugzilla ##bz_ver## on perl ##perl_ver##\n"
                      . "* Running on ##os_name## ##os_ver##",
);

1;

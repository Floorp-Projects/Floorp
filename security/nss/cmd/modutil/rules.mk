# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Some versions of yacc generate files that include platform-specific
# system headers.  For example, the yacc in Solaris 2.6 inserts
#     #include <values.h>
# which does not exist on NT.  For portability, always use Berkeley
# yacc (such as the yacc in Linux) to generate files.
#

generate: installparse.c installparse.l

installparse.c:
	yacc -p Pk11Install_yy -d installparse.y
	mv y.tab.c installparse.c
	mv y.tab.h installparse.h

installparse.l:
	lex -olex.Pk11Install_yy.c -PPk11Install_yy installparse.l
	@echo
	@echo "**YOU MUST COMMENT OUT UNISTD.H FROM lex.Pk11Install_yy.cpp**"

install.c: install-ds.h install.h

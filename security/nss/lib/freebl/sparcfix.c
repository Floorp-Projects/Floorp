/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is a program to modify v8+vis objects for linking.
 *
 * The Initial Developer of the Original Code is Sun Microsystems Inc.
 * Portions created by Sun Microsystems Inc. are 
 * Copyright (C) 1999-2000 Sun Microsystems Inc. All Rights Reserved.
 * 
 * Contributor(s):
 *	Netscape Communications Corporation
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *  $Id: sparcfix.c,v 1.1 2000/09/29 23:39:30 nelsonb%netscape.com Exp $
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(SOLARIS2_6) || defined(SOLARIS2_7) || defined(SOLARIS2_8)
#define NEW_SYSV_SPARC 1
#include <gelf.h>
#endif
#include <libelf.h>
#include <sys/elf_SPARC.h>

int
main(int argc, char *argv[])
{
	Elf *     elf;
	off_t     size;
	int       fd;
	int       count;
#if defined(NEW_SYSV_SPARC)
	GElf_Ehdr  hdr;
	GElf_Ehdr *ehdr = &hdr;
#else
	Elf32_Ehdr *ehdr;
#endif


	elf_version(EV_CURRENT);
	fd = open(argv[1], O_RDWR);
	if (fd < 0)
	    goto loser;
	elf = elf_begin(fd, ELF_C_RDWR, (Elf *)0);
	if (!elf)
	    goto loser;

#if defined(NEW_SYSV_SPARC)
	gelf_getehdr(elf, ehdr);
#else
        ehdr = elf32_getehdr(elf);
	if (!ehdr)
	    goto loser;
#endif

	if (ehdr->e_machine == EM_SPARC32PLUS) {
	    ehdr->e_machine = EM_SPARC;
	    ehdr->e_flags &= ~(EF_SPARC_32PLUS | EF_SPARC_SUN_US1);
#if defined(NEW_SYSV_SPARC)
	    count = gelf_update_ehdr(elf, ehdr);
	    if (count < 0)
		goto loser;
#endif
	    size = elf_update(elf, ELF_C_WRITE);
	    if (size < 0)
		goto loser;
	}

	do {
	    count = elf_end(elf);
	} while (count > 0);
	return count;

loser:
	return 1;
}

#include <sys/exec_elf.h>
#define ElfW(type)	_ElfW (Elf, ELFSIZE, type)
#define _ElfW(e,w,t)	_ElfW_1 (e, w, _##t)
#define _ElfW_1(e,w,t)	e##w##t

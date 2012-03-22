/* libunwind - a platform-independent unwind library
   Copyright (C) 2003-2004 Hewlett-Packard Co
	Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include <elf.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

#include "_UPT_internal.h"

#if UNW_TARGET_IA64

#include "elf64.h"

static unw_word_t
find_gp (struct UPT_info *ui, Elf64_Phdr *pdyn, Elf64_Addr load_base)
{
  Elf64_Off soff, str_soff;
  Elf64_Ehdr *ehdr = ui->ei.image;
  Elf64_Shdr *shdr;
  Elf64_Shdr *str_shdr;
  Elf64_Addr gp = 0;
  char *strtab;
  int i;

  if (pdyn)
    {
      /* If we have a PT_DYNAMIC program header, fetch the gp-value
	 from the DT_PLTGOT entry.  */
      Elf64_Dyn *dyn = (Elf64_Dyn *) (pdyn->p_offset + (char *) ui->ei.image);
      for (; dyn->d_tag != DT_NULL; ++dyn)
	if (dyn->d_tag == DT_PLTGOT)
	  {
	    gp = (Elf64_Addr) dyn->d_un.d_ptr + load_base;
	    goto done;
	  }
    }

  /* Without a PT_DYAMIC header, lets try to look for a non-empty .opd
     section.  If there is such a section, we know it's full of
     function descriptors, and we can simply pick up the gp from the
     second word of the first entry in this table.  */

  soff = ehdr->e_shoff;
  str_soff = soff + (ehdr->e_shstrndx * ehdr->e_shentsize);

  if (soff + ehdr->e_shnum * ehdr->e_shentsize > ui->ei.size)
    {
      Debug (1, "section table outside of image? (%lu > %lu)",
	     soff + ehdr->e_shnum * ehdr->e_shentsize,
	     ui->ei.size);
      goto done;
    }

  shdr = (Elf64_Shdr *) ((char *) ui->ei.image + soff);
  str_shdr = (Elf64_Shdr *) ((char *) ui->ei.image + str_soff);
  strtab = (char *) ui->ei.image + str_shdr->sh_offset;
  for (i = 0; i < ehdr->e_shnum; ++i)
    {
      if (strcmp (strtab + shdr->sh_name, ".opd") == 0
	  && shdr->sh_size >= 16)
	{
	  gp = ((Elf64_Addr *) ((char *) ui->ei.image + shdr->sh_offset))[1];
	  goto done;
	}
      shdr = (Elf64_Shdr *) (((char *) shdr) + ehdr->e_shentsize);
    }

 done:
  Debug (16, "image at %p, gp = %lx\n", ui->ei.image, gp);
  return gp;
}

HIDDEN int
_UPTi_find_unwind_table (struct UPT_info *ui, unw_addr_space_t as,
			 char *path, unw_word_t segbase, unw_word_t mapoff,
			 unw_word_t ip)
{
  Elf64_Phdr *phdr, *ptxt = NULL, *punw = NULL, *pdyn = NULL;
  Elf64_Ehdr *ehdr;
  int i;

  if (!_Uelf64_valid_object (&ui->ei))
    return -UNW_ENOINFO;

  ehdr = ui->ei.image;
  phdr = (Elf64_Phdr *) ((char *) ui->ei.image + ehdr->e_phoff);

  for (i = 0; i < ehdr->e_phnum; ++i)
    {
      switch (phdr[i].p_type)
	{
	case PT_LOAD:
	  if (phdr[i].p_offset == mapoff)
	    ptxt = phdr + i;
	  break;

	case PT_IA_64_UNWIND:
	  punw = phdr + i;
	  break;

	case PT_DYNAMIC:
	  pdyn = phdr + i;
	  break;

	default:
	  break;
	}
    }
  if (!ptxt || !punw)
    return 0;

  ui->di_cache.start_ip = segbase;
  ui->di_cache.end_ip = ui->di_cache.start_ip + ptxt->p_memsz;
  ui->di_cache.gp = find_gp (ui, pdyn, segbase - ptxt->p_vaddr);
  ui->di_cache.format = UNW_INFO_FORMAT_TABLE;
  ui->di_cache.u.ti.name_ptr = 0;
  ui->di_cache.u.ti.segbase = segbase;
  ui->di_cache.u.ti.table_len = punw->p_memsz / sizeof (unw_word_t);
  ui->di_cache.u.ti.table_data = (unw_word_t *)
    ((char *) ui->ei.image + (punw->p_vaddr - ptxt->p_vaddr));
  return 1;
}

#elif UNW_TARGET_X86 || UNW_TARGET_X86_64 || UNW_TARGET_HPPA \
|| UNW_TARGET_PPC32 || UNW_TARGET_PPC64 || UNW_TARGET_ARM

#include "dwarf-eh.h"
#include "dwarf_i.h"

/* We need our own instance of dwarf_read_encoded_pointer() here since
   the one in dwarf/Gpe.c is not (and should not be) exported.  */
HIDDEN int
_UPTi_dwarf_read_encoded_pointer (unw_addr_space_t as, unw_accessors_t *a,
				  unw_word_t *addr, unsigned char encoding,
				  const unw_proc_info_t *pi,
				  unw_word_t *valp, void *arg)
{
  return dwarf_read_encoded_pointer_inlined (as, a, addr, encoding,
					     pi, valp, arg);
}

HIDDEN int
_UPTi_find_unwind_table (struct UPT_info *ui, unw_addr_space_t as,
			 char *path, unw_word_t segbase, unw_word_t mapoff,
			 unw_word_t ip)
{
  Elf_W(Phdr) *phdr, *ptxt = NULL, *peh_hdr = NULL, *pdyn = NULL;
  unw_word_t addr, eh_frame_start, fde_count, load_base;
  unw_word_t max_load_addr = 0;
  unw_word_t start_ip = (unw_word_t) -1;
  unw_word_t end_ip = 0;
  struct dwarf_eh_frame_hdr *hdr;
  unw_proc_info_t pi;
  unw_accessors_t *a;
  Elf_W(Ehdr) *ehdr;
#if UNW_TARGET_ARM
  const Elf_W(Phdr) *parm_exidx = NULL;
#endif
  int i, ret, found = 0;

  /* XXX: Much of this code is Linux/LSB-specific.  */

  if (!elf_w(valid_object) (&ui->ei))
    return -UNW_ENOINFO;

  ehdr = ui->ei.image;
  phdr = (Elf_W(Phdr) *) ((char *) ui->ei.image + ehdr->e_phoff);

  for (i = 0; i < ehdr->e_phnum; ++i)
    {
      switch (phdr[i].p_type)
	{
	case PT_LOAD:
	  if (phdr[i].p_vaddr < start_ip)
	    start_ip = phdr[i].p_vaddr;

	  if (phdr[i].p_vaddr + phdr[i].p_memsz > end_ip)
	    end_ip = phdr[i].p_vaddr + phdr[i].p_memsz;

          // This doesn't really make sense since the offset might be
          // before the mapped section.  Note that we may also want to
          // take the size of the segment into account.
	  // if (phdr[i].p_offset == mapoff)
          if (phdr[i].p_offset >= mapoff)
	    ptxt = phdr + i;
	  if ((uintptr_t) ui->ei.image + phdr->p_filesz > max_load_addr)
	    max_load_addr = (uintptr_t) ui->ei.image + phdr->p_filesz;
	  break;

	case PT_GNU_EH_FRAME:
	  peh_hdr = phdr + i;
	  break;

	case PT_DYNAMIC:
	  pdyn = phdr + i;
	  break;

#if UNW_TARGET_ARM
	case PT_ARM_EXIDX:
	  parm_exidx = phdr + i;
	  break;
#endif

	default:
	  break;
	}
    }

  if (!ptxt)
    return 0;

  load_base = segbase - ptxt->p_vaddr;
  start_ip += load_base;
  end_ip += load_base;

  if (peh_hdr)
    {
      if (pdyn)
	{
	  /* For dynamicly linked executables and shared libraries,
	     DT_PLTGOT is the value that data-relative addresses are
	     relative to for that object.  We call this the "gp".  */
		Elf_W(Dyn) *dyn = (Elf_W(Dyn) *)(pdyn->p_offset
						 + (char *) ui->ei.image);
	  for (; dyn->d_tag != DT_NULL; ++dyn)
	    if (dyn->d_tag == DT_PLTGOT)
	      {
		/* Assume that _DYNAMIC is writable and GLIBC has
		   relocated it (true for x86 at least).  */
		ui->di_cache.gp = dyn->d_un.d_ptr;
		break;
	      }
	}
      else
	/* Otherwise this is a static executable with no _DYNAMIC.  Assume
	   that data-relative addresses are relative to 0, i.e.,
	   absolute.  */
	ui->di_cache.gp = 0;

      hdr = (struct dwarf_eh_frame_hdr *) (peh_hdr->p_offset
					   + (char *) ui->ei.image);
      if (hdr->version != DW_EH_VERSION)
	{
	  Debug (1, "table `%s' has unexpected version %d\n",
		 path, hdr->version);
	  return -UNW_ENOINFO;
	}

      a = unw_get_accessors (unw_local_addr_space);
      addr = (unw_word_t) (hdr + 1);

      /* Fill in a dummy proc_info structure.  We just need to fill in
	 enough to ensure that dwarf_read_encoded_pointer() can do it's
	 job.  Since we don't have a procedure-context at this point, all
	 we have to do is fill in the global-pointer.  */
      memset (&pi, 0, sizeof (pi));
      pi.gp = ui->di_cache.gp;

      /* (Optionally) read eh_frame_ptr: */
      if ((ret = _UPTi_dwarf_read_encoded_pointer (unw_local_addr_space, a,
						   &addr, hdr->eh_frame_ptr_enc, &pi,
						   &eh_frame_start, NULL)) < 0)
	return -UNW_ENOINFO;

      /* (Optionally) read fde_count: */
      if ((ret = _UPTi_dwarf_read_encoded_pointer (unw_local_addr_space, a,
						   &addr, hdr->fde_count_enc, &pi,
						   &fde_count, NULL)) < 0)
	return -UNW_ENOINFO;

      if (hdr->table_enc != (DW_EH_PE_datarel | DW_EH_PE_sdata4))
	{
    #if 1
	  abort ();
    #else
	  unw_word_t eh_frame_end;

	  /* If there is no search table or it has an unsupported
	     encoding, fall back on linear search.  */
	  if (hdr->table_enc == DW_EH_PE_omit)
	    Debug (4, "EH lacks search table; doing linear search\n");
	  else
	    Debug (4, "EH table has encoding 0x%x; doing linear search\n",
		   hdr->table_enc);

	  eh_frame_end = max_load_addr;	/* XXX can we do better? */

	  if (hdr->fde_count_enc == DW_EH_PE_omit)
	    fde_count = ~0UL;
	  if (hdr->eh_frame_ptr_enc == DW_EH_PE_omit)
	    abort ();

	  return linear_search (unw_local_addr_space, ip,
				eh_frame_start, eh_frame_end, fde_count,
				pi, need_unwind_info, NULL);
    #endif
	}

      ui->di_cache.start_ip = start_ip;
      ui->di_cache.end_ip = end_ip;
      ui->di_cache.format = UNW_INFO_FORMAT_REMOTE_TABLE;
      ui->di_cache.u.rti.name_ptr = 0;
      /* two 32-bit values (ip_offset/fde_offset) per table-entry: */
      ui->di_cache.u.rti.table_len = (fde_count * 8) / sizeof (unw_word_t);
      ui->di_cache.u.rti.table_data = ((load_base + peh_hdr->p_vaddr)
				       + (addr - (unw_word_t) ui->ei.image
					  - peh_hdr->p_offset));

      /* For the binary-search table in the eh_frame_hdr, data-relative
	 means relative to the start of that section... */
      ui->di_cache.u.rti.segbase = ((load_base + peh_hdr->p_vaddr)
				    + ((unw_word_t) hdr - (unw_word_t) ui->ei.image
				       - peh_hdr->p_offset));
      found = 1;
    }

#if UNW_TARGET_ARM
  if (parm_exidx)
    {
      ui->di_arm.format = UNW_INFO_FORMAT_ARM_EXIDX;
      ui->di_arm.start_ip = start_ip;
      ui->di_arm.end_ip = end_ip;
      ui->di_arm.u.rti.name_ptr = (unw_word_t) path;
      ui->di_arm.u.rti.table_data = load_base + parm_exidx->p_vaddr;
      ui->di_arm.u.rti.table_len = parm_exidx->p_memsz;
      found = 1;
    }
#endif

#ifdef CONFIG_DEBUG_FRAME
  /* Try .debug_frame. */
  found = dwarf_find_debug_frame (found, &ui->di_debug, ip, segbase, path,
				  start_ip, end_ip);
#endif

  return found;
}

#endif /* UNW_TARGET_X86 || UNW_TARGET_X86_64 || UNW_TARGET_HPPA*/

static int
get_unwind_info (struct UPT_info *ui, unw_addr_space_t as, unw_word_t ip)
{
  unsigned long segbase, mapoff;
  char path[PATH_MAX];

#if UNW_TARGET_IA64 && defined(__linux)
  if (!ui->ktab.start_ip && _Uia64_get_kernel_table (&ui->ktab) < 0)
    return -UNW_ENOINFO;

  if (ui->ktab.format != -1 && ip >= ui->ktab.start_ip && ip < ui->ktab.end_ip)
    return 0;
#endif

  if ((ui->di_cache.format != -1
       && ip >= ui->di_cache.start_ip && ip < ui->di_cache.end_ip)
#if UNW_TARGET_ARM
      || (ui->di_debug.format != -1
       && ip >= ui->di_arm.start_ip && ip < ui->di_arm.end_ip)
#endif
      || (ui->di_debug.format != -1
       && ip >= ui->di_debug.start_ip && ip < ui->di_debug.end_ip))
    return 0;

  if (ui->ei.image)
    {
      munmap (ui->ei.image, ui->ei.size);
      ui->ei.image = NULL;
      ui->ei.size = 0;

      /* invalidate the cache: */
      ui->di_cache.start_ip = ui->di_cache.end_ip = 0;
      ui->di_debug.start_ip = ui->di_debug.end_ip = 0;
      ui->di_cache.format = -1;
      ui->di_debug.format = -1;
#if UNW_TARGET_ARM
      ui->di_arm.start_ip = ui->di_arm.end_ip = 0;
      ui->di_arm.format = -1;
#endif
    }

  if (tdep_get_elf_image (&ui->ei, ui->pid, ip, &segbase, &mapoff, path,
                          sizeof(path)) < 0)
    return -UNW_ENOINFO;

  /* Here, SEGBASE is the starting-address of the (mmap'ped) segment
     which covers the IP we're looking for.  */
  if (_UPTi_find_unwind_table (ui, as, path, segbase, mapoff, ip) < 0)
    return -UNW_ENOINFO;

  /* This can happen in corner cases where dynamically generated
     code falls into the same page that contains the data-segment
     and the page-offset of the code is within the first page of
     the executable.  */
  if (ui->di_cache.format != -1
      && (ip < ui->di_cache.start_ip || ip >= ui->di_cache.end_ip))
     ui->di_cache.format = -1;

  if (ui->di_debug.format != -1
      && (ip < ui->di_debug.start_ip || ip >= ui->di_debug.end_ip))
     ui->di_debug.format = -1;

  if (ui->di_cache.format == -1
#if UNW_TARGET_ARM
      && ui->di_arm.format == -1
#endif
      && ui->di_debug.format == -1)
    return -UNW_ENOINFO;

  return 0;
}

int
_UPT_find_proc_info (unw_addr_space_t as, unw_word_t ip, unw_proc_info_t *pi,
		     int need_unwind_info, void *arg)
{
  struct UPT_info *ui = arg;
  int ret = -UNW_ENOINFO;

  if (get_unwind_info (ui, as, ip) < 0)
    return -UNW_ENOINFO;

#if UNW_TARGET_IA64
  if (ui->ktab.format != -1)
    {
      /* The kernel unwind table resides in local memory, so we have
	 to use the local address space to search it.  Since
	 _UPT_put_unwind_info() has no easy way of detecting this
	 case, we simply make a copy of the unwind-info, so
	 _UPT_put_unwind_info() can always free() the unwind-info
	 without ill effects.  */
      ret = tdep_search_unwind_table (unw_local_addr_space, ip, &ui->ktab, pi,
				      need_unwind_info, arg);
      if (ret >= 0)
	{
	  if (!need_unwind_info)
	    pi->unwind_info = NULL;
	  else
	    {
	      void *mem = malloc (pi->unwind_info_size);

	      if (!mem)
		return -UNW_ENOMEM;
	      memcpy (mem, pi->unwind_info, pi->unwind_info_size);
	      pi->unwind_info = mem;
	    }
	}
    }
#endif

  if (ret == -UNW_ENOINFO && ui->di_cache.format != -1)
    ret = tdep_search_unwind_table (as, ip, &ui->di_cache,
				    pi, need_unwind_info, arg);

#if UNW_TARGET_ARM
  if (ret == -UNW_ENOINFO && ui->di_arm.format != -1)
    ret = tdep_search_unwind_table (as, ip, &ui->di_arm, pi,
                                    need_unwind_info, arg);
#endif

  if (ret == -UNW_ENOINFO && ui->di_debug.format != -1)
    ret = tdep_search_unwind_table (as, ip, &ui->di_debug, pi,
				    need_unwind_info, arg);

  return ret;
}

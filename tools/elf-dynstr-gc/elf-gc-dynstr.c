/* elf_gc_dynst
 *
 * This is a program that removes unreferenced strings from the .dynstr
 * section in ELF shared objects. It also shrinks the .dynstr section and
 * relocates all symbols after it.
 *
 * This program was written and copyrighted by:
 *   Alexander Larsson <alla@lysator.liu.se>
 *
 *
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 */


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <elf.h>
#include <glib.h>


Elf32_Ehdr *elf_header = NULL;
#define FILE_OFFSET(offset) ((unsigned char *)(elf_header) + (offset))

struct dynamic_symbol {
  Elf32_Word old_index;
  Elf32_Word new_index;
  char *string;
};

GHashTable *used_dynamic_symbols = NULL;
/* Data is dynamic_symbols, hashes on old_index */
Elf32_Word hole_index;
Elf32_Word hole_end;
Elf32_Word hole_len;

Elf32_Addr hole_addr_start;
Elf32_Addr hole_addr_remap_start;
Elf32_Addr hole_addr_remap_end;


Elf32_Shdr *
elf_find_section_num(int section_index)
{
  Elf32_Shdr *section;
  Elf32_Word sectionsize;

  section = (Elf32_Shdr *)FILE_OFFSET(elf_header->e_shoff);
  sectionsize = elf_header->e_shentsize;

  section = (Elf32_Shdr *)((char *)section + sectionsize*section_index);

  return section;
}

Elf32_Shdr *
elf_find_section_named(char *name)
{
  Elf32_Shdr *section;
  Elf32_Shdr *strtab_section;
  Elf32_Word sectionsize;
  int numsections;
  char *strtab;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(elf_header->e_shoff);

  strtab_section = elf_find_section_num(elf_header->e_shstrndx);
  
  strtab = (char *)FILE_OFFSET(strtab_section->sh_offset);
  
  sectionsize = elf_header->e_shentsize;
  numsections = elf_header->e_shnum;

  for (i=0;i<numsections;i++) {
    if (strcmp(&strtab[section->sh_name], name) == 0) {
      return section;
    }
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
  return NULL;
}


Elf32_Shdr *
elf_find_section(Elf32_Word sh_type)
{
  Elf32_Shdr *section;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(elf_header->e_shoff);
  sectionsize = elf_header->e_shentsize;
  numsections = elf_header->e_shnum;

  for (i=0;i<numsections;i++) {
    if (section->sh_type == sh_type) {
      return section;
    }
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
  return NULL;
}

Elf32_Shdr *
elf_find_next_higher_section(Elf32_Word offset)
{
  Elf32_Shdr *section;
  Elf32_Shdr *higher;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(elf_header->e_shoff);
  sectionsize = elf_header->e_shentsize;
  numsections = elf_header->e_shnum;

  higher = NULL;

  for (i=0;i<numsections;i++) {
    if (section->sh_offset >= offset) {
      if (higher == NULL) {
	higher = section;
      } else if (section->sh_offset < higher->sh_offset) {
	higher = section;
      }
    }
    
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
  
  return higher;
}

Elf32_Word
vma_to_offset(Elf32_Addr addr)
{
  Elf32_Shdr *section;
  Elf32_Shdr *higher;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(elf_header->e_shoff);
  sectionsize = elf_header->e_shentsize;
  numsections = elf_header->e_shnum;

  higher = NULL;

  for (i=0;i<numsections;i++) {
    if ( (addr >= section->sh_addr) &&
	 (addr < section->sh_addr + section->sh_size) ) {
      return section->sh_offset + (addr-section->sh_addr);
    }
    
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }

  fprintf(stderr, "Warning, unable to convert address %d (0x%x) to file offset\n",
	 addr, addr);
  return 0;
}


void
find_segment_addr_min_max(Elf32_Word file_offset,
			  Elf32_Addr *start, Elf32_Addr *end)
{
  Elf32_Phdr *segment;
  Elf32_Word segmentsize;
  int numsegments;
  int i = 0;

  segment = (Elf32_Phdr *)FILE_OFFSET(elf_header->e_phoff);
  segmentsize = elf_header->e_phentsize;
  numsegments = elf_header->e_phnum;

  for (i=0;i<numsegments;i++) {
    if ((file_offset >= segment->p_offset) &&
	(file_offset < segment->p_offset + segment->p_filesz))  {
      *start = segment->p_vaddr;
      *end = segment->p_vaddr + segment->p_memsz;
      return;
    }

    segment = (Elf32_Phdr *)((char *)segment + segmentsize);
  }
  fprintf(stderr, "Error: Couldn't find segment in find_segment_addr_min_max()\n");
}

void *
dynamic_find_tag(Elf32_Shdr *dynamic, Elf32_Sword d_tag)
{
  int i;
  Elf32_Dyn *element;

  element = (Elf32_Dyn *)FILE_OFFSET(dynamic->sh_offset);
  for (i=0; element[i].d_tag != DT_NULL; i++) {
    if (element[i].d_tag = d_tag) {
      return FILE_OFFSET(element[i].d_un.d_ptr);
    }
  }
  
  return NULL;
}

Elf32_Word
fixup_offset(Elf32_Word offset)
{
  if (offset >= hole_index) {
    return offset - hole_len;
  }
  return offset;
}

Elf32_Word
fixup_size(Elf32_Word offset, Elf32_Word size)
{
  /* Note: Doesn't handle the cases where the hole and the size intersect
     partially. */
  
  if ( (hole_index >= offset) &&
       (hole_index < offset + size)){
    return size - hole_len;
  }
  
  return size;
}

Elf32_Addr
fixup_addr(Elf32_Addr addr)
{
  if (addr == 0)
    return 0;

  /*
  if ( (addr < hole_addr_remap_start) ||
       (addr >= hole_addr_remap_end))
    return addr;
  */
  
  if (addr >= hole_addr_start) {
    return addr - hole_len;
  }
  return addr;
}

Elf32_Word
fixup_addr_size(Elf32_Addr addr, Elf32_Word size)
{
  /* Note: Doesn't handle the cases where the hole and the size intersect
     partially. */
  /*
  if ( (addr < hole_addr_remap_start) ||
       (addr >= hole_addr_remap_end))
    return size;
  */
  if ( (hole_addr_start >= addr) &&
       (hole_addr_start < addr + size)){
    return size - hole_len;
  }
  
  return size;
}

void
possibly_add_string(int name_idx, const char *name)
{
  struct dynamic_symbol *dynamic_symbol;
  if (name_idx != 0) {
    dynamic_symbol = g_hash_table_lookup(used_dynamic_symbols, (gpointer) name_idx);
    
    if (dynamic_symbol == NULL) {
      
      dynamic_symbol = g_new(struct dynamic_symbol, 1);
      
      dynamic_symbol->old_index = name_idx;
      dynamic_symbol->new_index = 0;
      dynamic_symbol->string = g_strdup(name);
      
      g_hash_table_insert(used_dynamic_symbols, (gpointer)name_idx, dynamic_symbol);
      /*printf("added dynamic string: %s (%d)\n", dynamic_symbol->string, name_idx);*/
    }
  }
}

Elf32_Word
fixup_string(Elf32_Word old_idx)
{
  struct dynamic_symbol *dynamic_symbol;

  if (old_idx == 0)
    return 0;
  
  dynamic_symbol = g_hash_table_lookup(used_dynamic_symbols, (gpointer) old_idx);

  if (dynamic_symbol == NULL) {
    fprintf(stderr, "AAAAAAAAAAAARGH!? Unknown string found in fixup (index: %d)!\n", old_idx);
    return 0;
  }
  
  return dynamic_symbol->new_index;
}



void
add_strings_from_dynsym(Elf32_Shdr *dynsym, char *strtab)
{
  Elf32_Sym *symbol;
  Elf32_Sym *symbol_end;
  Elf32_Word entry_size;
  

  symbol = (Elf32_Sym *)FILE_OFFSET(dynsym->sh_offset);
  symbol_end = (Elf32_Sym *)FILE_OFFSET(dynsym->sh_offset + dynsym->sh_size);
  entry_size = dynsym->sh_entsize;

  while (symbol < symbol_end) {
    int name_idx;
    struct dynamic_symbol *dynamic_symbol;

    name_idx = symbol->st_name;
    possibly_add_string(name_idx, &strtab[name_idx]);

    
    symbol = (Elf32_Sym *)((char *)symbol + entry_size);
  }
}


void
fixup_strings_in_dynsym(Elf32_Shdr *dynsym)
{
  Elf32_Sym *symbol;
  Elf32_Sym *symbol_end;
  Elf32_Word entry_size;
  

  symbol = (Elf32_Sym *)FILE_OFFSET(dynsym->sh_offset);
  symbol_end = (Elf32_Sym *)FILE_OFFSET(dynsym->sh_offset + dynsym->sh_size);
  entry_size = dynsym->sh_entsize;
  
  while (symbol < symbol_end) {
    int name_idx;
    struct dynamic_symbol *dynamic_symbol;

    symbol->st_name = fixup_string(symbol->st_name);
			 
    symbol = (Elf32_Sym *)((char *)symbol + entry_size);
  }
}


void
add_strings_from_dynamic(Elf32_Shdr *dynamic, char *strtab)
{
  int i;
  int name_idx;
  Elf32_Dyn *element;
  Elf32_Word entry_size;

  entry_size = dynamic->sh_entsize;
  

  element = (Elf32_Dyn *)FILE_OFFSET(dynamic->sh_offset);
  while (element->d_tag != DT_NULL) {

    switch(element->d_tag) {
    case DT_NEEDED:
    case DT_SONAME:
    case DT_RPATH:
      name_idx = element->d_un.d_val;
      /*if (name_idx) printf("d_tag: %d\n", element->d_tag);*/
      possibly_add_string(name_idx, &strtab[name_idx]);
      break;
    default:
      /*printf("unhandled d_tag: %d (0x%x)\n", element->d_tag, element->d_tag);*/
    }

    element = (Elf32_Dyn *)((char *)element + entry_size);
  }
  
}

void
fixup_strings_in_dynamic(Elf32_Shdr *dynamic)
{
  int i;
  int name_idx;
  Elf32_Dyn *element;
  Elf32_Word entry_size;

  entry_size = dynamic->sh_entsize;

  element = (Elf32_Dyn *)FILE_OFFSET(dynamic->sh_offset);
  while (element->d_tag != DT_NULL) {

    switch(element->d_tag) {
    case DT_NEEDED:
    case DT_SONAME:
    case DT_RPATH:
      element->d_un.d_val = fixup_string(element->d_un.d_val);
      break;
    default:
      /*printf("unhandled d_tag: %d (0x%x)\n", element->d_tag, element->d_tag);*/
    }

    element = (Elf32_Dyn *)((char *)element + entry_size);
  }
  
}


void
add_strings_from_ver_d(Elf32_Shdr *ver_d, char *strtab)
{
  Elf32_Verdaux *veraux;
  Elf32_Verdef *verdef;
  int num_aux;
  int name_idx;
  int i;
  int cont;

  verdef = (Elf32_Verdef *)FILE_OFFSET(ver_d->sh_offset);

  do {
    num_aux = verdef->vd_cnt;
    veraux = (Elf32_Verdaux *)((char *)verdef + verdef->vd_aux);
    for (i=0; i<num_aux; i++) {
      name_idx = veraux->vda_name;
      possibly_add_string(name_idx, &strtab[name_idx]);
      veraux = (Elf32_Verdaux *)((char *)veraux + veraux->vda_next);
    }

    cont = verdef->vd_next != 0;
    verdef = (Elf32_Verdef *)((char *)verdef + verdef->vd_next);
  } while (cont);
  
}

void
fixup_strings_in_ver_d(Elf32_Shdr *ver_d)
{
  Elf32_Verdaux *veraux;
  Elf32_Verdef *verdef;
  int num_aux;
  int name_idx;
  int i;
  int cont;

  verdef = (Elf32_Verdef *)FILE_OFFSET(ver_d->sh_offset);

  do {
    num_aux = verdef->vd_cnt;
    veraux = (Elf32_Verdaux *)((char *)verdef + verdef->vd_aux);
    for (i=0; i<num_aux; i++) {
      veraux->vda_name = fixup_string(veraux->vda_name);
      veraux = (Elf32_Verdaux *)((char *)veraux + veraux->vda_next);
    }

    cont = verdef->vd_next != 0;
    verdef = (Elf32_Verdef *)((char *)verdef + verdef->vd_next);
  } while (cont);
  
}

void
add_strings_from_ver_r(Elf32_Shdr *ver_r, char *strtab)
{
  Elf32_Vernaux *veraux;
  Elf32_Verneed *verneed;
  int num_aux;
  int name_idx;
  int i;
  int cont;

  verneed = (Elf32_Verneed *)FILE_OFFSET(ver_r->sh_offset);

  do {
    name_idx = verneed->vn_file;
    possibly_add_string(name_idx, &strtab[name_idx]);
    num_aux = verneed->vn_cnt;
    veraux = (Elf32_Vernaux *)((char *)verneed + verneed->vn_aux);
    for (i=0; i<num_aux; i++) {
      name_idx = veraux->vna_name;
      possibly_add_string(name_idx, &strtab[name_idx]);
      veraux = (Elf32_Vernaux *)((char *)veraux + veraux->vna_next);
    }

    cont = verneed->vn_next != 0;
    verneed = (Elf32_Verneed *)((char *)verneed + verneed->vn_next);
  } while (cont);
}

void
fixup_strings_in_ver_r(Elf32_Shdr *ver_r)
{
  Elf32_Vernaux *veraux;
  Elf32_Verneed *verneed;
  int num_aux;
  int name_idx;
  int i;
  int cont;

  verneed = (Elf32_Verneed *)FILE_OFFSET(ver_r->sh_offset);

  do {
    verneed->vn_file = fixup_string(verneed->vn_file);
    num_aux = verneed->vn_cnt;
    veraux = (Elf32_Vernaux *)((char *)verneed + verneed->vn_aux);
    for (i=0; i<num_aux; i++) {
      veraux->vna_name = fixup_string(veraux->vna_name);
      veraux = (Elf32_Vernaux *)((char *)veraux + veraux->vna_next);
    }

    cont = verneed->vn_next != 0;
    verneed = (Elf32_Verneed *)((char *)verneed + verneed->vn_next);
  } while (cont);
}

gboolean sum_size(gpointer	key,
		  struct dynamic_symbol *sym,
		  int *size)
{
  *size += strlen(sym->string) + 1;
  return 1;
}

struct index_n_dynstr {
  int index;
  char *dynstr;
};

gboolean output_string(gpointer	key,
		       struct dynamic_symbol *sym,
		       struct index_n_dynstr *x)
{
  sym->new_index = x->index;
  memcpy(x->dynstr + x->index, sym->string, strlen(sym->string) + 1);
  x->index += strlen(sym->string) + 1;
  return 1;
}


char *
generate_new_dynstr(Elf32_Word *size_out)
{
  int size;
  char *new_dynstr;
  struct index_n_dynstr x;

  size = 1; /* first a zero */
  g_hash_table_foreach	(used_dynamic_symbols,
			 (GHFunc)sum_size,
			 &size);


  new_dynstr = g_malloc(size);

  new_dynstr[0] = 0;
  x.index = 1;
  x.dynstr = new_dynstr;
  g_hash_table_foreach	(used_dynamic_symbols,
			 (GHFunc)output_string,
			 &x);
  
  *size_out = size;
  return new_dynstr;
}

void
remap_sections(void)
{
  Elf32_Shdr *section;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(elf_header->e_shoff);
  sectionsize = elf_header->e_shentsize;
  numsections = elf_header->e_shnum;

  for (i=0;i<numsections;i++) {
    section->sh_size = fixup_size(section->sh_offset, section->sh_size);
    section->sh_offset = fixup_offset(section->sh_offset);

    section->sh_addr = fixup_addr(section->sh_addr);
    
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
}


void
remap_segments(void)
{
  Elf32_Phdr *segment;
  Elf32_Word segmentsize;
  int numsegments;
  int i = 0;

  segment = (Elf32_Phdr *)FILE_OFFSET(elf_header->e_phoff);
  segmentsize = elf_header->e_phentsize;
  numsegments = elf_header->e_phnum;

  for (i=0;i<numsegments;i++) {
    segment->p_filesz = fixup_size(segment->p_offset, segment->p_filesz);
    segment->p_offset = fixup_offset(segment->p_offset);

    segment->p_memsz = fixup_addr_size(segment->p_vaddr, segment->p_memsz);
    segment->p_vaddr = fixup_addr(segment->p_vaddr);
    segment->p_paddr = segment->p_vaddr;
    
    segment = (Elf32_Phdr *)((char *)segment + segmentsize);
  }
}

void
remap_elf_header(void)
{
    elf_header->e_phoff = fixup_offset(elf_header->e_phoff);
    elf_header->e_shoff = fixup_offset(elf_header->e_shoff);

    elf_header->e_entry = fixup_addr(elf_header->e_entry);
}

void
remap_symtab(Elf32_Shdr *symtab)
{
  Elf32_Sym *symbol;
  Elf32_Sym *symbol_end;
  Elf32_Word entry_size;

  symbol = (Elf32_Sym *)FILE_OFFSET(symtab->sh_offset);
  symbol_end = (Elf32_Sym *)FILE_OFFSET(symtab->sh_offset + symtab->sh_size);
  entry_size = symtab->sh_entsize;

  while (symbol < symbol_end) {
    symbol->st_value = fixup_addr(symbol->st_value);
    symbol = (Elf32_Sym *)((char *)symbol + entry_size);
  }
}


/* Ugly global variables: */
Elf32_Addr got_data_start = 0;
Elf32_Addr got_data_end = 0;


void
remap_rel_section(Elf32_Rel *rel, Elf32_Word size, Elf32_Word entry_size)
{
  Elf32_Rel *rel_end;
  Elf32_Word offset;
  Elf32_Addr *addr;

  rel_end = (Elf32_Rel *)((char *)rel + size);

  while (rel < rel_end) {
    if (ELF32_R_TYPE(rel->r_info) == R_386_RELATIVE) {
	  /* We need to relocate the data this is pointing to too. */
	  offset = vma_to_offset(rel->r_offset);

	  if ( (offset >= got_data_start) &&
	       (offset < got_data_end) ) {
	    /*printf("RELATIVE REL in .rel.got, skipping\n");*/
	  } else {
	    addr =  (Elf32_Addr *)FILE_OFFSET(offset);
	    *addr = fixup_addr(*addr);
	  }
    }

    rel->r_offset = fixup_addr(rel->r_offset);
    
    rel = (Elf32_Rel *)((char *)rel + entry_size);
  }
}

void
remap_rela_section(Elf32_Rela *rela, Elf32_Word size, Elf32_Word entry_size)
{
  Elf32_Rela *rela_end;
  Elf32_Addr *addr;
  Elf32_Word offset;

  rela_end = (Elf32_Rela *)((char *)rela + size);

  while (rela < rela_end) {
    if (ELF32_R_TYPE(rela->r_info) == R_386_RELATIVE) {
	  /* We need to relocate the data this is pointing to too. */
	  offset = vma_to_offset(rela->r_offset);

	  if ( (offset >= got_data_start) &&
	       (offset < got_data_end) ) {
	    /*printf("RELATIVE RELA in .rel.got, skipping\n");*/
	  } else {
	    addr =  (Elf32_Addr *)FILE_OFFSET(offset);
	    *addr = fixup_addr(*addr);
	  }
    }

    rela->r_offset = fixup_addr(rela->r_offset);
    
    rela = (Elf32_Rela *)((char *)rela + entry_size);
  }
}


void
remap_reloc(void)
{
  Elf32_Shdr *section;
  Elf32_Word sectionsize;
  Elf32_Word offset;
  int numsections;
  int i = 0;

  /* This is old code. relocations are now handled from remap_dynamic() */
  section = (Elf32_Shdr *)FILE_OFFSET(elf_header->e_shoff);
  sectionsize = elf_header->e_shentsize;
  numsections = elf_header->e_shnum;

  for (i=0;i<numsections;i++) {
    if (section->sh_type == SHT_REL) {
      Elf32_Rel *rel;

      rel = (Elf32_Rel *)FILE_OFFSET(section->sh_offset);
      remap_rel_section(rel, section->sh_size, section->sh_entsize);
      
    } else if (section->sh_type == SHT_RELA) {
      Elf32_Rela *rel;

      rel = (Elf32_Rela *)FILE_OFFSET(section->sh_offset);
      remap_rela_section(rel, section->sh_size, section->sh_entsize);
    }
    
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
}


void 
remap_i386_got(void)
{
  Elf32_Shdr *got_section;
  Elf32_Addr *got;
  Elf32_Addr *got_end;
  Elf32_Word entry_size;

  got_section = elf_find_section_named(".got");
  if (got_section == NULL) {
    fprintf(stderr, "Warning, no .got section\n");
    return;
  }

  got_data_start = got_section->sh_offset;
  got_data_end = got_section->sh_offset + got_section->sh_size;
  
  got = (Elf32_Addr *)FILE_OFFSET(got_section->sh_offset);
  got_end = (Elf32_Addr *)FILE_OFFSET(got_section->sh_offset + got_section->sh_size);
  entry_size = got_section->sh_entsize;

  *got= fixup_addr(*got); /* Pointer to .dynamic */
  got = (Elf32_Addr *)((char *)got + 2*entry_size); /* Skip two reserved entries. */
  
  
  while (got < got_end) {
    if (*got != 0)
      *got= fixup_addr(*got);
    
    got = (Elf32_Addr *)((char *)got + entry_size);
  }
}

Elf32_Word
get_dynamic_val(Elf32_Shdr *dynamic, Elf32_Sword tag)
{
  Elf32_Dyn *element;
  Elf32_Word entry_size;

  entry_size = dynamic->sh_entsize;

  element = (Elf32_Dyn *)FILE_OFFSET(dynamic->sh_offset);
  while (element->d_tag != DT_NULL) {
    if (element->d_tag == tag) {
      return element->d_un.d_val;
    }
    element = (Elf32_Dyn *)((char *)element + entry_size);
  }
  return 0;
}

void
remap_dynamic(Elf32_Shdr *dynamic, Elf32_Word new_dynstr_size)
{
  Elf32_Dyn *element;
  Elf32_Word entry_size;
  Elf32_Word rel_size;
  Elf32_Word rel_entry_size;
  Elf32_Rel *rel;
  Elf32_Rela *rela;

  entry_size = dynamic->sh_entsize;

  element = (Elf32_Dyn *)FILE_OFFSET(dynamic->sh_offset);
  while (element->d_tag != DT_NULL) {
    switch(element->d_tag) {
    case DT_STRSZ:
      element->d_un.d_val = new_dynstr_size;
      break;
    case DT_PLTGOT:
    case DT_HASH:
    case DT_STRTAB:
    case DT_INIT:
    case DT_FINI:
    case DT_VERDEF:
    case DT_VERNEED:
    case DT_VERSYM:
      element->d_un.d_ptr = fixup_addr(element->d_un.d_ptr);
      break;
    case DT_JMPREL:
      rel_size = get_dynamic_val(dynamic, DT_PLTRELSZ);
      if (get_dynamic_val(dynamic, DT_PLTREL) == DT_REL) {
	rel_entry_size = get_dynamic_val(dynamic, DT_RELENT);
	rel = (Elf32_Rel *)FILE_OFFSET(vma_to_offset(element->d_un.d_ptr));
	remap_rel_section(rel, rel_size, rel_entry_size);
      } else {
	rel_entry_size = get_dynamic_val(dynamic, DT_RELAENT);
	rela = (Elf32_Rela *)FILE_OFFSET(vma_to_offset(element->d_un.d_ptr));
	remap_rela_section(rela, rel_size, rel_entry_size);
      }

      element->d_un.d_ptr = fixup_addr(element->d_un.d_ptr);
      break;
    case DT_REL:
      rel_size = get_dynamic_val(dynamic, DT_RELSZ);
      rel_entry_size = get_dynamic_val(dynamic, DT_RELENT);
      rel = (Elf32_Rel *)FILE_OFFSET(vma_to_offset(element->d_un.d_ptr));
      remap_rel_section(rel, rel_size, rel_entry_size);
      
      element->d_un.d_ptr = fixup_addr(element->d_un.d_ptr);
      break;
    case DT_RELA:
      rel_size = get_dynamic_val(dynamic, DT_RELASZ);
      rel_entry_size = get_dynamic_val(dynamic, DT_RELAENT);
      rela = (Elf32_Rela *)FILE_OFFSET(vma_to_offset(element->d_un.d_ptr));
      remap_rela_section(rela, rel_size, rel_entry_size);

      element->d_un.d_ptr = fixup_addr(element->d_un.d_ptr);
      break;
    default:
      /*printf("unhandled d_tag: %d (0x%x)\n", element->d_tag, element->d_tag);*/
    }

    element = (Elf32_Dyn *)((char *)element + entry_size);
  }
}

align_hole(Elf32_Word *start, Elf32_Word *end)
{
  Elf32_Word len;
  Elf32_Word align;
  Elf32_Shdr *section;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;
  int unaligned;
  
  len = *end - *start;
  align = 0;
    
  sectionsize = elf_header->e_shentsize;
  numsections = elf_header->e_shnum;
  do {
    section = (Elf32_Shdr *)FILE_OFFSET(elf_header->e_shoff);
    unaligned = 0;
    
    for (i=0;i<numsections;i++) {
      if ( (section->sh_addralign > 1) &&
	   ( (section->sh_offset-len + align)%section->sh_addralign != 0) ) {
	unaligned = 1;
      }
      
      section = (Elf32_Shdr *)((char *)section + sectionsize);
    }

    if (unaligned) {
      align++;
    }
      
  } while (unaligned);

  *start += align;
}

int
main(int argc, char *argv[])
{
  int fd;
  unsigned char *mapping;
  Elf32_Word size;
  struct stat statbuf;
  Elf32_Shdr *dynamic;
  Elf32_Shdr *dynsym;
  Elf32_Shdr *symtab;
  Elf32_Shdr *dynstr;
  Elf32_Shdr *hash;
  Elf32_Shdr *higher_section;
  int dynstr_index;
  Elf32_Shdr *ver_r;
  Elf32_Shdr *ver_d;
  char *dynstr_data;
  char *new_dynstr;
  Elf32_Word old_dynstr_size;
  Elf32_Word new_dynstr_size;
  
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    return 1;
  }

  fd = open(argv[1], O_RDWR);
  if (fd == -1) {
    fprintf(stderr, "Cannot open file %s\n", argv[1]);
    return 1;
  }
  
  if (fstat(fd, &statbuf) == -1) {
    fprintf(stderr, "Cannot stat file %s\n", argv[1]);
    return 1;
  }
  
  size = statbuf.st_size;
    
  mapping = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (mapping == (unsigned char *)-1) {
    fprintf(stderr, "Cannot mmap file %s\n", argv[1]);
    return 1;
  }

  used_dynamic_symbols = g_hash_table_new(g_direct_hash, g_direct_equal);

  elf_header = (Elf32_Ehdr *)mapping;

  if (strncmp((void *)elf_header, ELFMAG, SELFMAG)!=0) {
    fprintf(stderr, "Not an ELF file\n");
    return 1;
  }

  if (elf_header->e_ident[EI_VERSION] != EV_CURRENT) {
    fprintf(stderr, "Wrong ELF file version\n");
    return 1;
  }

  if (elf_header->e_ident[EI_CLASS] != ELFCLASS32) {
    fprintf(stderr, "Only 32bit ELF files supported\n");
    return 1;
  }

  if ( (elf_header->e_ident[EI_DATA] != ELFDATA2LSB) ||
       (elf_header->e_machine !=  EM_386) ) {
    fprintf(stderr, "Only intel LSB binaries are supported right now\n");
    return 1;
  }

  if (elf_header->e_type != ET_DYN) {
    fprintf(stderr, "Not an ELF shared object\n");
    return 1;
  }
  
  dynamic = elf_find_section(SHT_DYNAMIC);
  dynsym = elf_find_section(SHT_DYNSYM);
  symtab = elf_find_section(SHT_SYMTAB);
  dynstr_index = dynsym->sh_link;
  dynstr = elf_find_section_num(dynstr_index);
  dynstr_data = FILE_OFFSET(dynstr->sh_offset);
  old_dynstr_size = dynstr->sh_size;
  ver_d = elf_find_section(SHT_GNU_verdef);
  ver_r = elf_find_section(SHT_GNU_verneed);
  hash = elf_find_section(SHT_HASH);

  /* Generate hash table with all used strings: */
  
  add_strings_from_dynsym(dynsym, dynstr_data);
  add_strings_from_dynamic(dynamic, dynstr_data);
  if (ver_d && (ver_d->sh_link == dynstr_index))
    add_strings_from_ver_d(ver_d, dynstr_data);
  if (ver_r && (ver_r->sh_link == dynstr_index))
    add_strings_from_ver_r(ver_r, dynstr_data);

  /* Generate new dynstr section from the used strings hashtable: */
  
  new_dynstr = generate_new_dynstr(&new_dynstr_size);
  /*
  printf("New dynstr size: %d\n", new_dynstr_size);
  printf("Old dynstr size: %d\n", old_dynstr_size);
  */
  
  if (new_dynstr_size >= old_dynstr_size) {
    fprintf(stderr, "Couldn't GC any strings, exiting.\n");
    return 0;
  }

  /* Fixup all references: */
  
  fixup_strings_in_dynsym(dynsym);
  fixup_strings_in_dynamic(dynamic);
  if (ver_d && (ver_d->sh_link == dynstr_index))
    fixup_strings_in_ver_d(ver_d);
  if (ver_r && (ver_r->sh_link == dynstr_index))
    fixup_strings_in_ver_r(ver_r);
  
  /* Copy over the new dynstr: */
  memcpy(dynstr_data, new_dynstr, new_dynstr_size);
  memset(dynstr_data + new_dynstr_size, ' ', old_dynstr_size-new_dynstr_size);

  /* Compact the dynstr section and the file: */

  /* 1. Set up the data for the fixup_offset() function: */
  hole_index = dynstr->sh_offset + new_dynstr_size;
  higher_section = elf_find_next_higher_section(hole_index);
  hole_end = higher_section->sh_offset;

  align_hole(&hole_index, &hole_end);
  
  hole_len = hole_end - hole_index;

  hole_addr_start = hole_index; /* TODO: Fix this to something better */

  find_segment_addr_min_max(dynstr->sh_offset,
			    &hole_addr_remap_start, &hole_addr_remap_end);
  
  /*
  printf("Hole remap: 0x%lx - 0x%lx\n", hole_addr_remap_start, hole_addr_remap_end);

  printf("hole: %lu - %lu (%lu bytes)\n", hole_index, hole_end, hole_len);
  printf("hole: 0x%lx - 0x%lx (0x%lx bytes)\n", hole_index, hole_end, hole_len);
  */
  
  /* 2. Change all section and segment sizes and offsets: */
  
  remap_symtab(dynsym);
  if (symtab)
    remap_symtab(symtab);
  
  remap_i386_got();
  remap_dynamic(dynamic, new_dynstr_size);
  
  remap_sections(); /* After this line the section headers are wrong */
  remap_segments();
  remap_elf_header();
    
  /* 3. Do the real compacting. */

  memmove(mapping + hole_index,
	  mapping + hole_index + hole_len,
	  size - (hole_index + hole_len));
  
  munmap(mapping, size);

  ftruncate(fd, size - hole_len);
  close(fd);
}




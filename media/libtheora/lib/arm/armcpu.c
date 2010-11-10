/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2010                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

 CPU capability detection for ARM processors.

 function:
  last mod: $Id: cpu.c 17344 2010-07-21 01:42:18Z tterribe $

 ********************************************************************/

#include "armcpu.h"

#if !defined(OC_ARM_ASM)|| \
 !defined(OC_ARM_ASM_EDSP)&&!defined(OC_ARM_ASM_ARMV6)&& \
 !defined(OC_ARM_ASM_NEON)
ogg_uint32_t oc_cpu_flags_get(void){
  return 0;
}

#elif defined(_MSC_VER)
/*For GetExceptionCode() and EXCEPTION_ILLEGAL_INSTRUCTION.*/
# define WIN32_LEAN_AND_MEAN
# define WIN32_EXTRA_LEAN
# include <windows.h>

ogg_uint32_t oc_cpu_flags_get(void){
  ogg_uint32_t flags;
  flags=0;
  /*MSVC has no inline __asm support for ARM, but it does let you __emit
     instructions via their assembled hex code.
    All of these instructions should be essentially nops.*/
# if defined(OC_ARM_ASM_EDSP)
  __try{
    /*PLD [r13]*/
    __emit(0xF5DDF000);
    flags|=OC_CPU_ARM_EDSP;
  }
  __except(GetExceptionCode()==EXCEPTION_ILLEGAL_INSTRUCTION){
    /*Ignore exception.*/
  }
#  if defined(OC_ARM_ASM_MEDIA)
  __try{
    /*SHADD8 r3,r3,r3*/
    __emit(0xE6333F93);
    flags|=OC_CPU_ARM_MEDIA;
  }
  __except(GetExceptionCode()==EXCEPTION_ILLEGAL_INSTRUCTION){
    /*Ignore exception.*/
  }
#   if defined(OC_ARM_ASM_NEON)
  __try{
    /*VORR q0,q0,q0*/
    __emit(0xF2200150);
    flags|=OC_CPU_ARM_NEON;
  }
  __except(GetExceptionCode()==EXCEPTION_ILLEGAL_INSTRUCTION){
    /*Ignore exception.*/
  }
#   endif
#  endif
# endif
  return flags;
}

#elif defined(__linux__)
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

ogg_uint32_t oc_cpu_flags_get(void){
  ogg_uint32_t  flags;
  FILE         *fin;
  flags=0;
  /*Reading /proc/self/auxv would be easier, but that doesn't work reliably on
     Android.
    This also means that detection will fail in Scratchbox.*/
  fin=fopen("/proc/cpuinfo","r");
  if(fin!=NULL){
    /*512 should be enough for anybody (it's even enough for all the flags that
       x86 has accumulated... so far).*/
    char buf[512];
    while(fgets(buf,511,fin)!=NULL){
      if(memcmp(buf,"Features",8)==0){
        char *p;
        p=strstr(buf," edsp");
        if(p!=NULL&&(p[5]==' '||p[5]=='\n'))flags|=OC_CPU_ARM_EDSP;
        p=strstr(buf," neon");
        if(p!=NULL&&(p[5]==' '||p[5]=='\n'))flags|=OC_CPU_ARM_NEON;
      }
      if(memcmp(buf,"CPU architecture:",17)==0){
        int version;
        version=atoi(buf+17);
        if(version>=6)flags|=OC_CPU_ARM_MEDIA;
      }
    }
    fclose(fin);
  }
  return flags;
}

#else
/*The feature registers which can tell us what the processor supports are
   accessible in priveleged modes only, so we can't have a general user-space
   detection method like on x86.*/
# error "Configured to use ARM asm but no CPU detection method available for " \
 "your platform.  Reconfigure with --disable-asm (or send patches)."
#endif

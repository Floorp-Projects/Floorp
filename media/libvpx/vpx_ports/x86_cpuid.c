/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include "x86.h"

struct cpuid_vendors {
  char vendor_string[12];
  vpx_cpu_t vendor_id;
};

static struct cpuid_vendors cpuid_vendor_list[VPX_CPU_LAST] = {
  { "AuthenticAMD", VPX_CPU_AMD           },
  { "AMDisbetter!", VPX_CPU_AMD_OLD       },
  { "CentaurHauls", VPX_CPU_CENTAUR       },
  { "CyrixInstead", VPX_CPU_CYRIX         },
  { "GenuineIntel", VPX_CPU_INTEL         },
  { "NexGenDriven", VPX_CPU_NEXGEN        },
  { "Geode by NSC", VPX_CPU_NSC           },
  { "RiseRiseRise", VPX_CPU_RISE          },
  { "SiS SiS SiS ", VPX_CPU_SIS           },
  { "GenuineTMx86", VPX_CPU_TRANSMETA     },
  { "TransmetaCPU", VPX_CPU_TRANSMETA_OLD },
  { "UMC UMC UMC ", VPX_CPU_UMC           },
  { "VIA VIA VIA ", VPX_CPU_VIA           },
};

vpx_cpu_t vpx_x86_vendor(void) {
  unsigned int reg_eax;
  unsigned int vs[3];
  int i;

  /* Get the Vendor String from the CPU */
  cpuid(0, reg_eax, vs[0], vs[2], vs[1]);

  for (i = 0; i < VPX_CPU_LAST; i++) {
    if (strncmp((const char *)vs, cpuid_vendor_list[i].vendor_string, 12) == 0)
      return (cpuid_vendor_list[i].vendor_id);
  }

  return VPX_CPU_UNKNOWN;
}

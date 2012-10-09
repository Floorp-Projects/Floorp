/**
   nr_common.h

   
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved
   
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote 
      products derived from this software without specific prior written
      permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
   
 */


#ifndef _nr_common_h
#define _nr_common_h

#include <csi_platform.h>

#ifdef USE_MPATROL
#define USEDEBUG 1
#include <mpatrol.h>
#endif

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#include <string.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#include <errno.h>
#else
#include <sys/errno.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <sys/queue.h>
#include <r_log.h>

extern int NR_LOG_REASSD;

#include "registry.h"
#include "nrstats.h"
 
typedef struct nr_captured_packet_ {
     UCHAR cap_interface;       /* 1 for primary, 2 for secondary */
     struct timeval ts;     /* The time this packet was captured */
     UINT4 len;             /* The length of the packet */
     UINT8 packet_number;   /* The listener's packet index */
} nr_captured_packet;

#ifndef NR_ROOT_PATH
#define NR_ROOT_PATH "/usr/local/ctc/"
#endif

#define NR_ARCHIVE_DIR NR_ROOT_PATH  "archive/"
#define NR_TEMP_DIR NR_ROOT_PATH  "tmp/"
#define NR_ARCHIVE_STATEFILE NR_ROOT_PATH  "archive/state"
#define NR_CAPTURED_PID_FILENAME  NR_ROOT_PATH "captured.pid"
#define NR_REASSD_PID_FILENAME  NR_ROOT_PATH "reassd.pid"
#define NR_MODE_FILENAME  NR_ROOT_PATH "mode.txt"

char *nr_revision_number(void);




/* Memory buckets for CTC memory types */
#define NR_MEM_TCP       1
#define NR_MEM_HTTP      2
#define NR_MEM_DELIVERY  3
#define NR_MEM_OUT_HM    4
#define NR_MEM_OUT_SSL   5
#define NR_MEM_SSL       7
#define NR_MEM_COMMON    8
#define NR_MEM_CODEC     9

#endif


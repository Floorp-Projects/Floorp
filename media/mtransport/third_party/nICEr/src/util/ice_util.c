/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdarg.h>


static char *RCSSTRING __UNUSED__="$Id: ice_util.c,v 1.2 2008/04/28 17:59:05 ekr Exp $";

#include <stdarg.h>
#include <string.h>
#include "nr_api.h"
#include "ice_util.h"

int nr_concat_strings(char **outp,...)
  {
    va_list ap;
    char *s,*out=0;
    int len=0;
    int _status;

    va_start(ap,outp);
    while(s=va_arg(ap,char *)){
      len+=strlen(s);
    }
    va_end(ap);


    if(!(out=RMALLOC(len+1)))
      ABORT(R_NO_MEMORY);
    
    *outp=out;

    va_start(ap,outp);
    while(s=va_arg(ap,char *)){
      len=strlen(s);
      memcpy(out,s,len);
      out+=len;
    }
    va_end(ap);

    *out=0;
    
    _status=0;
  abort:
    return(_status);
  }


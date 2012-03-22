/* libunwind - a platform-independent unwind library
   Copyright (C) 2003 Hewlett-Packard Co
	Contributed by ...

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

#include "unwind_i.h"

static inline int
common_init (struct cursor *c, unsigned use_prev_instr)
{
  int ret;

  c->ip_loc = HPPA_REG_LOC (c, UNW_HPPA_IP);
  c->sp_loc = HPPA_REG_LOC (c, UNW_HPPA_SP);

  ret = hppa_get (c, c->ip_loc, &c->ip);
  if (ret < 0)
    return ret;

  ret = hppa_get (c, HPPA_REG_LOC (c, UNW_HPPA_SP), &c->sp);
  if (ret < 0)
    return ret;

  c->dwarf.stash_frames = 0;
  c->dwarf.use_prev_instr = use_prev_instr;
  return 0;
}

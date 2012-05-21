/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <libunwind.h>

unw_word_t
_ReadSLEB (unsigned char **dpp)
{
  unsigned shift = 0;
  unw_word_t byte, result = 0;
  unsigned char *bp = *dpp;

  while (1)
    {
      byte = *bp++;
      result |= (byte & 0x7f) << shift;
      shift += 7;
      if ((byte & 0x80) == 0)
	break;
    }

  if (shift < 8 * sizeof (unw_word_t) && (byte & 0x40) != 0)
    /* sign-extend negative value */
    result |= ((unw_word_t) -1) << shift;

  *dpp = bp;
  return result;
}

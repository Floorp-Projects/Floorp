/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "secplcy.h"
#include "prmem.h"

SECCipherFind *sec_CipherFindInit(PRBool onlyAllowed,
				  secCPStruct *policy,
				  long *ciphers)
{
  SECCipherFind *find = PR_NEWZAP(SECCipherFind);
  if (find)
    {
      find->policy = policy;
      find->ciphers = ciphers;
      find->onlyAllowed = onlyAllowed;
      find->index = -1;
    }
  return find;
}

long sec_CipherFindNext(SECCipherFind *find)
{
  char *policy;
  long rv = -1;
  secCPStruct *policies = (secCPStruct *) find->policy;
  long *ciphers = (long *) find->ciphers;
  long numCiphers = policies->num_ciphers;

  find->index++;
  while((find->index < numCiphers) && (rv == -1))
    {
      /* Translate index to cipher. */
      rv = ciphers[find->index];

      /* If we're only looking for allowed ciphers, and if this
	 cipher isn't allowed, loop around.*/
      if (find->onlyAllowed)
	{
	  /* Find the appropriate policy flag. */
	  policy = (&(policies->begin_ciphers)) + find->index + 1;

	  /* If this cipher isn't allowed by policy, continue. */
	  if (! (*policy))
	    {
	      rv = -1;
	      find->index++;
	    }
	}
    }

  return rv;
}

char sec_IsCipherAllowed(long cipher, secCPStruct *policies,
			 long *ciphers)
{
  char result = SEC_CIPHER_NOT_ALLOWED; /* our default answer */
  long numCiphers = policies->num_ciphers;
  char *policy;
  int i;
  
  /* Convert the cipher number into a policy flag location. */
  for (i=0, policy=(&(policies->begin_ciphers) + 1);
       i<numCiphers;
       i++, policy++)
    {
      if (cipher == ciphers[i])
	break;
    }

  if (i < numCiphers)
    {
      /* Found the cipher, get the policy value. */
      result = *policy;
    }

  return result;
}

void sec_CipherFindEnd(SECCipherFind *find)
{
  PR_FREEIF(find);
}

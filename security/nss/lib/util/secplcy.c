/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

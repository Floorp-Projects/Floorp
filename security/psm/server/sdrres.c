/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "resource.h"
#include "minihttp.h"
#include "advisor.h"
#include "sdrres.h"

#define SSMRESOURCE(object) (&(object)->super)

/*
 * SDRContext_Create
 *    Create and initialize an SDR context resource object
 */
SSMStatus
SSMSDRContext_Create(void *arg, SSMControlConnection *ctrl, SSMResource **res)
{
  SSMStatus rv = SSM_SUCCESS;
  SSMSDRContext *sdr = 0;

  *res = NULL;

  sdr = SSM_ZNEW(SSMSDRContext);
  if (sdr == NULL) { rv = SSM_FAILURE; goto done; }

  rv = SSMResource_Init(ctrl, SSMRESOURCE(sdr), SSM_RESTYPE_SDR_CONTEXT);
  if (rv != SSM_SUCCESS) goto done;

  /* Init SDR fields here */

  /* Return the new value */
  *res = SSMRESOURCE(sdr);
  sdr = NULL;

done:
  if (sdr) SSM_FreeResource(SSMRESOURCE(sdr));

  SSM_DEBUG("SDRContext_Create (%d) %lx\n", rv, *res);

  return rv;
}

/*
 * SDRContext_Destroy
 *     Destroy contents of resource and optionally free the memory.
 *     NOTE: doFree should always be PR_TRUE, since this type is not
 *        subclassed.
 */
SSMStatus
SSMSDRContext_Destroy(SSMResource *res, PRBool doFree)
{
  SSMStatus rv;
  SSMSDRContext *sdr = (SSMSDRContext *)res;

  rv = SSMResource_Destroy(res, PR_FALSE);
  if (rv != SSM_SUCCESS) goto done;

  if (doFree) PR_Free(sdr);

done:
  SSM_DEBUG("SDRContext_Destroy (%d)\n", rv);

  return rv;
}

/*
 * SSMSDRContext_FormSubmitHandler
 */
SSMSDRContext_FormSubmitHandler(SSMResource * res, HTTPRequest * req)
{
    SSMStatus rv;

    SSM_DEBUG("SSMSDRContext_FormSubmit\n");
 
    if (!res->m_formName)
        goto loser;
    if (PL_strcmp(res->m_formName, "set_db_password") == 0)
        rv = SSM_SetDBPasswordHandler(req);
    else /* other cases where this could be used will go here */
        goto loser;

    SSM_DEBUG("SSMSDRContext_FormSubmit (%d)\n", rv);

    return rv;
 loser:
    SSM_DEBUG("FormSubmit handler is called with no valid formName\n");
    SSM_NotifyUIEvent(res);
    return SSM_FAILURE;
}

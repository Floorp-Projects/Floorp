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
#include "obspriv.h"
#include "newproto.h"
#include <string.h>
#include <assert.h>
#include <time.h>

/* 
   Originally this code was used to obscure the control messages
   traveling between processes. With the relaxation of export rules,
   this whole step is no longer necessary, and is included for
   informational purposes only. (We need to finish removing the 
   obscuring code.)
*/
struct obscureNOPStr {
    SSMObscureObject * obj;
};

typedef struct obscureNOPStr obscureV1;

static int 
ssmObscure_Destroy(void * privData)
{
    obscureV1 * priv = (obscureV1 *)privData;

    memset(priv, 0, sizeof *priv);
    cmt_free(priv);
    return 0;
}

static int
ssmObscure_Send(void * privData, void * buf, unsigned int len)
{
    /* obscureV1 *  priv 		= (obscureV1 *)privData;*/

    /* NOP */
    return len;
}

static int    
ssmObscure_Recv(void * privData, void * buf, unsigned int len)
{
    /*obscureV1 *  priv 		= (obscureV1 *)privData;*/

    /* NOP */
    return len;
}

static int    
ssmObscure_SendInit(void * privData, void * buf)
{
    /*obscureV1 *      priv = (obscureV1 *)privData;*/

    return 0;
}

static int    
ssmObscure_RecvInit(void * privData, void * buf, unsigned int len,
                    SSMObscureBool * pDone)
{
    return 0;
}

static void *
ssmObscure_InitPrivate(SSMObscureObject * obj, SSMObscureBool IsServer)
{
    obscureV1 *   priv 		= (obscureV1 *) cmt_alloc(sizeof (obscureV1));

    if (!priv)
    	return NULL;

    priv->obj        = obj;

    obj->privData  = (void *)priv;
    obj->destroy  = ssmObscure_Destroy;
    obj->send     = ssmObscure_Send;
    obj->recv     = ssmObscure_Recv;
    obj->sendInit = ssmObscure_SendInit;
    obj->recvInit = ssmObscure_RecvInit;
    
    return priv;
}

obsInitFn SSMObscure_InitPrivate = ssmObscure_InitPrivate;

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
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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
/*
 *  SSLEVIL
 *
 *  Wrappers to callback in the mozilla thread
 *
 *  Certificate code is not threadsafe.
 *  We compensate by using it only on the mozilla thread.
 *  These functions push an event on the XXX queue 
 *  to cause the cert function to run in that thread. 
 *  All grok xp_error properly.
 *
 */


/* Special macros facilitate running on Win 16 */
#if defined(XP_PC) && !defined(_WIN32)   /* then we are win 16 */ 

  /* 
   * Allocate the data passed to the callback functions from the heap...
   *
   * This inter-thread structure cannot reside on a thread stack since the 
   * thread's stack is swapped away with the thread under Win16...
   */

#define ALLOC_OR_DEFINE(type, pointer_var, out_of_memory_action) \
    type * pointer_var = PORT_ZAlloc(sizeof(type));     \
    do {                                                \
        if (!pointer_var)                               \
            out_of_memory_action;                       \
    } while (0)             /* and now a semicolon can follow :-) */

#define FREE_IF_ALLOC_IS_USED(pointer_var) PORT_Free(pointer_var)

#else /* not win 16... so we can alloc via auto variables */

#define ALLOC_OR_DEFINE(type, pointer_var, out_of_memory_action) \
    type   actual_ ## pointer_var;                      \
    type * pointer_var = &actual_ ## pointer_var;       \
    PORT_Memset (pointer_var, 0, sizeof (type));        \
    ((void) 0)               /* and now a semicolon can follow  */

 #define FREE_IF_ALLOC_IS_USED(pointer_var_name) ((void) 0)

#endif /* not Win 16 */

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

#if 0	/* EXAMPLE */
/*
 *  SOB_MOZ_dup
 *
 *  Call CERT_DupCertificate inside
 *  the mozilla thread
 *
 */

struct EVIL_dup
{
  int error;
  CERTCertificate *cert;
  CERTCertificate *return_cert;
};


/* This is called inside the mozilla thread */

PR_STATIC_CALLBACK(void) sob_moz_dup_fn (void *data)
{
  CERTCertificate *return_cert;
  struct EVIL_dup *dup_data = (struct EVIL_dup *)data;

  PORT_SetError (dup_data->error);

  return_cert = CERT_DupCertificate (dup_data->cert);

  dup_data->return_cert = return_cert;
  dup_data->error = PORT_GetError();
}


/* Wrapper for the ET_MOZ call */
 
CERTCertificate *sob_moz_dup (CERTCertificate *cert)
{
  CERTCertificate *dup_cert;
  ALLOC_OR_DEFINE(struct EVIL_dup, dup_data, NULL);

  dup_data->error = PORT_GetError();
  dup_data->cert  = cert;

  /* Synchronously invoke the callback function on the mozilla thread. */
  if (mozilla_event_queue)
    ET_moz_CallFunction (sob_moz_dup_fn, dup_data);
  else
    sob_moz_dup_fn (dup_data);

  PORT_SetError (dup_data->error);
  dup_cert = dup_data->return_cert;

  /* Free the data passed to the callback function... */
  FREE_IF_ALLOC_IS_USED(dup_data);
  return dup_cert;
}

#endif /* EXAMPLE */
/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

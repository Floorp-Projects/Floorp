/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

/*
  SSMResource is the base class for anything that wants to be accessible 
  to the NSM client.

  To subclass from SSMResource, you *must* do the following:

  * Define a resource type for your class in the SSMResourceType enumerated
  type below.

  * Embed an SSMResource struct as the first member of your class struct.
  This is the way that polymorphism works in this class system: you can freely
  change type on a pointer to an SSMResource or its subclasses, because
  the memory pointed to is guaranteed to consist of the base class followed
  by subclass members. Subsequently, to subclass from any subclass of
  SSMResource, you must embed an instance of the existing class as the 
  first member of your subclass.

  (This is how it's done in C++, btw. Why don't we use C++, you ask?
  Because of variations among C++ compilers, and because the rest of the
  group will hate us if we do.)

  * Provide the following member functions for your class:

    - SSMStatus YourClassName_Create (void *arg, SSMResource **res)
      Requirement: MANDATORY
      Description: This creates an instance of your class. Necessary because
                   you need to allocate a properly sized chunk of memory,
                   and because the _Create routine also must call the correct
                   _Init method to properly initialize member data.
      Parameters:  arg - an initialization argument specific to the class.
                   res - Returns the newly created instance, or NULL if error.
                   Return value - PR_SUCCESS for success, etc.

    - SSMStatus YourClassName_Init([signature varies])
      Requirement: MANDATORY
      Description: This initializes member data in your class. 
      Parameters:  Dependent on how your class needs to be initialized. 
                   The calling convention is used only by your _Create method,
                   and by the _Init method of all subclasses of your class.

    - SSMStatus YourClassName_Destroy(SSMResource *res, PRBool doFree)
      Requirement: MANDATORY
      Description: This deallocates member data in an instance of your 
                   class, and optionally deallocates the class instance 
                   itself (if doFree is true). When calling superclass
                   _Destroy functions, should always pass PR_FALSE in
                   doFree. The default method deallocates the object with
                   PR_Free.
      Parameters:  res - An instance of your class.
                   doFree - if PR_TRUE, you must free (res). Presumably,
                            if doFree is true, then this object was created
                            by your _Create method above, so use whatever
                            memory deallocator corresponds to how the
                            instance was allocated in _Create.
                   Return value - PR_SUCCESS if successful, etc.

    - SSMStatus YourClassName_GetAttrIDs(SSMResource *res,
                                        SSMAttributeID **ids,
                                        PRIntn *count)
      Requirement: MANDATORY
      Description: This is called when the client or part of the NSM server
                   wants to know all the available attributes of a particular
                   object.
      Parameters:  res - An instance of your class.
                   ids - Returns an array (allocated with PR_CALLOC) of
                         the available IDs.
                   count - Returns the number of IDs in (ids).
                   Return value - PR_SUCCESS if successful
                                  (Other underlying errors as appropriate)

    - SSMStatus YourClassName_GetAttr(SSMResource *res, 
                                     SSMAttributeID attrID, 
                                     SSMAttributeValue *value)
      Requirement: MANDATORY
      Description: This is called when a client requests data from an
                   instance of your class. The default method simply 
                   returns PR_FAILURE.
      Parameters:  res - An instance of your class.
                   attrID - The ID of the requested member data.
                   value - The attribute value struct to be filled in.
                   Return value - PR_SUCCESS if successful
                                  SSM_ERR_BAD_FID: Field ID is incorrect.
                                  (Other underlying errors as appropriate)

    - SSMStatus YourClassName_SetAttr(SSMResource *res,
                                     SSMAttributeID attrID,
                                     SSMAttributeValue *value)
      Requirement: Optional
      Description: This is called when a client wants to change a member
                   of an instance of your class. This only needs to be 
                   implemented if you want to accept values from the 
                   client (most classes will not want to do this). The 
                   default method simply returns PR_FAILURE.
      Parameters:  res - An instance of your class.
                   attrID - The ID of the member to be changed.
                   value - The value to set the attribute to.
                   Return value - PR_SUCCESS if successful
                                  SSM_ERR_BAD_FID: Field ID is incorrect.
                                  SSM_ERR_ATTRTYPE_MISMATCH: Type mismatch
                                  (Other underlying errors as appropriate)
      

    - void YourClassName_Invariant(YourClassName *obj)
      Requirement: Optional (but recommended)
      Description: Performs invariant checking on member data particular
                   to your class. Normally this is done by calling PR_ASSERT
                   on typical expected conditions in your member data.
                   Your invariant method should call the superclass' Invariant
                   method to ensure proper invariant checking of the whole
                   instance.
      Parameters:  obj - An instance of your class.
                   Return value - PR_SUCCESS if successful
                                  (Errors as appropriate)

  * Register your class by making a call to SSM_RegisterResourceType 
  from inside SSM_ResourceInit in resource.c, passing whatever 
  methods your class overrides. Note that in some subclasses (such as 
  SSMConnection), additional virtual functions are provided for other 
  polymorphic functionality. You'll need to override those methods 
  inside your _Init routine directly.

 */

#ifndef __SSM_RESOURCE_H__
#define __SSM_RESOURCE_H__

#include "ssmdefs.h"
#include "ssmerrs.h"
#include "rsrcids.h"
#include "hashtbl.h"
#include "protocol.h"
#include "protocolf.h"
#include "prtypes.h"
#include "nspr.h"
#include "serv.h"
#include "collectn.h"


/* typedef struct SSMResource SSMResource; */
/* SSMControlConnection is defined in ctrlcon.h */
/* typedef struct SSMControlConnection SSMControlConnection; */
typedef struct HTTPRequest HTTPRequest;

typedef SSMStatus (*SSMResourceCreateFunc) (void *arg, 
                                           SSMControlConnection * conn, 
                                           SSMResource **res);

typedef SSMStatus (*SSMResourceDestroyFunc)(SSMResource *res, PRBool doFree);

typedef SSMStatus (*SSMResourceGetAttrIDsFunc)(SSMResource *res,
                                              SSMAttributeID **ids,
                                              PRIntn *count);

typedef SSMStatus (*SSMResourceGetAttrFunc)(SSMResource *res,
                                           SSMAttributeID attrID,
                                           SSMResourceAttrType attrType, 
                                           SSMAttributeValue *value);

typedef SSMStatus (*SSMResourceSetAttrFunc)(SSMResource *res,
                                           SSMAttributeID attrID,
                                           SSMAttributeValue *value);

typedef SSMStatus (*SSMResourceShutdownFunc)(SSMResource *res,
                                            SSMStatus status);

typedef SSMStatus (*SSMResourcePickleFunc)(SSMResource *res, 
                                          PRIntn * len,
                                          void **value);

typedef SSMStatus (*SSMResourceUnpickleFunc)(SSMResource ** res,
                                            SSMControlConnection * conn,
                                            PRIntn len, void * value);

typedef SSMStatus (*SSMResourceHTMLFunc)(SSMResource *res,
                                        PRIntn * len, 
                                        void ** value);

/* be sure to include <nlsutil.h> before your implementations */
typedef SSMStatus (*SSMResourcePrintFunc)(SSMResource *res,
                                         char *fmt,
                                         PRIntn numParams,
					 char ** value,
                                         char **resultStr);

typedef SSMStatus (*SSMSubmitHandlerFunc)(struct SSMResource *res,
                                          HTTPRequest *req);

/* What do we do with a resource when the client wants to destroy it? */
typedef enum
{
    SSM_CLIENTDEST_NOTHING = (PRUint32) 0, /* take no action */
    SSM_CLIENTDEST_FREE,                   /* free the resource */
    SSM_CLIENTDEST_SHUTDOWN                /* shut down threads
                                              before freeing object */
} SSMClientDestroyAction;

typedef enum
{
  SSM_BUTTON_NONE = 0,
  SSM_BUTTON_CANCEL,
  SSM_BUTTON_OK
} SSMUIButtonType;

typedef struct SSMResourceClass SSMResourceClass;

/* typedef struct SSMResource SSMResource; */

struct SSMResource
{
    SSMResourceClass *m_class; /* Pointer to class object */
    SSMResourceID          m_id;
    SSMResourceType        m_classType;
    SSMControlConnection*  m_connection; /* Control connection 
                                                   that owns the  resource */
    SSMStatus               m_status;    /* Status of last closing thread.
                                           Indicates whether to do an
                                           emergency shutdown (interrupt all
                                           threads, post shutdown msgs, etc.)
                                           or to allow continuing operation. */
    PRIntn                 m_threadCount; /* Number of service threads */
    PRMonitor *            m_lock;      /* Thread lock for this connection 
                                           object */
    PRMonitor *            m_UILock; /* Use this lock for waiting and 
                                      * notifying for UI events.
                                      */
    PRBool                 m_UIBoolean; /* This value is set by the Notify 
                                         * on UILock.
                                         */
    PRIntn                 m_refCount;       /* Reference count */
    PRIntn                 m_clientCount;  /* Number of client references */
    SSMUIButtonType        m_buttonType; /* This will be set when a UI event
                                          * button event happens and the 
                                          * target is the resource.
                                          */
    char *                 m_formName; /* The name of the form that created
                                        * the UI event causing the form submit
                                        * handler to get called.
                                        */
    char *                 m_fileName; /* If this object requested a file path,
                                        * then this field will be used to 
                                        * store the returned value.
                                        */
    void *                 m_uiData;   /* Using this value to pass misc UI 
					* information.
					*/

    CMTItem                m_clientContext; /* This is set by client and passed back
                                       * to client during UI operations */

    PRThread *             m_waitThread; /*Thread waiting for us to shut down*/
    SSMClientDestroyAction m_clientDest;/* what to do if client destroys us */

    SSMResourceDestroyFunc m_destroy_func;
    SSMResourceShutdownFunc m_shutdown_func;
    SSMResourceGetAttrIDsFunc m_getids_func;
    SSMResourceGetAttrFunc m_get_func;
    SSMResourceSetAttrFunc m_set_func;
    SSMResourcePickleFunc  m_pickle_func;
    SSMResourceHTMLFunc    m_html_func;
    SSMResourcePrintFunc   m_print_func;
    SSMSubmitHandlerFunc   m_submit_func;
    PRBool                 m_resourceShutdown;
};

/* Macros */
#define RESOURCE_CLASS(x) ( ((SSMResource *) (x))->m_classType )

/************************************************************
** FUNCTION: SSM_CreateResource
**
** DESCRIPTION: Create a new resource of type (type). If (rawData)
**              is not NULL, get initial values from that structure.
**              rawData is treated differently depending on the type
**              of resource being created; for example, a certificate
**              object will store a reference to the certificate 
**              structure in (rawData), whereas a connection resource
**              will copy values out of an SSMHashTable pointed to
**              by (rawData).
** INPUTS:
**   type
**     The type of resource to be created.
**   arg
**     A type-dependent initial value for attribute(s) of the newly
**     created resource.
**   resID
**     Returns the ID of the newly created resource.
**   result
**     Returns a pointer to the newly created resource.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
*************************************************************/
SSMStatus SSM_CreateResource(SSMResourceType type, 
                            void *arg,
                            SSMControlConnection * conn,
                            SSMResourceID *resID, 
                            SSMResource **result);
			  
			  

/************************************************************
** FUNCTION: SSM_GetResAttribute
**
** DESCRIPTION: Get an attribute from an SSMResource object.
**
** INPUTS:
**   res
**     The resource from which an attribute is to be retrieved.
**   attrID
**     The field/resource ID of the attribute.
**   attrType
**     The expected type of the retrieved attribute.
**   value
**     The attribute value to be filled in.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
** NOTES:
**   ### mwelch All this memory allocation can be problematic.
*************************************************************/
SSMStatus SSM_GetResAttribute(SSMResource *res, SSMAttributeID attrID,
                              SSMResourceAttrType attrType,
                              SSMAttributeValue *value);

/************************************************************
** FUNCTION: SSM_SetResAttribute
**
** DESCRIPTION: Set an attribute on an SSMResource object. If
**              the attribute does not yet exist, it is created.
**
** INPUTS:
**   res
**     The resource in which an attribute is to be changed.
**   attrID
**     The field/resource ID of the attribute.
**   value
**     A pointer to the value of the newly changed attribute. The value
**     is copied into the resource.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
*************************************************************/
SSMStatus SSM_SetResAttribute(SSMResource *res, SSMAttributeID attrID,
                             SSMAttributeValue *value);

/************************************************************
** FUNCTION: SSM_PickleResource
** 
** DESCRIPTION: Pickle resource. 
** 
** INPUTS: 
**   res
**     The resource to be pickled.
**
** OUTPUTS:
**   len
**     Length of the pickled blob.
**   value
**     Pickled resource.
**
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or NSPR error code.
*************************************************************/	   
SSMStatus
SSM_PickleResource(SSMResource * res, PRIntn * len, void ** value);


/************************************************************
** FUNCTION: SSM_FreeResource
**
** DESCRIPTION: Free a reference on (res). If the reference count
**              reaches 0, (res) is destroyed.
**
** INPUTS:
**   res
**     The resource to be dereferenced.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
*************************************************************/
SSMStatus SSM_FreeResource(SSMResource *res);


/************************************************************
** FUNCTION: SSM_GetResourceReference
**
** DESCRIPTION: Get a reference on (res). 
**
** INPUTS:
**   res
**     The resource to be referenced.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
*************************************************************/
SSMStatus SSM_GetResourceReference(SSMResource *res);

/************************************************************
** FUNCTION: SSM_RegisterResourceType
**
** DESCRIPTION: Register a resource type within the Cartman server.
**
** INPUTS:
**   type
**      The new type to be registered.
**   superClass
**      Inherit functions from the already-registered superclass.
**      if (superClass) is not SSM_RESTYPE_NULL and any function
**      parameters below are NULL, those functions are inherited
**      from the indicated superclass.
**  createFunc, rDestFunc, setFunc, getFunc
**      Accessor functions for the resource.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE, the underlying NSPR error,
**      or the following values:
**      SSM_ERR_INVALID_FUNC: (superClass) was SSM_RESTYPE_NULL
**                            and one or more of the function parameters
**                            was also NULL.
*************************************************************/

SSMStatus SSM_RegisterResourceType(char *                    className, 
                                  SSMResourceType           type,
                                  SSMResourceType           superClass,
                                  SSMClientDestroyAction    cDestAction,
                                  SSMResourceCreateFunc     createFunc,
                                  SSMResourceDestroyFunc    destFunc,
                                  SSMResourceShutdownFunc   shutdownFunc,
                                  SSMResourceGetAttrIDsFunc getIDsFunc,
                                  SSMResourceGetAttrFunc    getFunc,
                                  SSMResourceSetAttrFunc    setFunc,
                                  SSMResourcePickleFunc     pickleFunc, 
                                  SSMResourceUnpickleFunc   unpickleFunc,
                                  SSMResourceHTMLFunc       htmlFunc,
                                  SSMResourcePrintFunc      printFunc,
                                  SSMSubmitHandlerFunc      submitFunc);

PRBool SSM_IsA(SSMResource *res, SSMResourceType type);
PRBool SSM_IsAKindOf(SSMResource *res, SSMResourceType type);

SSMStatus SSM_ClientGetResourceReference(SSMResource *res, SSMResourceID *id);
SSMStatus SSM_ShutdownResource(SSMResource *res, SSMStatus status);
SSMStatus SSM_ClientDestroyResource(SSMControlConnection *connection,
                                   SSMResourceID rid, SSMResourceType type);
void SSM_LockResource(SSMResource *res);
SSMStatus SSM_UnlockResource(SSMResource *res);
SSMStatus SSM_WaitResource(SSMResource *res, PRIntervalTime ticks);
SSMStatus SSM_WaitForOKCancelEvent(SSMResource *res, PRIntervalTime ticks);
SSMStatus SSM_NotifyOKCancelEvent(SSMResource *res);
SSMStatus SSM_NotifyResource(SSMResource *res);

void SSM_LockUIEvent(SSMResource *res);
SSMStatus SSM_UnlockUIEvent(SSMResource *res);
SSMStatus SSM_WaitUIEvent(SSMResource *res, PRIntervalTime ticks);
SSMStatus SSM_NotifyUIEvent(SSMResource *);

SSMStatus SSM_PickleResource(SSMResource * res, PRIntn * len, void ** value);
SSMStatus SSM_UnpickleResource(SSMResource ** res, SSMResourceType type, 
                              SSMControlConnection * connection,
                              PRIntn len, void * value);

SSMStatus SSM_MessageFormatResource(SSMResource *res, char *fmt, PRIntn numParam,
				   char ** value,
                                   char **resultStr);

SSMStatus SSMResource_Create(void *arg, SSMControlConnection * conn, 
                            SSMResource **res);
SSMStatus SSMResource_Destroy(SSMResource *res, PRBool doFree);
SSMStatus SSMResource_Shutdown(SSMResource *res, SSMStatus status);
SSMStatus SSMResource_Init(SSMControlConnection * conn, SSMResource *res, 
			  SSMResourceType type);
void SSMResource_Invariant(SSMResource *res);

SSMStatus SSMResource_GetAttr(SSMResource *res,
                              SSMAttributeID attrID,
                              SSMResourceAttrType attrType,
                              SSMAttributeValue *value);

SSMStatus SSMResource_SetAttr(SSMResource *res,
                             SSMAttributeID attrID,
                             SSMAttributeValue *value);

SSMStatus SSMResource_GetAttrIDs(SSMResource *res,
                                SSMAttributeID **ids,
                                PRIntn *count);

SSMStatus SSMResource_Pickle(SSMResource *res,
                            PRIntn * len,
                            void **value);

SSMStatus SSMResource_Unpickle(SSMResource ** res,
                              SSMControlConnection *conn,
                              PRIntn len,
                              void * value);

SSMStatus SSMResource_HTML(SSMResource *res,
                          PRIntn * len,
                          void ** value);

SSMStatus SSMResource_Print(SSMResource *res,
                           char *fmt,
                           PRIntn numParam,
                           char **value,
                           char **resultStr);

SSMStatus SSMResource_FormSubmitHandler(SSMResource *res,
                                        HTTPRequest *req);

/* ALWAYS pass in msg a string allocated by NSPR (PR_smprintf for example),
   because the string is freed from within SSM_Debug. */
void SSM_Debug(SSMResource *res, char *msg);

char * SSM_ResourceClassName(SSMResource *res);

/* Wait for a resource to shut down. */
SSMStatus SSM_WaitForResourceShutdown(SSMResource *res);

/* Register a thread with a resource (or pass arg==NULL for standalone) */
void SSM_RegisterThread(char *threadName, SSMResource *arg);
void SSM_RegisterNewThread(char *threadName, SSMResource *ptr);

SSMStatus
SSM_ShutdownResource(SSMResource *res, SSMStatus status);

SSMStatus SSM_HandlePromptReply(SSMResource *res, char *reply);

/* Initialize resources. Only called by main. */
SSMStatus SSM_ResourceInit();

/* Create a resource thread */
PRThread *
SSM_CreateThread(SSMResource *res, void (*func)(void *arg));

#endif

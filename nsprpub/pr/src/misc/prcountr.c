/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** prcountr.c -- NSPR Instrumentation Counters
**
** Implement the interface defined in prcountr.h
**
** Design Notes:
**
** The Counter Facility (CF) has a single anchor: qNameList.      
** The anchor is a PRCList. qNameList is a list of links in QName
** structures. From qNameList any QName structure and its
** associated RName structure can be located. 
** 
** For each QName, a list of RName structures is anchored at
** rnLink in the QName structure.
** 
** The counter itself is embedded in the RName structure.
** 
** For manipulating the counter database, single lock is used to
** protect the entire list: counterLock.
**
** A PRCounterHandle, defined in prcountr.h, is really a pointer
** to a RName structure. References by PRCounterHandle are
** dead-reconed to the RName structure. The PRCounterHandle is
** "overloaded" for traversing the QName structures; only the
** function PR_FindNextQnameHandle() uses this overloading.
**
** 
** ToDo (lth): decide on how to lock or atomically update
** individual counters. Candidates are: the global lock; a lock
** per RName structure; Atomic operations (Note that there are
** not adaquate atomic operations (yet) to achieve this goal). At
** this writing (6/19/98) , the update of the counter variable in
** a QName structure is unprotected.
**
*/

#include "prcountr.h"
#include "prclist.h"
#include "prlock.h"
#include "prlog.h"
#include "prmem.h"
#include <string.h>

/*
**
*/
typedef struct QName
{
    PRCList link;
    PRCList rNameList;
    char    name[PRCOUNTER_NAME_MAX+1];
} QName;

/*
**
*/
typedef struct RName
{
    PRCList link;
    QName   *qName;
    PRLock  *lock;
    volatile PRUint32   counter;    
    char    name[PRCOUNTER_NAME_MAX+1]; 
    char    desc[PRCOUNTER_DESC_MAX+1]; 
} RName;


/*
** Define the Counter Facility database
*/
static PRLock  *counterLock;
static PRCList qNameList;
static PRLogModuleInfo *lm;

/*
** _PR_CounterInitialize() -- Initialize the Counter Facility
**
*/
static void _PR_CounterInitialize( void )
{
    /*
    ** This function should be called only once
    */
    PR_ASSERT( counterLock == NULL );
    
    counterLock = PR_NewLock();
    PR_INIT_CLIST( &qNameList );
    lm = PR_NewLogModule("counters");
    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: Initialization complete"));

    return;
} /* end _PR_CounterInitialize() */

/*
** PR_CreateCounter() -- Create a counter
**
**  ValidateArguments
**  Lock
**  if (qName not already in database)
**      NewQname
**  if (rName already in database )
**      Assert
**  else NewRname
**  NewCounter
**  link 'em up
**  Unlock
**
*/
PR_IMPLEMENT(PRCounterHandle) 
	PR_CreateCounter( 
		const char *qName, 
    	const char *rName, 
        const char *description 
) 
{
    QName   *qnp;
    RName   *rnp;
    PRBool  matchQname = PR_FALSE;

    /* Self initialize, if necessary */
    if ( counterLock == NULL )
        _PR_CounterInitialize();

    /* Validate input arguments */
    PR_ASSERT( strlen(qName) <= PRCOUNTER_NAME_MAX );
    PR_ASSERT( strlen(rName) <= PRCOUNTER_NAME_MAX );
    PR_ASSERT( strlen(description) <= PRCOUNTER_DESC_MAX );

    /* Lock the Facility */
    PR_Lock( counterLock );

    /* Do we already have a matching QName? */
    if (!PR_CLIST_IS_EMPTY( &qNameList ))
    {
        qnp = (QName *) PR_LIST_HEAD( &qNameList );
        do {
            if ( strcmp(qnp->name, qName) == 0)
            {
                matchQname = PR_TRUE;
                break;
            }
            qnp = (QName *)PR_NEXT_LINK( &qnp->link );
        } while( qnp != (QName *)&qNameList );
    }
    /*
    ** If we did not find a matching QName,
    **    allocate one and initialize it.
    **    link it onto the qNameList.
    **
    */
    if ( matchQname != PR_TRUE )
    {
        qnp = PR_NEWZAP( QName );
        PR_ASSERT( qnp != NULL );
        PR_INIT_CLIST( &qnp->link ); 
        PR_INIT_CLIST( &qnp->rNameList ); 
        strcpy( qnp->name, qName );
        PR_APPEND_LINK( &qnp->link, &qNameList ); 
    }

    /* Do we already have a matching RName? */
    if (!PR_CLIST_IS_EMPTY( &qnp->rNameList ))
    {
        rnp = (RName *) PR_LIST_HEAD( &qnp->rNameList );
        do {
            /*
            ** No duplicate RNames are allowed within a QName
            **
            */
            PR_ASSERT( strcmp(rnp->name, rName));
            rnp = (RName *)PR_NEXT_LINK( &rnp->link );
        } while( rnp != (RName *)&qnp->rNameList );
    }

    /* Get a new RName structure; initialize its members */
    rnp = PR_NEWZAP( RName );
    PR_ASSERT( rnp != NULL );
    PR_INIT_CLIST( &rnp->link );
    strcpy( rnp->name, rName );
    strcpy( rnp->desc, description );
    rnp->lock = PR_NewLock();
    if ( rnp->lock == NULL )
    {
        PR_ASSERT(0);
    }

    PR_APPEND_LINK( &rnp->link, &qnp->rNameList ); /* add RName to QName's rnList */    
    rnp->qName = qnp;                       /* point the RName to the QName */

    /* Unlock the Facility */
    PR_Unlock( counterLock );
    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: Create: QName: %s %p, RName: %s %p\n\t",
        qName, qnp, rName, rnp ));

    return((PRCounterHandle)rnp);
} /*  end PR_CreateCounter() */
  

/*
**
*/
PR_IMPLEMENT(void) 
	PR_DestroyCounter( 
		PRCounterHandle handle 
)
{
    RName   *rnp = (RName *)handle;
    QName   *qnp = rnp->qName;

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: Deleting: QName: %s, RName: %s", 
        qnp->name, rnp->name));

    /* Lock the Facility */
    PR_Lock( counterLock );

    /*
    ** Remove RName from the list of RNames in QName
    ** and free RName
    */
    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: Deleting RName: %s, %p", 
        rnp->name, rnp));
    PR_REMOVE_LINK( &rnp->link );
    PR_Free( rnp->lock );
    PR_DELETE( rnp );

    /*
    ** If this is the last RName within QName
    **   remove QName from the qNameList and free it
    */
    if ( PR_CLIST_IS_EMPTY( &qnp->rNameList ) )
    {
        PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: Deleting unused QName: %s, %p", 
            qnp->name, qnp));
        PR_REMOVE_LINK( &qnp->link );
        PR_DELETE( qnp );
    } 

    /* Unlock the Facility */
    PR_Unlock( counterLock );
    return;
} /*  end PR_DestroyCounter() */

/*
**
*/
PR_IMPLEMENT(PRCounterHandle) 
	PR_GetCounterHandleFromName( 
    	const char *qName, 
    	const char *rName 
)
{
    const char    *qn, *rn, *desc;
    PRCounterHandle     qh, rh = NULL;
    RName   *rnp = NULL;

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: GetCounterHandleFromName:\n\t"
        "QName: %s, RName: %s", qName, rName ));

    qh = PR_FindNextCounterQname( NULL );
    while (qh != NULL)
    {
        rh = PR_FindNextCounterRname( NULL, qh );
        while ( rh != NULL )
        {
            PR_GetCounterNameFromHandle( rh, &qn, &rn, &desc );
            if ( (strcmp( qName, qn ) == 0)
                && (strcmp( rName, rn ) == 0 ))
            {
                rnp = (RName *)rh;
                goto foundIt;
            }
            rh = PR_FindNextCounterRname( rh, qh );
        }
        qh = PR_FindNextCounterQname( NULL );
    }

foundIt:
    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: GetConterHandleFromName: %p", rnp ));
    return(rh);
} /*  end PR_GetCounterHandleFromName() */

/*
**
*/
PR_IMPLEMENT(void) 
	PR_GetCounterNameFromHandle( 
    	PRCounterHandle handle,  
	    const char **qName, 
	    const char **rName, 
		const char **description 
)
{
    RName   *rnp = (RName *)handle;
    QName   *qnp = rnp->qName;

    *qName = qnp->name;
    *rName = rnp->name;
    *description = rnp->desc;

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: GetConterNameFromHandle: "
        "QNp: %p, RNp: %p,\n\tQName: %s, RName: %s, Desc: %s", 
        qnp, rnp, qnp->name, rnp->name, rnp->desc ));

    return;
} /*  end PR_GetCounterNameFromHandle() */


/*
**
*/
PR_IMPLEMENT(void) 
	PR_IncrementCounter( 
		PRCounterHandle handle
)
{
    PR_Lock(((RName *)handle)->lock);
    ((RName *)handle)->counter++;
    PR_Unlock(((RName *)handle)->lock);

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: Increment: %p, %ld", 
        handle, ((RName *)handle)->counter ));

    return;
} /*  end PR_IncrementCounter() */



/*
**
*/
PR_IMPLEMENT(void) 
	PR_DecrementCounter( 
		PRCounterHandle handle
)
{
    PR_Lock(((RName *)handle)->lock);
    ((RName *)handle)->counter--;
    PR_Unlock(((RName *)handle)->lock);

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: Decrement: %p, %ld", 
        handle, ((RName *)handle)->counter ));

    return;
} /*  end PR_DecrementCounter()  */


/*
**
*/
PR_IMPLEMENT(void) 
	PR_AddToCounter( 
    	PRCounterHandle handle, 
	    PRUint32 value 
)
{
    PR_Lock(((RName *)handle)->lock);
    ((RName *)handle)->counter += value;
    PR_Unlock(((RName *)handle)->lock);

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: AddToCounter: %p, %ld", 
        handle, ((RName *)handle)->counter ));

    return;
} /*  end PR_AddToCounter() */


/*
**
*/
PR_IMPLEMENT(void) 
	PR_SubtractFromCounter( 
    	PRCounterHandle handle, 
	    PRUint32 value 
)
{
    PR_Lock(((RName *)handle)->lock);
    ((RName *)handle)->counter -= value;
    PR_Unlock(((RName *)handle)->lock);
    
    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: SubtractFromCounter: %p, %ld", 
        handle, ((RName *)handle)->counter ));

    return;
} /*  end  PR_SubtractFromCounter() */

/*
**
*/
PR_IMPLEMENT(PRUint32) 
	PR_GetCounter( 
		PRCounterHandle handle 
)
{
    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: GetCounter: %p, %ld", 
        handle, ((RName *)handle)->counter ));

    return(((RName *)handle)->counter);
} /*  end  PR_GetCounter() */

/*
**
*/
PR_IMPLEMENT(void) 
	PR_SetCounter( 
		PRCounterHandle handle, 
		PRUint32 value 
)
{
    ((RName *)handle)->counter = value;

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: SetCounter: %p, %ld", 
        handle, ((RName *)handle)->counter ));

    return;
} /*  end  PR_SetCounter() */

/*
**
*/
PR_IMPLEMENT(PRCounterHandle) 
	PR_FindNextCounterQname( 
        PRCounterHandle handle
)
{
    QName *qnp = (QName *)handle;

    if ( PR_CLIST_IS_EMPTY( &qNameList ))
            qnp = NULL;
    else if ( qnp == NULL )
        qnp = (QName *)PR_LIST_HEAD( &qNameList );
    else if ( PR_NEXT_LINK( &qnp->link ) ==  &qNameList )
        qnp = NULL;
    else  
        qnp = (QName *)PR_NEXT_LINK( &qnp->link );

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: FindNextQname: Handle: %p, Returns: %p", 
        handle, qnp ));

    return((PRCounterHandle)qnp);
} /*  end  PR_FindNextCounterQname() */


/*
**
*/
PR_IMPLEMENT(PRCounterHandle) 
	PR_FindNextCounterRname( 
        PRCounterHandle rhandle, 
        PRCounterHandle qhandle 
)
{
    RName *rnp = (RName *)rhandle;
    QName *qnp = (QName *)qhandle;


    if ( PR_CLIST_IS_EMPTY( &qnp->rNameList ))
        rnp = NULL;
    else if ( rnp == NULL )
        rnp = (RName *)PR_LIST_HEAD( &qnp->rNameList );
    else if ( PR_NEXT_LINK( &rnp->link ) ==  &qnp->rNameList )
        rnp = NULL;
    else
        rnp = (RName *)PR_NEXT_LINK( &rnp->link );

    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: FindNextRname: Rhandle: %p, QHandle: %p, Returns: %p", 
        rhandle, qhandle, rnp ));

    return((PRCounterHandle)rnp);
} /*  end PR_FindNextCounterRname() */

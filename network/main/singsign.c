/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#if defined(SingleSignon)

#include "libi18n.h"            /* For the character code set conversions */
#include "secnav.h"             /* For SECNAV_UnMungeString and SECNAV_MungeString */

#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#include "prinrval.h"           /* for PR_IntervalNow */
#else
#include "private/prpriv.h"     /* for NewNamedMonitor */
#endif

#ifdef PROFILE
#pragma profile on
#endif

#include "prefapi.h"
#include "xpgetstr.h"
#include "xpgetstr.h"

extern int MK_SIGNON_PASSWORDS_GENERATE;
extern int MK_SIGNON_PASSWORDS_REMEMBER;
extern int MK_SIGNON_PASSWORDS_FETCH;
extern int MK_SIGNON_YOUR_SIGNONS;

/* locks for signon cache */

static PRMonitor * signon_lock_monitor = NULL;
static PRThread  * signon_lock_owner = NULL;
static int signon_lock_count = 0;

PRIVATE void
si_lock_signon_list(void)
{
    if(!signon_lock_monitor)
	signon_lock_monitor =
            PR_NewNamedMonitor("signon-lock");

    PR_EnterMonitor(signon_lock_monitor);

    while(TRUE) {

	/* no current owner or owned by this thread */
	PRThread * t = PR_CurrentThread();
	if(signon_lock_owner == NULL || signon_lock_owner == t) {
	    signon_lock_owner = t;
	    signon_lock_count++;

	    PR_ExitMonitor(signon_lock_monitor);
	    return;
	}

	/* owned by someone else -- wait till we can get it */
	PR_Wait(signon_lock_monitor, PR_INTERVAL_NO_TIMEOUT);

    }
}

PRIVATE void
si_unlock_signon_list(void)
{
   PR_EnterMonitor(signon_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    PR_ASSERT(signon_lock_owner == PR_CurrentThread());
#endif

    signon_lock_count--;

    if(signon_lock_count == 0) {
	signon_lock_owner = NULL;
	PR_Notify(signon_lock_monitor);
    }
    PR_ExitMonitor(signon_lock_monitor);

}


/*
 * Data and procedures for rememberSignons preference
 */

static const char *pref_rememberSignons =
    "network.signon.rememberSignons";
PRIVATE Bool si_RememberSignons = FALSE;

PRIVATE int
si_SaveSignonDataLocked(char * filename);

PRIVATE void
si_SetSignonRememberingPref(Bool x)
{
	/* do nothing if new value of pref is same as current value */
	if (x == si_RememberSignons) {
	    return;
	}

	/* if pref is being turned off, save the current signons to a file */
	if (x == 0) {
	    si_lock_signon_list();
	    si_SaveSignonDataLocked(NULL);
	    si_unlock_signon_list();
	}

	/* change the pref */
	si_RememberSignons = x;

	/* if pref is being turned on, load the signon file into memory */
	if (x == 1) {
	    SI_RemoveAllSignonData();
	    SI_LoadSignonData(NULL);
	}
}

MODULE_PRIVATE int PR_CALLBACK
si_SignonRememberingPrefChanged(const char * newpref, void * data)
{
	Bool x;
	PREF_GetBoolPref(pref_rememberSignons, &x);
	si_SetSignonRememberingPref(x);
	return PREF_NOERROR;
}

void
si_RegisterSignonPrefCallbacks(void)
{
    Bool x;
    static Bool first_time = TRUE;

    if(first_time)
    {
	first_time = FALSE;
	PREF_GetBoolPref(pref_rememberSignons, &x);
	si_SetSignonRememberingPref(x);
	PREF_RegisterCallback(pref_rememberSignons, si_SignonRememberingPrefChanged, NULL);
    }
}

PRIVATE Bool
si_GetSignonRememberingPref(void)
{
	si_RegisterSignonPrefCallbacks();
	return si_RememberSignons;
}

/*
 * Data and procedures for signon cache
 */

typedef struct _SignonDataStruct {
    char * name;
    char * value;
    Bool isPassword;
} si_SignonDataStruct;

typedef struct _SignonUserStruct {
    XP_List * signonData_list;
} si_SignonUserStruct;

typedef struct _SignonURLStruct {
    char * URLName;
    si_SignonUserStruct* chosen_user; /* this is a state variable */
    Bool firsttime_chosing_user; /* this too is a state variable */
    XP_List * signonUser_list;
} si_SignonURLStruct;

PRIVATE XP_List * si_signon_list=0;
PRIVATE Bool si_signon_list_changed = FALSE;
PRIVATE int si_TagCount = 0;
PRIVATE si_SignonURLStruct * lastURL = NULL;


/* Remove misleading portions from URL name */
PRIVATE char*
si_StrippedURL (char* URLName) {
    char *result = 0;
    char *s, *t;

    /* check for null URLName */
    if (URLName == NULL || XP_STRLEN(URLName) == 0) {
	return NULL;
    }

    /* check for protocol at head of URL */
    s = URLName;
    s = (char*) XP_STRCHR(s+1, '/');
    if (s) {
        if (*(s+1) == '/') {
            /* looks good, let's parse it */
	    return NET_ParseURL(URLName, GET_HOST_PART);
	} else {
	    /* not a valid URL, leave it as is */
	    StrAllocCopy(result, URLName);
	    return result;
	}
    }

    /* initialize result */
    StrAllocCopy(result, URLName);

    /* remove everything in result after and including first question mark */
    s = result;
    s = (char*) XP_STRCHR(s, '?');
    if (s) {
        *s = '\0';
    }

    /* remove everything in result after last slash (e.g., index.html) */
    s = result;
    t = 0;
    while ((s = (char*) XP_STRCHR(s+1, '/'))) {
	t = s;
    }
    if (t) {
        *(t+1) = '\0';
    }

    /* remove socket number from result */
    s = result;
    while ((s = (char*) XP_STRCHR(s+1, ':'))) {
	/* s is at next colon */
        if (*(s+1) != '/') {
            /* and it's not :// so it must be start of socket number */
            if ((t = (char*) XP_STRCHR(s+1, '/'))) {
		/* t is at slash terminating the socket number */
		do {
		    /* copy remainder of t to s */
		    *s++ = *t;
		} while (*(t++));
	    }
	    break;
	}
    }

    /* alll done */
    return result;
}

/* remove terminating CRs or LFs */
PRIVATE void
si_StripLF(char* buffer) {
    while ((buffer[XP_STRLEN(buffer)-1] == '\n') ||
            (buffer[XP_STRLEN(buffer)-1] == '\r')) {
        buffer[XP_STRLEN(buffer)-1] = '\0';
    }
}

/* If user-entered password is "generate", then generate a random password */
PRIVATE void
si_Randomize(char * password) {
    PRIntervalTime random;
    int i;
    const char * hexDigits = "0123456789AbCdEf";
    if (XP_STRCMP(password, XP_GetString(MK_SIGNON_PASSWORDS_GENERATE)) == 0) {
	random = PR_IntervalNow();
	for (i=0; i<8; i++) {
	    password[i] = hexDigits[random%16];
	    random = random/16;
	}
    }
}

/*
 * Get the URL node for a given URL name
 *
 * This routine is called only when holding the signon lock!!!
 */
PRIVATE si_SignonURLStruct *
si_GetURL(char * URLName) {
    XP_List * list_ptr = si_signon_list;
    si_SignonURLStruct * URL;
    char *strippedURLName = 0;

    if (!URLName) {
	/* no URLName specified, return first URL (returns NULL if not URLs) */
	return (si_SignonURLStruct *) XP_ListNextObject(list_ptr);
    }

    strippedURLName = si_StrippedURL(URLName);
    while((URL = (si_SignonURLStruct *) XP_ListNextObject(list_ptr))!=0) {
	if(URL->URLName && !XP_STRCMP(strippedURLName, URL->URLName)) {
	    XP_FREE(strippedURLName);
	    return URL;
	}
    }
    XP_FREE(strippedURLName);
    return (NULL);
}

/* Remove a user node from a given URL node */
PUBLIC Bool
SI_RemoveUser(char *URLName, char *userName, Bool save) {
    si_SignonURLStruct * URL;
    si_SignonUserStruct * user;
    si_SignonDataStruct * data;
    XP_List * user_ptr;
    XP_List * data_ptr;

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()) {
	return FALSE;
    }

    si_lock_signon_list();

    /* get URL corresponding to URLName (or first URL if URLName is NULL) */
    URL = si_GetURL(URLName);
    if (!URL) {
	/* URL not found */
	si_unlock_signon_list();
	return FALSE;
    }

    /* free the data in each node of the specified user node for this URL */
    user_ptr = URL->signonUser_list;

    if (userName == NULL) {

	/* no user specified so remove the first one */
	user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr);

    } else {

	/* find the specified user */
	while((user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr))!=0) {
	    data_ptr = user->signonData_list;
	    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
		if (XP_STRCMP(data->value, userName)==0) {
		    goto foundUser;
		}
	    }
	}
	si_unlock_signon_list();
	return FALSE; /* user not found so nothing to remove */
	foundUser: ;
    }

    /* free the items in the data list */
    data_ptr = user->signonData_list;
    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
	XP_FREE(data->name);
	XP_FREE(data->value);
    }

    /* free the data list */
    XP_ListDestroy(user->signonData_list);

    /* free the user node */
    XP_ListRemoveObject(URL->signonUser_list, user);

    /* remove this URL if it contains no more users */
    if (XP_ListCount(URL->signonUser_list) == 0) {
	XP_FREE(URL->URLName);
	XP_ListRemoveObject(si_signon_list, URL);
    }

    /* write out the change to disk */
    if (save) {
	si_signon_list_changed = TRUE;
	si_SaveSignonDataLocked(NULL);
    }

    si_unlock_signon_list();
    return TRUE;
}

/*
 * temporary UI until FE implements this function as a single dialog box
 */
XP_Bool FE_Select(
	MWContext* pContext,
	char* pMessage,
	char** pList,
	int* pCount) {
    int i;
    char *message = 0;
    for (i = 0; i < *pCount; i++) {
	StrAllocCopy(message, pMessage);
        StrAllocCat(message, " = ");
	StrAllocCat(message, pList[i]);
	if (FE_Confirm(pContext, message)) {
	    /* user selected this one */
	    XP_FREE(message);
	    *pCount = i;
	    return TRUE;
	}
    }

    /* user rejected all */
    XP_FREE(message);
    return FALSE;
}

/*
 * Get the user node for a given URL
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE si_SignonUserStruct*
si_GetUser(MWContext *context, char* URLName, Bool pickFirstUser) {
    si_SignonURLStruct* URL;
    si_SignonUserStruct* user;
    si_SignonDataStruct* data;
    XP_List * data_ptr=0;
    XP_List * user_ptr=0;
    XP_List * old_user_ptr=0;

    /* get to node for this URL */
    URL = si_GetURL(URLName);
    if (URL != NULL) {

	/* node for this URL was found */
	int user_count;
	user_ptr = URL->signonUser_list;
	if ((user_count = XP_ListCount(user_ptr)) == 1) {

	    /* only one set of data exists for this URL so select it */
	    user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr);
	    URL->firsttime_chosing_user = FALSE;
	    URL->chosen_user = user;

	} else if (pickFirstUser) {
	    user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr);

	} else if (URL->firsttime_chosing_user) {
	    /* multiple users for this URL so a choice needs to be made */
	    char ** list;
	    char ** list2;
	    si_SignonUserStruct** users;
	    si_SignonUserStruct** users2;
	    char * caption = 0;

	    list = XP_ALLOC(user_count*sizeof(char*));
	    users = XP_ALLOC(user_count*sizeof(si_SignonUserStruct*));
	    list2 = list;
	    users2 = users;

	    /* step through set of user nodes for this URL and create list of
	     * first data node of each (presumably that is the user name).
	     * Note that the user nodes are already ordered by
	     * most-recently-used so the first one in the list is the most
	     * likely one to be chosen.
	     */
	    URL->firsttime_chosing_user = FALSE;
	    old_user_ptr = user_ptr;

	    while((user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr))!=0) {
		data_ptr = user->signonData_list;
		/* consider first item in data list to be the identifying item */
		data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
		if (!caption) {
		    caption = data->name;
		}
		*(list2++) = data->value;
		*(users2++) = user;
	    }

	    /* have user select an item from the list */
	    if (FE_Select(context, caption, list, &user_count)) {
		/* user selected an item */
		user = users[user_count]; /* this is the selected item */
		URL->chosen_user = user;
		/* item selected is now most-recently used, put at head of list */
		XP_ListRemoveObject(URL->signonUser_list, user);
		XP_ListAddObject(URL->signonUser_list, user);
	    } else {
		user = NULL;
	    }
	    XP_FREE(list);
	    XP_FREE(users);

	} else {
	    user = URL->chosen_user;
	}
    } else {
	user = NULL;
    }
    return user;
}

/*
 * Get the user for which a change-of-password is to be applied
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE si_SignonUserStruct*
si_GetUserForChangeForm(MWContext *context, char* URLName, int messageNumber)
{
    si_SignonURLStruct* URL;
    si_SignonUserStruct* user;
    si_SignonDataStruct * data;
    XP_List * user_ptr = 0;
    XP_List * data_ptr = 0;
    char *message = 0;

    /* get to node for this URL */
    URL = si_GetURL(URLName);
    if (URL != NULL) {

	/* node for this URL was found */
	user_ptr = URL->signonUser_list;
	while((user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr))!=0) {
	    char *strippedURLName = si_StrippedURL(URLName);
	    data_ptr = user->signonData_list;
	    data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
	    message = PR_smprintf(XP_GetString(messageNumber),
				  data->value,
				  strippedURLName);
	    XP_FREE(strippedURLName);
	    if (FE_Confirm(context, message)) {
		/*
		 * since this user node is now the most-recently-used one, move it
		 * to the head of the user list so that it can be favored for
		 * re-use the next time this form is encountered
		 */
		XP_ListRemoveObject(URL->signonUser_list, user);
		XP_ListAddObject(URL->signonUser_list, user);
		si_signon_list_changed = TRUE;
		si_SaveSignonDataLocked(NULL);
		XP_FREE(message);
		return user;
	    }
	}
    }
    XP_FREE(message);
    return NULL;
}

/*
 * Put data obtained from a submit form into the data structure for
 * the specified URL
 *
 * See comments below about state of signon lock when routine is called!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */
PRIVATE void
si_PutData(char * URLName, LO_FormSubmitData * submit, Bool save) {
    char * name;
    char * value;
    Bool added_to_list = FALSE;
    si_SignonURLStruct * URL;
    si_SignonUserStruct * user;
    si_SignonDataStruct * data;
    int j;
    XP_List * list_ptr;
    XP_List * user_ptr;
    XP_List * data_ptr;
    si_SignonURLStruct * tmp_URL_ptr;
    size_t new_len;
    Bool mismatch;
    int i;

    /* discard this if the password is empty */
    for (i=0; i<submit->value_cnt; i++) {
	if ((((uint8*)submit->type_array)[i] == FORM_TYPE_PASSWORD) &&
		(XP_STRLEN( ((char **)(submit->value_array)) [i])) == 0) {
	    return;
	}
    }

    /*
     * lock the signon list
     *	 Note that, for efficiency, SI_LoadSignonData already sets the lock
     *	 before calling this routine whereas none of the other callers do.
     *	 So we need to determine whether or not we were called from
     *	 SI_LoadSignonData before setting or clearing the lock.  We can
     *   determine this by testing "save" since only SI_LoadSignonData passes
     *   in a value of FALSE for "save".
     */
    if (save) {
	si_lock_signon_list();
    }

    /* make sure the signon list exists */
    if(!si_signon_list) {
	si_signon_list = XP_ListNew();
	if(!si_signon_list) {
	    if (save) {
		si_unlock_signon_list();
	    }
	    return;
	}
    }

    /* find node in signon list having the same URL */
    if ((URL = si_GetURL(URLName)) == NULL) {

        /* doesn't exist so allocate new node to be put into signon list */
	URL = XP_NEW(si_SignonURLStruct);
	if (!URL) {
	    if (save) {
		si_unlock_signon_list();
	    }
	    return;
	}

	/* fill in fields of new node */
	URL->URLName = si_StrippedURL(URLName);
	if (!URL->URLName) {
	    XP_FREE(URL);
	    if (save) {
		si_unlock_signon_list();
	    }
	    return;
	}

	URL->signonUser_list = XP_ListNew();
	if(!URL->signonUser_list) {
	    XP_FREE(URL->URLName);
	    XP_FREE(URL);
	}

	/* put new node into signon list so that it is before any
	 * strings of smaller length
	 */
	new_len = XP_STRLEN(URL->URLName);
	list_ptr = si_signon_list;
	while((tmp_URL_ptr =
		(si_SignonURLStruct *) XP_ListNextObject(list_ptr))!=0) {
	    if(new_len > XP_STRLEN (tmp_URL_ptr->URLName)) {
		XP_ListInsertObject
		    (si_signon_list, tmp_URL_ptr, URL);
		added_to_list = TRUE;
		break;
	    }
	}
	/* no shorter strings found in list */
	if (!added_to_list) {
	    XP_ListAddObjectToEnd (si_signon_list, URL);
	}
    }

    /* initialize state variables in URL node */
    URL->chosen_user = NULL;
    URL->firsttime_chosing_user = TRUE;

    /*
     * see if a user node with data list matching new data already exists
     * (password fields will not be checked for in this matching)
     */
    user_ptr = URL->signonUser_list;
    while((user = (si_SignonUserStruct *) XP_ListNextObject(user_ptr))!=0) {
	data_ptr = user->signonData_list;
	j = 0;
	while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
	    mismatch = FALSE;

	    /* skip non text/password fields */
	    while ((j < submit->value_cnt) &&
		    (((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
		    (((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
		j++;
	    }

	    /* check for match on name field and type field */
	    if ((j < submit->value_cnt) &&
		    (data->isPassword ==
			(((uint8*)submit->type_array)[j]==FORM_TYPE_PASSWORD)) &&
		    (!XP_STRCMP(data->name, ((char **)submit->name_array)[j]))) {

		/* success, now check for match on value field if not password */
		if (!submit->value_array[j]) {
		    /* special case a null value */
                    if (!XP_STRCMP(data->value, "")) {
			j++; /* success */
		    } else {
			mismatch = TRUE;
			break; /* value mismatch, try next user */
		    }
		} else if (!XP_STRCMP(data->value, ((char **)submit->value_array)[j])
			|| data->isPassword) {
		    j++; /* success */
		} else {
		    mismatch = TRUE;
		    break; /* value mismatch, try next user */
		}
	    } else {
		mismatch = TRUE;
		break; /* name or type mismatch, try next user */
	    }

	}
	if (!mismatch) {

	    /* all names and types matched and all non-password values matched */

	    /*
	     * note: it is ok for password values not to match; it means
	     * that the user has either changed his password behind our
	     * back or that he previously mis-entered the password
	     */

	    /* update the saved password values */
	    data_ptr = user->signonData_list;
	    j = 0;
	    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {

		/* skip non text/password fields */
		while ((j < submit->value_cnt) &&
			(((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
			(((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
		    j++;
		}

		/* update saved password */
		if ((j < submit->value_cnt) && data->isPassword) {
		    if (XP_STRCMP(data->value, ((char **)submit->value_array)[j])) {
			si_signon_list_changed = TRUE;
			StrAllocCopy(data->value, ((char **)submit->value_array)[j]);
			si_Randomize(data->value);
		    }
		}
		j++;
	    }

	    /*
	     * since this user node is now the most-recently-used one, move it
	     * to the head of the user list so that it can be favored for
	     * re-use the next time this form is encountered
	     */
	    XP_ListRemoveObject(URL->signonUser_list, user);
	    XP_ListAddObject(URL->signonUser_list, user);

	    /* return */
	    if (save) {
		si_signon_list_changed = TRUE;
		si_SaveSignonDataLocked(NULL);
		si_unlock_signon_list();
	    }
	    return; /* nothing more to do since data already exists */
	}
    }

    /* user node with current data not found so create one */
    user = XP_NEW(si_SignonUserStruct);
    if (!user) {
	if (save) {
	    si_unlock_signon_list();
	}
	return;
    }
    user->signonData_list = XP_ListNew();
    if(!user->signonData_list) {
	XP_FREE(user);
	if (save) {
	    si_unlock_signon_list();
	}
	return;
    }


    /* create and fill in data nodes for new user node */
    for (j=0; j<submit->value_cnt; j++) {

	/* skip non text/password fields */
	if((((uint8*)submit->type_array)[j]!=FORM_TYPE_TEXT) &&
		(((uint8*)submit->type_array)[j]!=FORM_TYPE_PASSWORD)) {
	    continue;
	}

	/* create signon data node */
	data = XP_NEW(si_SignonDataStruct);
	if (!data) {
	    XP_ListDestroy(user->signonData_list);
	    XP_FREE(user);
	}
	data->isPassword =
	    (((uint8 *)submit->type_array)[j] == FORM_TYPE_PASSWORD);
        name = 0; /* so that StrAllocCopy doesn't free previous name */
	StrAllocCopy(name, ((char **)submit->name_array)[j]);
	data->name = name;
        value = 0; /* so that StrAllocCopy doesn't free previous name */
	if (submit->value_array[j]) {
	    StrAllocCopy(value, ((char **)submit->value_array)[j]);
	} else {
            StrAllocCopy(value, ""); /* insures that value is not null */
	}
	data->value = value;
	if (data->isPassword) {
	    si_Randomize(data->value);
	}

	/* append new data node to end of data list */
	XP_ListAddObjectToEnd (user->signonData_list, data);
    }

    /* append new user node to front of user list for matching URL */
	/*
	 * Note that by appending to the front, we assure that if there are
	 * several users, the most recently used one will be favored for
         * reuse the next time this form is encountered.  But don't do this
	 * when reading in signons.txt (i.e., when save is false), otherwise
	 * we will be reversing the order when reading in.
	 */
    if (save) {
	XP_ListAddObject(URL->signonUser_list, user);
	si_signon_list_changed = TRUE;
	si_SaveSignonDataLocked(NULL);
	si_unlock_signon_list();
    } else {
	XP_ListAddObjectToEnd(URL->signonUser_list, user);
    }
}

/*
 * When a new form is started, set the tagCount to 0
 */
PUBLIC void
SI_StartOfForm() {
    si_TagCount = 0;
}

/*
 * Load signon data from disk file
 * The parameter passed in on entry is ignored
 */

#define BUFFER_SIZE 4096
#define MAX_ARRAY_SIZE 50
PUBLIC int
SI_LoadSignonData(char * filename) {
    char * URLName;
    LO_FormSubmitData submit;
    char* name_array[MAX_ARRAY_SIZE];
    char* value_array[MAX_ARRAY_SIZE];
    uint8 type_array[MAX_ARRAY_SIZE];
    char buffer[BUFFER_SIZE];
    XP_File fp;
    Bool badInput;
    int i;
    char* unmungedValue;

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()) {
	return 0;
    }

    /* open the signon file */
    if(!(fp = XP_FileOpen(filename, xpHTTPSingleSignon, XP_FILE_READ)))
	return(-1);

    /* initialize the submit structure */
    submit.name_array = (PA_Block)name_array;
    submit.value_array = (PA_Block)value_array;
    submit.type_array = (PA_Block)type_array;

    /* read the URL line */
    si_lock_signon_list();
    while(XP_FileReadLine(buffer, BUFFER_SIZE, fp)) {

	/* ignore comment lines at head of file */
        if (buffer[0] == '#') {
	    continue;
	}
	si_StripLF(buffer);
	URLName = NULL;
	StrAllocCopy(URLName, buffer);

	/* preparre to read the name/value pairs */
	submit.value_cnt = 0;
	badInput = FALSE;

	while(XP_FileReadLine(buffer, BUFFER_SIZE, fp)) {

	    /* line starting with . terminates the pairs for this URL entry */
            if (buffer[0] == '.') {
		break; /* end of URL entry */
	    }

	    /* line just read is the name part */

	    /* save the name part and determine if it is a password */
	    si_StripLF(buffer);
	    name_array[submit.value_cnt] = NULL;
            if (buffer[0] == '*') {
		type_array[submit.value_cnt] = FORM_TYPE_PASSWORD;
		StrAllocCopy(name_array[submit.value_cnt], buffer+1);
	    } else {
		type_array[submit.value_cnt] = FORM_TYPE_TEXT;
		StrAllocCopy(name_array[submit.value_cnt], buffer);
	    }

	    /* read in and save the value part */
	    if(!XP_FileReadLine(buffer, BUFFER_SIZE, fp)) {
		/* error in input file so give up */
		badInput = TRUE;
		break;
	    }
	    si_StripLF(buffer);
	    value_array[submit.value_cnt] = NULL;
            /* note that we need to skip over leading '=' of value */
	    if (type_array[submit.value_cnt] == FORM_TYPE_PASSWORD) {
		if ((unmungedValue=SECNAV_UnMungeString(buffer+1)) == NULL) {
		    /* this is the free source and there is no obscuring of passwords */
		    unmungedValue = buffer+1;
		}
		StrAllocCopy(value_array[submit.value_cnt++], unmungedValue);
		XP_FREE(unmungedValue);
	    } else {
		StrAllocCopy(value_array[submit.value_cnt++], buffer+1);
	    }

	    /* check for overruning of the arrays */
	    if (submit.value_cnt >= MAX_ARRAY_SIZE) {
		break;
	    }
	}

	/* store the info for this URL into memory-resident data structure */
	if (!URLName || XP_STRLEN(URLName) == 0) {
	    badInput = TRUE;
	}
	if (!badInput) {
	    si_PutData(URLName, &submit, FALSE);
	}

	/* free up all the allocations done for processing this URL */
	XP_FREE(URLName);
	for (i=0; i<submit.value_cnt; i++) {
	    XP_FREE(name_array[i]);
	    XP_FREE(value_array[i]);
	}
	if (badInput) {
	    return (1);
	}
    }
    si_unlock_signon_list();
    return(0);
}


/*
 * Save signon data to disk file
 * The parameter passed in on entry is ignored
 *
 * This routine is called only when holding the signon lock!!!
 *
 * This routine is called only if signon pref is enabled!!!
 */

PRIVATE int
si_SaveSignonDataLocked(char * filename) {
    XP_List * list_ptr;
    XP_List * user_ptr;
    XP_List * data_ptr;
    si_SignonURLStruct * URL;
    si_SignonUserStruct * user;
    si_SignonDataStruct * data;
    XP_File fp;

    /* do nothing if signon list has not changed */
    list_ptr = si_signon_list;
    if(!si_signon_list_changed) {
	return(-1);
    }

    /* do nothing is signon list is empty */
    if(XP_ListIsEmpty(si_signon_list)) {
	return(-1);
    }

    /* do nothing if we are unable to open file that contains signon list */
    if(!(fp = XP_FileOpen(filename, xpHTTPSingleSignon, XP_FILE_WRITE))) {
	return(-1);
    }

    /* write out leading comments */
    XP_FileWrite("# Netscape HTTP Signons File" LINEBREAK
                  "# This is a generated file!  Do not edit."
                  LINEBREAK "#" LINEBREAK, -1, fp);

    /* format shall be:
     * url LINEBREAK {name LINEBREAK value LINEBREAK}*	. LINEBREAK
     * if type is signon, name is preceded by an asterisk (*)
     */

    /* write out each URL node */
    while((URL = (si_SignonURLStruct *)
	    XP_ListNextObject(list_ptr)) != NULL) {

	user_ptr = URL->signonUser_list;

	/* write out each user node of the URL node */
	while((user = (si_SignonUserStruct *)
		XP_ListNextObject(user_ptr)) != NULL) {
	    XP_FileWrite(URL->URLName, -1, fp);
	    XP_FileWrite(LINEBREAK, -1, fp);

	    data_ptr = user->signonData_list;

	    /* write out each data node of the user node */
	    while((data = (si_SignonDataStruct *)
		    XP_ListNextObject(data_ptr)) != NULL) {
		char* mungedValue;
		if (data->isPassword) {
                    XP_FileWrite("*", -1, fp);
		}
		XP_FileWrite(data->name, -1, fp);
		XP_FileWrite(LINEBREAK, -1, fp);
                XP_FileWrite("=", -1, fp); /* precede values with '=' */
		if (data->isPassword) {
		    if ((mungedValue = SECNAV_MungeString(data->value))) {
			XP_FileWrite(mungedValue, -1, fp);
			XP_FREE(mungedValue);
		    } else {
			/* this is the free source and passwords are not obscured */
			XP_FileWrite(data->value, -1, fp);
		    }
		} else {
		    XP_FileWrite(data->value, -1, fp);
		}
		XP_FileWrite(LINEBREAK, -1, fp);
	    }
            XP_FileWrite(".", -1, fp);
	    XP_FileWrite(LINEBREAK, -1, fp);
	}
    }

    si_signon_list_changed = FALSE;
    XP_FileClose(fp);
    return(0);
}


/*
 * Save signon data to disk file
 * The parameter passed in on entry is ignored
 */

PUBLIC int
SI_SaveSignonData(char * filename) {
    int retval;

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()) {
	return FALSE;
    }

    /* lock and call common save routine */
    si_lock_signon_list();
    retval = si_SaveSignonDataLocked(filename);
    si_unlock_signon_list();
    return retval;
}

/*
 * Remove all the signons and free everything
 */

PUBLIC void
SI_RemoveAllSignonData() {

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()) {
	return;
    }

    /* repeatedly remove first user node of first URL node */
    while (SI_RemoveUser(NULL, NULL, FALSE)) {
    }
}

/*
 * Check for a signon submission and remember the data if so
 */
PUBLIC void
SI_RememberSignonData(MWContext *context, LO_FormSubmitData * submit)
{
    int i;
    int passwordCount = 0;
    int pswd[3];

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	return;
    }

    /* determine how many passwords are in the form and where they are */
    for (i=0; i<submit->value_cnt; i++) {
	if (((uint8 *)submit->type_array)[i] == FORM_TYPE_PASSWORD) {
	    if (passwordCount < 3 ) {
		pswd[passwordCount] = i;
	    }
	    passwordCount++;
	}
    }

    /* process the form according to how many passwords it has */
    if (passwordCount == 1) {
	/* one-password form is a log-in so remember it */
	si_PutData(context->hist.cur_doc_ptr->address, submit, TRUE);

    } else if (passwordCount == 2) {
	/* two-password form is a registration */

    } else if (passwordCount == 3) {
	/* three-password form is a change-of-password request */

	XP_List * data_ptr;
	si_SignonDataStruct* data;
	si_SignonUserStruct* user;

	/* make sure all passwords are non-null and 2nd and 3rd are identical */
	if (!submit->value_array[pswd[0]] || ! submit->value_array[pswd[1]] ||
		!submit->value_array[pswd[2]] ||
		XP_STRCMP(((char **)submit->value_array)[pswd[1]],
		       ((char **)submit->value_array)[pswd[2]])) {
	    return;
	}

	/* ask user if this is a password change */
	si_lock_signon_list();
	user = si_GetUserForChangeForm(
	    context,
	    context->hist.cur_doc_ptr->address,
	    MK_SIGNON_PASSWORDS_REMEMBER);

	/* return if user said no */
	if (!user) {
	    si_unlock_signon_list();
	    return;
	}

	/* get to password being saved */
	data_ptr = user->signonData_list;
	while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
	    if (data->isPassword) {
		break;
	    }
	}

	/*
         * if second password is "generate" then generate a random
	 * password for it and use same random value for third password
	 * as well (Note: this all works because we already know that
	 * second and third passwords are identical so third password
         * must also be "generate".  Furthermore si_Randomize() will
	 * create a random password of exactly eight characters -- the
         * same length as "generate".)
	 */
	si_Randomize(((char **)submit->value_array)[pswd[1]]);
	XP_STRCPY(((char **)submit->value_array)[pswd[2]],
	       ((char **)submit->value_array)[pswd[1]]);
	StrAllocCopy(data->value, ((char **)submit->value_array)[pswd[1]]);
	si_signon_list_changed = TRUE;
	si_SaveSignonDataLocked(NULL);
	si_unlock_signon_list();
    }
}

/*
 * Check for remembered data from a previous signon submission and
 * restore it if so
 */
PUBLIC void
SI_RestoreOldSignonData
    (MWContext *context, LO_FormElementStruct *form_element, char* URLName)
{
    si_SignonURLStruct* URL;
    si_SignonUserStruct* user;
    si_SignonDataStruct* data;
    XP_List * data_ptr=0;

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	return;
    }

    /* get URL */
    si_lock_signon_list();
    URL = si_GetURL(URLName);

    /*
     * if we leave a form without submitting it, the URL's state variable
     * will not get reset.  So we test for that case now and reset the
     * state variables if necessary
     */
    if (lastURL != URL) {
	if (lastURL) {
	    lastURL -> chosen_user = NULL;
	    lastURL -> firsttime_chosing_user = TRUE;
	}
    }
    lastURL = URL;

    /* see if this is first item in form and is a password */
    si_TagCount++;
    if ((si_TagCount == 1) &&
	    (form_element->element_data->type == FORM_TYPE_PASSWORD)) {
	/*
         * it is so there's a good change its a password change form,
         * let's ask user to confirm this
	 */
	user = si_GetUserForChangeForm(context, URLName, MK_SIGNON_PASSWORDS_FETCH);
	if (user) {
            /* user has confirmed it's a change-of-password form */
	    data_ptr = user->signonData_list;
	    data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
	    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
		if (data->isPassword) {
#ifdef XP_MAC
		    StrAllocCopy(
			(char *)form_element->element_data->ele_text.default_text,
			data->value);
#else
		    char* default_text =
			(char*)(form_element->element_data->ele_text.default_text);
		    StrAllocCopy(default_text, data->value);
		    form_element->element_data->ele_text.default_text =
			(unsigned long *)default_text;
#endif
		    si_unlock_signon_list();
		    return;
		}
	    }
	}
    }

    /* get the data from previous time this URL was visited */

    if (si_TagCount == 1) {
	user = si_GetUser(context, URLName, FALSE);
    } else {
	if (URL) {
	    user = URL->chosen_user;
	} else {
	    user = NULL;
	}
    }
    if (user) {

	/* restore the data from previous time this URL was visited */
	data_ptr = user->signonData_list;
	while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
	    if(XP_STRCMP(data->name,
		    (char *)form_element->element_data->ele_text.name)==0) {
#ifdef XP_MAC
		StrAllocCopy(
		    (char *)form_element->element_data->ele_text.default_text,
		    data->value);
#else
		char* default_text =
		    (char*)(form_element->element_data->ele_text.default_text);
		StrAllocCopy(default_text, data->value);
		form_element->element_data->ele_text.default_text =
		    (unsigned long *)default_text;
#endif
		si_unlock_signon_list();
		return;
	    }
	}
    }
    si_unlock_signon_list();
}

/*
 * Remember signon data from a browser-generated password dialog
 */
PRIVATE void
si_RememberSignonDataFromBrowser(char* URLName, char* username, char* password)
{
    LO_FormSubmitData submit;
    char* USERNAME = "username";
    char* PASSWORD = "password";
    char* name_array[2];
    char* value_array[2];
    uint8 type_array[2];

    /* do nothing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	return;
    }

    /* initialize a temporary submit structure */
    submit.name_array = (PA_Block)name_array;
    submit.value_array = (PA_Block)value_array;
    submit.type_array = (PA_Block)type_array;
    submit.value_cnt = 2;

    name_array[0] = USERNAME;
    value_array[0] = NULL;
    StrAllocCopy(value_array[0], username);
    type_array[0] = FORM_TYPE_TEXT;

    name_array[1] = PASSWORD;
    value_array[1] = NULL;
    StrAllocCopy(value_array[1], password);
    type_array[1] = FORM_TYPE_PASSWORD;

    /* Save the signon data */
    si_PutData(URLName, &submit, TRUE);

    /* Free up the data memory just allocated */
    XP_FREE(value_array[0]);
    XP_FREE(value_array[1]);
}

/*
 * Check for remembered data from a previous browser-generated password dialog
 * restore it if so
 */
PRIVATE void
si_RestoreOldSignonDataFromBrowser
    (MWContext *context, char* URLName, Bool pickFirstUser,
    char** username, char** password)
{
    si_SignonUserStruct* user;
    si_SignonDataStruct* data;
    XP_List * data_ptr=0;

    /* get the data from previous time this URL was visited */
    si_lock_signon_list();
    user = si_GetUser(context, URLName, pickFirstUser);
    if (!user) {
	/* leave original username and password from caller unchanged */
	/* username = 0; */
	/* *password = 0; */
	si_unlock_signon_list();
	return;
    }

    /* restore the data from previous time this URL was visited */
    data_ptr = user->signonData_list;
    while((data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr))!=0) {
        if(XP_STRCMP(data->name, "username")==0) {
	    StrAllocCopy(*username, data->value);
        } else if(XP_STRCMP(data->name, "password")==0) {
	    StrAllocCopy(*password, data->value);
	}
    }
    si_unlock_signon_list();
}

/* Browser-generated prompt for user-name and password */
PUBLIC int
SI_PromptUsernameAndPassword
    (MWContext *context, char *prompt,
    char **username, char **password, char *URLName)
{
    int status;
    char *copyOfPrompt=0;

    /* just for safety -- really is a problem in SI_Prompt */
    StrAllocCopy(copyOfPrompt, prompt);

    /* do the FE thing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	status = FE_PromptUsernameAndPassword
		     (context, copyOfPrompt, username, password);
	XP_FREEIF(copyOfPrompt);
	return status;
    }

    /* prefill with previous username/password if any */
    si_RestoreOldSignonDataFromBrowser
	(context, URLName, FALSE, username, password);

    /* get new username/password from user */
    status = FE_PromptUsernameAndPassword
		 (context, copyOfPrompt, username, password);

    /* remember these values for next time */
    if (status) {
	si_RememberSignonDataFromBrowser (URLName, *username, *password);
    }

    /* cleanup and return */
    XP_FREEIF(copyOfPrompt);
    return status;
}

/* Browser-generated prompt for password */
PUBLIC char *
SI_PromptPassword
    (MWContext *context, char *prompt, char *URLName, Bool pickFirstUser)
{
    char *password=0, *username=0, *copyOfPrompt=0, *result;
    char *urlname = URLName;
    char *s;

    /* just for safety -- really is a problem in SI_Prompt */
    StrAllocCopy(copyOfPrompt, prompt);

    /* do the FE thing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	result = FE_PromptPassword(context, copyOfPrompt);
	XP_FREEIF(copyOfPrompt);
	return result;
    }

    /* get host part of URL which is of form user@host */
    s = URLName;
    while (*s && (*s != '@')) {
	s++;
    }
    if (*s) {
	/* @ found, host is URL part following the @ */
	urlname = s+1; /* urlname is part of URL after the @ */
    }

    /* get previous password used with this username */
    si_RestoreOldSignonDataFromBrowser
	(context, urlname, pickFirstUser, &username, &password);

    /* return if a password was found */
    /*
     * Note that we reject a password of " ".  It is a dummy password
     * that was put in by a preceding call to SI_Prompt.  This occurs
     * in mknews which calls on SI_Prompt to get the username and then
     * SI_PromptPassword to get the password (why they didn't simply
     * call on SI_PromptUsernameAndPassword is beyond me).  So the call
     * to SI_Prompt will save the username along with the dummy password.
     * In this call to SI_Password, the real password gets saved in place
     * of the dummy one.
     */
    if (password && XP_STRLEN(password) && XP_STRCMP(password, " ")) {
	XP_FREEIF(copyOfPrompt);
	return password;
    }

    /* if no password found, get new password from user */
    password = FE_PromptPassword(context, copyOfPrompt);

    /* if username wasn't even found, extract it from URLName */
    if (!username) {
	s = URLName;

	/* get to @ in URL if any */
        while (*s && (*s != '@')) {
	    s++;
	}
	if (*s) {
	    /* @ found, username is URL part preceding the @ */
            *s = '\0';
	    StrAllocCopy(username, URLName);
            *s = '@';
	} else {
	    /* no @ found, use entire URL as uername */
	    StrAllocCopy(username, URLName);
	}
    }

    /* remember these values for next time */
    if (password && XP_STRLEN(password)) {
	si_RememberSignonDataFromBrowser (urlname, username, password);
    }

    /* cleanup and return */
    XP_FREEIF(username);
    XP_FREEIF(copyOfPrompt);
    return password;
}

/* Browser-generated prompt for username */
PUBLIC char *
SI_Prompt (MWContext *context, char *prompt,
    char* defaultUsername, char *URLName)
{
    char *password=0, *username=0, *copyOfPrompt=0, *result;

    /*
     * make copy of prompt because it is currently in a temporary buffer in
     * the caller (from GetString) which will be destroyed if GetString() is
     * called again before prompt is used
     */
    StrAllocCopy(copyOfPrompt, prompt);

    /* do the FE thing if signon preference is not enabled */
    if (!si_GetSignonRememberingPref()){
	result = FE_Prompt(context, copyOfPrompt, defaultUsername);
	XP_FREEIF(copyOfPrompt);
	return result;
    }

    /* get previous username used */
    if (!defaultUsername || !XP_STRLEN(defaultUsername)) {
	si_RestoreOldSignonDataFromBrowser
	    (context, URLName, FALSE, &username, &password);
    } else {
       StrAllocCopy(username, defaultUsername);
    }

    /* prompt for new username */
    result = FE_Prompt(context, copyOfPrompt, username);

    /* remember this username for next time */
    if (result && XP_STRLEN(result)) {
        if (username && (XP_STRCMP(username, result) == 0)) {
            /*
             * User is re-using the same user name as from previous time
             * so keep the previous password as well
             */
            si_RememberSignonDataFromBrowser (URLName, result, password);
        } else {
            /*
             * We put in a dummy password of " " which we will test
             * for in a following call to SI_PromptPassword.  See comments
             * in that routine.
             */
            si_RememberSignonDataFromBrowser (URLName, result, " ");
        }
    }

    /* cleanup and return */
    XP_FREEIF(username);
    XP_FREEIF(password);
    XP_FREEIF(copyOfPrompt);
    return result;
}

#include "mkgeturl.h"
#include "htmldlgs.h"
extern int XP_EMPTY_STRINGS;
extern int SA_VIEW_BUTTON_LABEL;
extern int SA_REMOVE_BUTTON_LABEL;

#define BUFLEN 5000

#define FLUSH_BUFFER			\
    if (buffer) {			\
	StrAllocCat(buffer2, buffer);	\
	g = 0;				\
    }

/* return TRUE if "number" is in sequence of comma-separated numbers */
Bool si_InSequence(char* sequence, int number) {
    char* ptr;
    char* endptr;
    char* undo = NULL;
    Bool retval = FALSE;
    int i;

    /* not necessary -- routine will work even with null sequence */
    if (!*sequence) {
	return FALSE;
    }

    for (ptr = sequence ; ptr ; ptr = endptr) {

	/* get to next comma */
        endptr = XP_STRCHR(ptr, ',');

	/* if comma found, set it to null */
	if (endptr) {

	    /* restore last comma-to-null back to comma */
	    if (undo) {
                *undo = ',';
	    }
	    undo = endptr;
            *endptr++ = '\0';
	}

        /* if there is a number before the comma, compare it with "number" */
	if (*ptr) {
	    i = atoi(ptr);
	    if (i == number) {

                /* "number" was in the sequence so return TRUE */
		retval = TRUE;
		break;
	    }
	}
    }

    if (undo) {
        *undo = ',';
    }
    return retval;
}

MODULE_PRIVATE void
si_DisplayUserInfoAsHTML
    (MWContext *context, si_SignonUserStruct* user, char* URLName)
{
    char *buffer = (char*)XP_ALLOC(BUFLEN);
    char *buffer2 = 0;
    int g = 0;
    XP_List *data_ptr = user->signonData_list;
    si_SignonDataStruct* data;
    int dataNum;

    static XPDialogInfo dialogInfo = {
	XP_DIALOG_OK_BUTTON,
	NULL,
	250,
	200
    };

    XPDialogStrings* strings;
    StrAllocCopy(buffer2, "");

    g += PR_snprintf(buffer+g, BUFLEN-g,
"<FORM><TABLE>\n"
"<TH><CENTER>%s<BR></CENTER><CENTER><SELECT NAME=\"selname\" MULTIPLE SIZE=3>\n",
	URLName);
    FLUSH_BUFFER
    dataNum = 0;

    /* write out details of each data item for user */
    while ( (data=(si_SignonDataStruct *) XP_ListNextObject(data_ptr)) ) {
	g += PR_snprintf(buffer+g, BUFLEN-g,
"<OPTION VALUE=%d>%s=%s</OPTION>",
            dataNum, data->name, data->isPassword ? "***" : data->value);
	FLUSH_BUFFER
	dataNum++;
    }
    g += PR_snprintf(buffer+g, BUFLEN-g,
"</SELECT></CENTER></TH></TABLE></FORM>\n"
	);
    FLUSH_BUFFER

    /* free buffer since it is no longer needed */
    if (buffer) {
	XP_FREE(buffer);
    }

    /* do html dialog */
    strings = XP_GetDialogStrings(XP_EMPTY_STRINGS);
    if (!strings) {
	if (buffer2) {
	    XP_FREE(buffer2);
	}
	return;
    }
    if (buffer2) {
	XP_CopyDialogString(strings, 0, buffer2);
	XP_FREE(buffer2);
	buffer2 = NULL;
    }
    XP_MakeHTMLDialog(context, &dialogInfo, MK_SIGNON_YOUR_SIGNONS,
		strings, context, PR_FALSE);

    return;
}

PR_STATIC_CALLBACK(PRBool)
si_SignonInfoDialogDone
    (XPDialogState* state, char** argv, int argc, unsigned int button)
{
    XP_List *URL_ptr;
    XP_List *user_ptr;
    XP_List *data_ptr;
    si_SignonURLStruct *URL;
    si_SignonURLStruct *URLToDelete = 0;
    si_SignonUserStruct* user;
    si_SignonUserStruct* userToDelete = 0;
    si_SignonDataStruct* data;

    char *buttonName, *userNumberAsString;
    int userNumber;
    char* gone;

    /* test for cancel */
    if (button == XP_DIALOG_CANCEL_BUTTON) {
	/* CANCEL button was pressed */
	return PR_FALSE;
    }

    /* get button name */
    buttonName = XP_FindValueInArgs("button", argv, argc);
    if (buttonName &&
	    !XP_STRCASECMP(buttonName, XP_GetString(SA_VIEW_BUTTON_LABEL))) {

	/* view button was pressed */

        /* get "selname" value in argv list */
        if (!(userNumberAsString = XP_FindValueInArgs("selname", argv, argc))) {
	    /* no selname in argv list */
	    return(PR_TRUE);
	}

        /* convert "selname" value from string to an integer */
	userNumber = atoi(userNumberAsString);

	/* step to the user corresponding to that integer */
	si_lock_signon_list();
	URL_ptr = si_signon_list;
	while ((URL = (si_SignonURLStruct *) XP_ListNextObject(URL_ptr))) {
	    user_ptr = URL->signonUser_list;
	    while ((user = (si_SignonUserStruct *)
		    XP_ListNextObject(user_ptr))) {
		if (--userNumber < 0) {
		    si_DisplayUserInfoAsHTML
			((MWContext *)(state->arg), user, URL->URLName);
		    return(PR_TRUE);
		}
	    }
	}
	si_unlock_signon_list();
    }

    /* OK was pressed, do the deletions */

    /* first get the comma-separated sequence of signons to be deleted */
    gone = XP_FindValueInArgs("goneC", argv, argc);
    XP_ASSERT(gone);
    if (!gone) {
	return PR_FALSE;
    }

    /* step through all users and delete those that are in the sequence
     * Note: we can't delete user while "user_ptr" is pointing to it because
     * that would destroy "user_ptr". So we do a lazy deletion
     */
    userNumber = 0;
    URL_ptr = si_signon_list;
    while ((URL = (si_SignonURLStruct *) XP_ListNextObject(URL_ptr))) {
	user_ptr = URL->signonUser_list;
	while ((user = (si_SignonUserStruct *)
		XP_ListNextObject(user_ptr))) {
	    if (si_InSequence(gone, userNumber)) {
		if (userToDelete) {

                    /* get to first data item -- that's the user name */
		    data_ptr = userToDelete->signonData_list;
		    data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
		    /* do the deletion */
		    SI_RemoveUser(URLToDelete->URLName, data->value, TRUE);
		}
		userToDelete = user;
		URLToDelete = URL;
	    }
	    userNumber++;
	}
    }
    if (userToDelete) {
        /* get to first data item -- that's the user name */
	data_ptr = userToDelete->signonData_list;
	data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);

	/* do the deletion */
	SI_RemoveUser(URLToDelete->URLName, data->value, TRUE);
	si_signon_list_changed = TRUE;
	si_SaveSignonDataLocked(NULL);
    }

    return PR_FALSE;
}

PUBLIC void
SI_DisplaySignonInfoAsHTML(MWContext *context)
{
    char *buffer = (char*)XP_ALLOC(BUFLEN);
    char *buffer2 = 0;
    int g = 0, userNumber;
    XP_List *URL_ptr;
    XP_List *user_ptr;
    XP_List *data_ptr;
    si_SignonURLStruct *URL;
    si_SignonUserStruct * user;
    si_SignonDataStruct* data;

    static XPDialogInfo dialogInfo = {
	XP_DIALOG_OK_BUTTON | XP_DIALOG_CANCEL_BUTTON,
	si_SignonInfoDialogDone,
	300,
	420
    };

    XPDialogStrings* strings;
    StrAllocCopy(buffer2, "");

    /* Write out the javascript */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"<script>\n"
"function DeleteSignonSelected() {\n"
"  selname = document.theform.selname;\n"
"  goneC = document.theform.goneC;\n"
"  var p;\n"
"  var i;\n"
"  for (i=selname.options.length-1 ; i>=0 ; i--) {\n"
"    if (selname.options[i].selected) {\n"
"      selname.options[i].selected = 0;\n"
"      goneC.value = goneC.value + \",\" + selname.options[i].value;\n"
"      for (j=i ; j<selname.options.length ; j++) {\n"
"        selname.options[j] = selname.options[j+1];\n"
"      }\n"
"    }\n"
"  }\n"
"}\n"
"</script>\n"
	);
    FLUSH_BUFFER

    /* force loading of the signons file */
    si_RegisterSignonPrefCallbacks();
    si_lock_signon_list();
    URL_ptr = si_signon_list;

    /* Write out each URL */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"<FORM><TABLE>\n"
"<TH><CENTER>%s<BR></CENTER><CENTER><SELECT NAME=\"selname\" MULTIPLE SIZE=15>\n",
	XP_GetString(MK_SIGNON_YOUR_SIGNONS));
    FLUSH_BUFFER
    userNumber = 0;
    while ( (URL=(si_SignonURLStruct *) XP_ListNextObject(URL_ptr)) ) {
	user_ptr = URL->signonUser_list;
	while ( (user=(si_SignonUserStruct *) XP_ListNextObject(user_ptr)) ) {
	    /* first data item for user is the username */
	    data_ptr = user->signonData_list;
	    data = (si_SignonDataStruct *) XP_ListNextObject(data_ptr);
	    g += PR_snprintf(buffer+g, BUFLEN-g,
"<OPTION VALUE=%d>%s: %s</OPTION>",
		userNumber, URL->URLName, data->value);
	    FLUSH_BUFFER
	    userNumber++;
	}
    }
    si_unlock_signon_list();
    g += PR_snprintf(buffer+g, BUFLEN-g,
"</SELECT></CENTER>\n"
	);
    FLUSH_BUFFER

    g += PR_snprintf(buffer+g, BUFLEN-g,
"<CENTER>\n"
"<INPUT TYPE=\"BUTTON\" VALUE=%s ONCLICK=\"DeleteSignonSelected();\">\n"
"<INPUT TYPE=\"BUTTON\" NAME=\"view\" VALUE=%s ONCLICK=\"parent.clicker(this,window.parent)\">\n"
"<INPUT TYPE=\"HIDDEN\" NAME=\"goneC\" VALUE=\"\">\n"
"</CENTER></TH>\n"
"</TABLE></FORM>\n",
	XP_GetString(SA_REMOVE_BUTTON_LABEL),
	XP_GetString(SA_VIEW_BUTTON_LABEL) );
    FLUSH_BUFFER

    /* free buffer since it is no longer needed */
    if (buffer) {
	XP_FREE(buffer);
    }

    /* do html dialog */
    strings = XP_GetDialogStrings(XP_EMPTY_STRINGS);
    if (!strings) {
	if (buffer2) {
	    XP_FREE(buffer2);
	}
	return;
    }
    if (buffer2) {
	XP_CopyDialogString(strings, 0, buffer2);
	XP_FREE(buffer2);
	buffer2 = NULL;
    }
    XP_MakeHTMLDialog(context, &dialogInfo, MK_SIGNON_YOUR_SIGNONS,
		strings, context, PR_FALSE);

    return;
}

#endif

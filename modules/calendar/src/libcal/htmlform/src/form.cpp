/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */ 

#include "jdefines.h"
#include "julnstr.h"

#include <calendar.h>
#include <gregocal.h>
#include <datefmt.h>
#include <time.h>
#include <unistring.h>
#include "smpdtfmt.h"
#include <simpletz.h>
#include "datetime.h"
#include "jutility.h"
#include "duration.h"

#include "attendee.h"
#include "vevent.h"
#include "icalsrdr.h"
#include "icalfrdr.h"
#include "prprty.h"
#include "icalprm.h"
#include "freebusy.h"
#include "vfrbsy.h"
#include "nscal.h"
#include "keyword.h"

#include "txnobjfy.h"
#include "user.h"
#include "txnobj.h"

#include "jlog.h"
#include "uri.h"

#include "xp.h"
#include "prmon.h"

#include "form.h"

static void julian_handle_close(JulianForm *jf);
#ifdef OSF1
       void julian_handle_accept(JulianForm *jf, int newStatus);
#else
       void julian_handle_accept(JulianForm *jf, Attendee::STATUS newStatus);
#endif
static void julian_handle_moredetail(JulianForm *jf);
static void julian_send_response(JulianString& subject, JulianPtrArray& recipients, JulianString& LoginName, JulianForm& jf, NSCalendar& calendar);

static void Julian_ClearLoginInfo();
static int Julian_GetLoginInfo(JulianForm& jf, MWContext* context, char** url, char** password);

#define julian_pref_name "calendar.login_url"

XP_Bool JulianForm::ms_bFoundNLSDataDirectory = FALSE;

JulianForm *jform_CreateNewForm(char *calendar_mime_data, pJulian_Form_Callback_Struct callbacks,
                                XP_Bool bFoundNLSDataDirectory)
{
	JulianForm	*jf = new JulianForm();

	if (jf)
	{
        jf->refcount = 1;
		jf->setMimeData(calendar_mime_data);
		jf->setCallbacks(callbacks);
        JulianForm::setFoundNLSDataDirectory(bFoundNLSDataDirectory);
	}
	return jf;
}

void jform_DeleteForm(JulianForm *jf)
{
    if (jf)
    {
        jf->refcount--;
        if (jf->refcount == 0)
        {
            delete jf;
        }
    }
}

char* jform_GetForm(JulianForm *jf)
{
    jf->StartHTML();
	return jf->getHTMLForm(FALSE);
}

void jform_CallBack(JulianForm *jf, char *type)
{
	char*			button_type;
	char*			button_type2;
	char*			button_data;
	form_data_combo	fdc;
	int32			x;

    PR_EnterMonitor(jf->my_monitor);
    /*
	** type contains the html form string for this.
	** The gernal format is ? type = label or name = data
	** The last thing in this list is the button that started
	** this.
	*/
	button_type = XP_STRCHR(type, '?');
	button_type++;	// Skip ?. Now points to type

	while (TRUE)
	{
		button_type2 = XP_STRCHR(button_type, '=');
		if (button_type2)
		{
			*button_type2++ = '\0';
			button_data = button_type2; // Points to data part
			button_type2 = XP_STRCHR(button_type2, '&');
		} else {
			button_data = nil;
			button_type2 = nil;
		}
		if (button_type2)
		{
			*button_type2++ = '\0';
		}
		fdc.type = button_type;
		fdc.data = button_data;
		jf->formData[jf->formDataCount] = fdc;
		jf->formDataCount++;
		if (!button_type2 || (jf->formDataCount > formDataIndex))
			break;
		else
			button_type = button_type2;

	}

	for (x=0; x < jf->formDataCount; x++)
	{
		button_type = jf->formData[x].type;
		jf->setLabel( button_data = jf->formData[x].data );

		if (!JulianFormFactory::Buttons_Details_Type.CompareTo(button_type)) 
		{
			julian_handle_moredetail(jf);
		} else
		if (!JulianFormFactory::Buttons_Accept_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_ACCEPTED);
		} else
		if (!JulianFormFactory::Buttons_Add_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_ACCEPTED);
		} else
		if (!JulianFormFactory::Buttons_Close_Type.CompareTo(button_type))
		{
			julian_handle_close(jf);
		} else
		if (!JulianFormFactory::Buttons_Decline_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_DECLINED);
		} else
		if (!JulianFormFactory::Buttons_Tentative_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_TENTATIVE);
		} else
		if (!JulianFormFactory::Buttons_SendFB_Type.CompareTo(button_type))
		{
		} else
		if (!JulianFormFactory::Buttons_SendRefresh_Type.CompareTo(button_type))
		{
		} else
		if (!JulianFormFactory::Buttons_DelTo_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_DELEGATED);
		}
	}

    PR_ExitMonitor(jf->my_monitor);
}

/**
*** JulianForm Class
***
*** This is the c++ interface to JulianFormFactory
***
**/

JulianForm::JulianForm()
{
	mimedata = nil;
	imipCal = nil;
    jff = nil;
	contextName = "Julian:More Details";
	formDataCount = 0;
    my_monitor = PR_NewMonitor();
}

JulianForm::~JulianForm()
{
    if (FALSE && imipCal)
    {
        if (imipCal->getLog())
            delete imipCal->getLog();
        delete imipCal;
		imipCal = nil;
    }

    if (jff)
    {
        delete jff;
    }

    if (my_monitor) PR_DestroyMonitor(my_monitor);
}

XP_Bool
JulianForm::StartHTML()
{
    JulianString		u;
    UnicodeString ust;
	if (imipCal == NULL)
	{
		ICalReader	*tfr = (ICalReader *) new ICalStringReader(mimedata);
        mimedata = nil;
		if (tfr)
		{
            JLog * log = new JLog();
            if (!ms_bFoundNLSDataDirectory)
            {
                if (log != 0)
                {
                    // TODO: finish
                    //log->log("ERROR: Can't find REQUIRED NLS Data Directory\n");
                }
            }
			imipCal = new NSCalendar(log);
            ust = u.GetBuffer();
			if (imipCal) imipCal->parse(tfr, ust);
			delete tfr;
		}

        return TRUE;
	}

    return FALSE;
}

char*
JulianForm::getHTMLForm(XP_Bool Want_Detail, NET_StreamClass *this_stream)
{
	if (imipCal != nil)
	{
		if ((jff = new JulianFormFactory(*imipCal, *this, getCallbacks())) != nil)
		{
            htmlForm = "";	// Empty it

            jff->init();
		    jff->getHTML(&htmlForm, this_stream, Want_Detail);
		}
	}

    char* t2 = (char*) XP_ALLOC(htmlForm.GetStrlen() + 1);
    if (t2) strcpy(t2, htmlForm.GetBuffer());
	return t2;
}

void
JulianForm::setMimeData(char *NewMimeData)
{
	if (NewMimeData)
	{
		mimedata = NewMimeData;
	}
}

char*
JulianForm::getComment()
{
	for (int32 x=0; x < formDataCount; x++)
	{
		if (!JulianFormFactory::CommentsFieldName.CompareTo(formData[x].type))
		{
			(*getCallbacks()->PlusToSpace)(formData[x].data);
			(*getCallbacks()->UnEscape)	 (formData[x].data);
			return formData[x].data;
		}
	}

	return nil;
}

char*
JulianForm::getDelTo()
{
	for (int32 x=0; x < formDataCount; x++)
	{
        char* temp;

        temp = PR_smprintf( "%s", formData[x].type); // Where is a pr_strcpy??
        if (temp)
        {
            (*getCallbacks()->PlusToSpace)(temp);
		    if (!XP_STRCMP(jff->Buttons_DelTo_Label.GetBuffer(), temp))
		    {
			    (*getCallbacks()->PlusToSpace)(formData[x].data);
			    (*getCallbacks()->UnEscape)	 (formData[x].data);
			    return formData[x].data;
		    }
            PR_DELETE(temp);
        }
	}

	return nil;
}

void
julian_handle_close(JulianForm *jf)
{
	MWContext*	cx = nil;

    if ((cx = jf->getContext()) != nil)
	{
		(*jf->getCallbacks()->DestroyWindow)(cx);
	}
}

void
julian_handle_moredetail(JulianForm *jf)
{
	MWContext*	        cx = nil;
    char*		        newhtml = NULL;
	NET_StreamClass*    stream = nil;
    URL_Struct*         url = nil;

    if (jf->getCallbacks() == nil ||
        jf->getCallbacks()->CreateURLStruct == nil)
        return;

	url = (*jf->getCallbacks()->CreateURLStruct)("internal_url://", NET_RESIZE_RELOAD);
    if (url)
    {
        url->internal_url = TRUE;
	    (*jf->getCallbacks()->SACopy)(&(url->content_type), TEXT_HTML);

	    // Look to see if we already made a window for this
	    if ((cx = jf->getContext()) != nil)
	    {
		    (*jf->getCallbacks()->RaiseWindow)(cx);
	    }

        //
        // If the more details window isn't there,
        // make one
        //
	    if (!cx)
	    {
            Chrome* customChrome = XP_NEW_ZAP(Chrome);

		    /* make the window */
            if (customChrome)
            {
                customChrome->show_scrollbar = TRUE;		 /* TRUE to show scrollbars on window */
                customChrome->allow_resize = TRUE;			 /* TRUE to allow resize of windows */
                customChrome->allow_close = TRUE;			 /* TRUE to allow window to be closed */
                customChrome->disable_commands = TRUE;		 /* TRUE if user has set hot-keys / menus off */
                customChrome->restricted_target = TRUE;		 /* TRUE if window is off-limits for opening links into */
            }

		    cx = (*jf->getCallbacks()->MakeNewWindow)((*jf->getCallbacks()->FindSomeContext)(), nil, jf->contextName, customChrome);
	    }

	    if (cx)
	    {
	        static PA_InitData data;

		    /* make a netlib stream to display in the window */
	        data.output_func = jf->getCallbacks()->ProcessTag;
		    stream = (*jf->getCallbacks()->BeginParseMDL)(FO_CACHE_AND_VIEW_SOURCE | FO_CACHE_AND_PRESENT_INLINE, &data, url, cx);
            if (stream)
            {
                jf->StartHTML();
                jf->getHTMLForm(TRUE, stream);
			    (*stream->complete) (stream->data_object);
                XP_FREE(stream);
            }
        } 
    }
}

void
julian_send_response(JulianString& subject, JulianPtrArray& recipients, JulianString& LoginName, JulianForm& jf, NSCalendar& calendar)
{
	TransactionObject::ETxnErrorCode txnStatus;
	TransactionObject*	txnObj;
	JulianPtrArray*		capiModifiers = 0;

    if (jf.getCallbacks() == nil ||
        jf.getCallbacks()->FindSomeContext == nil)
        return;

	MWContext* this_context = (*jf.getCallbacks()->FindSomeContext)();

	capiModifiers = new JulianPtrArray();

	if (capiModifiers)
	{
        UnicodeString usMeUri = LoginName.GetBuffer();
		URI meUri(usMeUri);
		User* uFrom = new User(usMeUri, meUri.getName());
        char *capurl = NULL, *passwd = NULL;
        UnicodeString uSubject;
        UnicodeString uLoginName;

        if (Julian_GetLoginInfo(jf, this_context, &capurl, &passwd) > 0)
        {
            char*   calUser = "";
            char*   calHost = "";
            char*   calNode = "10000";
            char*   temp;

            // Skip pass "capi://", if it is there
            if (XP_STRSTR(capurl, "://"))
            {
               capurl = XP_STRSTR(capurl, "://");
               capurl += 3;
            }

            // Break apart the user and host:node parts
            temp = XP_STRSTR(capurl, "/");
            if (temp)
            {
                calUser = temp;
                *(calUser) = '\0';
                calUser++;
                calHost = capurl;
            }

            // Break apart the host and node parts
            temp = XP_STRSTR(calHost, ":");
            if (temp)
            {
                calNode = temp;
                *(calNode++) = '\0';
            }

            // Currently the URL login form is as follows:
            // capi://host:node/login
            // i.e. 
            // capi://calendar-1.mcom.com:10000/John Sun
            // TODO: this may change.

            uFrom->setRealName(calUser);
            uFrom->setCAPIInfo(calUser, passwd, calHost, calNode);
            if (0)
            {
                if (passwd) XP_FREE(passwd);
                if (capurl) XP_FREE(capurl);
            }
        }

        uSubject = subject.GetBuffer();
        uLoginName = LoginName.GetBuffer();

		txnObj = TransactionObjectFactory::Make(calendar, *(calendar.getEvents()),
												*uFrom, recipients, uSubject, *capiModifiers,
												&jf, this_context, uLoginName);
		if (txnObj != 0)
		{
			txnObj->execute(0, txnStatus);
			delete txnObj; txnObj = 0;
		}

		if (uFrom) delete uFrom;
	    
        delete capiModifiers;
	}

	// If more details windows, then close it
	// Only if there were no problems
	if (txnStatus == TransactionObject::TR_OK)
	{
		if (jf.getContext())
		{
			julian_handle_close(&jf);
		}
	}
}

void
#ifdef OSF1
julian_handle_accept(JulianForm *jf, int newStatus)
#else
julian_handle_accept(JulianForm *jf, Attendee::STATUS newStatus)
#endif
{
	NSCalendar*		imipCal = jf->getCalendar();
	ICalComponent*	component;
    JulianString	orgName;
    char*			name = nil;
	JulianString	nameU;
	JulianString	subject = JulianString(jf->getLabel());

     // this should be set to the logged in user
	if (jf->getCallbacks()->CopyCharPref)
		(*jf->getCallbacks()->CopyCharPref)("mail.identity.useremail", &name);
	if (name)
	{
		nameU = "MAILTO:";
		nameU += name;
	}

	if (imipCal->getEvents() != 0)
    {
		XP_Bool firstTime = TRUE;
        // process events to send
        UnicodeString uName;
        JulianPtrArray * vDelegatedTo = 0;
        char * deltonames = 0;
        UnicodeString mailto = "MAILTO:";
        UnicodeString del_name;
        char * nextname;
        XP_Bool bDontSend = FALSE;

		for (int32 i = 0; i < imipCal->getEvents()->GetSize(); i++)
		{
			component = (ICalComponent *) imipCal->getEvents()->GetAt(i);

			if (component->GetType() == ICalComponent::ICAL_COMPONENT_VEVENT)
			{
				// Set org
				if (orgName.GetStrlen() == 0) orgName = (((TimeBasedEvent *)component)->getOrganizer()).toCString("");

				// Find out who logged in user is
                uName = nameU.GetBuffer();
                // TODO: send th
                if (newStatus == Attendee::STATUS_DELEGATED) 
                {
                    deltonames = jf->getDelTo();
                     
                    if ((0 != deltonames) && (XP_STRLEN(deltonames) > 0))
                    {
                        vDelegatedTo = new JulianPtrArray();
                        if (0 != vDelegatedTo)
                        {
		    			    while (*deltonames)
			    		    {
				    		    del_name = mailto;
					    	    nextname = deltonames;

                                while (nextname && *nextname && *nextname != ' ')
                                {
                                    nextname++;
                                }

                                if (*nextname)
                                {
                                    *nextname = '\0';
    							    nextname++;
                                }

						        del_name += deltonames;
                                vDelegatedTo->Add(new UnicodeString(del_name));

	    					    if (!(*nextname))
		    				    {
			    				    // Last one
				    			    break;
                                }
                                else
                                {						    	 
                                    deltonames = nextname;
                                }
				            }
                        }
                        else
                        {
                            bDontSend = TRUE;
                        }
                    }
                    else
                    {
                        bDontSend = TRUE;
                    }
                }
				((TimeBasedEvent *)component)->setAttendeeStatusInt(uName, newStatus, vDelegatedTo);
                if (0 != vDelegatedTo)
                {
                    ICalComponent::deleteUnicodeStringVector(vDelegatedTo);
                    delete vDelegatedTo; vDelegatedTo = 0;
                }
				if (jf->hasComment())
				{
					((TimeBasedEvent *)component)->setNewComments(jf->getComment());
				}
				((TimeBasedEvent *)component)->stamp();

				// Right now allways use the last Organizer. Should be the same for all
				// events etc.
				orgName = (((TimeBasedEvent *)component)->getOrganizer()).toCString("");

				subject += JulianFormFactory::SubjectSep;
				subject += (((TimeBasedEvent *)component)->getSummary()).toCString("");
			}
		}

		if (firstTime)
		{
			JulianPtrArray*	recipients = new JulianPtrArray();

			// set method to reply
            if (imipCal->getMethod() != NSCalendar::METHOD_PUBLISH)
            {
			    imipCal->setMethod(NSCalendar::METHOD_REPLY);
                if ((newStatus != Attendee::STATUS_DELEGATED) || 
                    (newStatus == Attendee::STATUS_DELEGATED && !bDontSend))
                {
                    UnicodeString usOrgName = orgName.GetBuffer();
	    			URI orgUri(usOrgName);
		    		User* uToOrg = new User(usOrgName, orgUri.getName());
			    	recipients->Add(uToOrg);
                }
            }

			julian_send_response(subject, *recipients, nameU, *jf, *imipCal);
			firstTime = FALSE;

			User::deleteUserVector(recipients);
            delete recipients;
		}
    }

	if (name) XP_FREE(name);
}

void
Julian_ClearLoginInfo()
{
}

int 
Julian_GetLoginInfo(JulianForm& jf, MWContext* context, char** url, char** password) { 
    static char* lastCalPwd = NULL; 
    char* defaultUrl; 
    int status = -2; 
  
    if (jf.getCallbacks() &&
        jf.getCallbacks()->CopyCharPref)
        status = (*jf.getCallbacks()->CopyCharPref)(julian_pref_name, &defaultUrl);
    // -1 is pref isn't there, which will be a normal case for us
    // Any other neg number is some other bad error so bail.
    if (status < -1) return status; 

    *url = defaultUrl; 
    *password = NULL;

    if (lastCalPwd != NULL)
    { 
        *password = XP_STRDUP(lastCalPwd); 
        if (*password == NULL) return 0 /*MK_OUT_OF_MEMORY*/;

        return 1;
    } 

   if (!(*context->funcs->PromptUsernameAndPassword)
                                      (context, 
//                                    XP_GetString(JULIAN_LOGIN_PROMPT), 
                                      "Enter capi url and password", 
                                      url, 
                                      password))
    { 
        *url = NULL; 
        *password = NULL;
        status = -1;
    } else { 
        if (jf.getCallbacks() &&
            jf.getCallbacks()->SetCharPref)
        {
            (*jf.getCallbacks()->SetCharPref)(julian_pref_name, *url); 
        }
        lastCalPwd = XP_STRDUP(*password);
        status  = 1;
    } 
    XP_FREE(defaultUrl); 
    return status; 
}


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

#include "uri.h"

#include "xp.h"
#include "fe_proto.h"

#include "form.h"
#include "formFactory.h"

static void julian_handle_close(JulianForm *jf);
static void julian_handle_accept(JulianForm *jf, Attendee::STATUS newStatus);
static void julian_handle_moredetail(JulianForm *jf);
static void julian_send_response(UnicodeString& subject, UnicodeString& orgName, UnicodeString& LoginName, JulianForm& jf, NSCalendar& calendar);

JulianForm *jform_CreateNewForm(char *calendar_mime_data, pJulian_Form_Callback_Struct callbacks)
{
	JulianForm	*jf = new JulianForm();

	if (jf)
	{
		jf->setMimeData(calendar_mime_data);
		jf->setCallbacks(callbacks);
	}
	return jf;
}

void jform_DeleteForm(JulianForm *jf)
{
	if (jf) delete jf;
}

char* jform_GetForm(JulianForm *jf)
{
	return jf->getHTMLForm(FALSE);
}

void jform_CallBack(JulianForm *jf, char *type)
{
	char*			button_type;
	char*			button_type2;
	char*			button_data;
	form_data_combo	fdc;
	int32			x;
	/*
	** type contains the html form string for this.
	** The gernal format is ? type = label or name = data
	** The last thing in this list is the button that started
	** this.
	*/
	button_type = XP_STRCHR(type, '?');
	button_type++;	// Skip ?. Now points to type

	while (1)
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

		if (!JulianFormFactory::Buttons_Details_Type.compare(button_type)) 
		{
			julian_handle_moredetail(jf);
		} else
		if (!JulianFormFactory::Buttons_Accept_Type.compare(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_ACCEPTED);
		} else
		if (!JulianFormFactory::Buttons_Add_Type.compare(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_ACCEPTED);
		} else
		if (!JulianFormFactory::Buttons_Close_Type.compare(button_type))
		{
			julian_handle_close(jf);
		} else
		if (!JulianFormFactory::Buttons_Decline_Type.compare(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_DECLINED);
		} else
		if (!JulianFormFactory::Buttons_Tentative_Type.compare(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_TENTATIVE);
		} else
		if (!JulianFormFactory::Buttons_SendFB_Type.compare(button_type))
		{
		} else
		if (!JulianFormFactory::Buttons_SendRefresh_Type.compare(button_type))
		{
		} else
		if (!JulianFormFactory::Buttons_DelTo_Type.compare(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_DELEGATED);
		}
	}
}

JulianForm::JulianForm()
{
	mimedata = nil;
	imipCal = nil;
	moredetails_context = nil;
	contextName = "Julian:More Details";
	formDataCount = 0;
}

JulianForm::~JulianForm()
{
	if (mimedata) free(mimedata);
}

char*
JulianForm::getHTMLForm(XP_Bool Want_Detail)
{
JulianFormFactory	*jff = nil;
UnicodeString		u;
char				*htmltext = nil;


	if (TRUE /*imipCal == NULL*/)
	{
		ICalReader	*tfr = (ICalReader *) new ICalStringReader(mimedata);

		if (tfr)
		{
			imipCal = new NSCalendar();
			if (imipCal) imipCal->parse(tfr, u);
			delete tfr;
		}
	}

	if (imipCal != nil)
	{
		if ((jff = new JulianFormFactory(*imipCal, *this, getCallbacks())) != nil)
		{
			htmlForm.truncate(0);	// Empty it

			if ((jff->getHTML(&htmlForm, Want_Detail)) != nil)
			{
				if ((htmltext = (char *)malloc(htmlForm.size() + 1)) != nil)
					strcpy(htmltext, htmlForm.toCString(""));
			}

			delete jff;
		}

		// Keep the imipCal around for use later;
	}

	return htmltext;
}

void
JulianForm::setMimeData(char *NewMimeData)
{
	if (NewMimeData)
	{
		if (mimedata) free(mimedata);
		mimedata = (char *)malloc(strlen(NewMimeData));
		strcpy(mimedata, NewMimeData);
	}
}

char*
JulianForm::getComment()
{
	int32 x;

	for (x=0; x < formDataCount; x++)
	{
		if (!JulianFormFactory::CommentsFieldName.compare(formData[x].type))
		{
			(*getCallbacks()->PlusToSpace)(formData[x].data);
			(*getCallbacks()->UnEscape)	 (formData[x].data);
			return formData[x].data;
		}
	}

	return nil;
}

void
julian_handle_close(JulianForm *jf)
{
	if (jf->getContext())
	{
		(*jf->getCallbacks()->DestroyWindow)(jf->getContext());
		jf->setContext(nil);
	}
}

void
julian_handle_moredetail(JulianForm *jf)
{
	MWContext*	cx = nil;
    char*		newhtml = NULL;
	NET_StreamClass *stream;
    URL_Struct *url;

	url = (*jf->getCallbacks()->CreateURLStruct)("", NET_DONT_RELOAD);
	(*jf->getCallbacks()->SACopy)(&(url->content_type), TEXT_HTML);

	// Look to see if we already made a window for this
	if (jf->getContext() != nil)
	{
		cx = jf->getContext();
		(*jf->getCallbacks()->RaiseWindow)(cx);
	}

	if (!cx)
	{
		/* make the window */
		jf->setContext(cx = (*jf->getCallbacks()->MakeNewWindow)((*jf->getCallbacks()->FindSomeContext)(), NULL, jf->contextName, NULL));
	}

	if ((newhtml = jf->getHTMLForm(TRUE)) != NULL)
	{
		if (cx)
		{
			int32 ret;

			/* make a netlib stream to display in the window */
			stream = (*jf->getCallbacks()->StreamBuilder)(FO_PRESENT, url, cx);
			// 	ret = (*(stream)->put_block)(stream->data_object, newhtml, strlen(newhtml));
			// (*stream->complete) (stream->data_object);
        }        
    }
}

void
julian_send_response(UnicodeString& subject, UnicodeString& orgName, UnicodeString& LoginName, JulianForm& jf, NSCalendar& calendar)
{
	TransactionObject::ETxnErrorCode txnStatus;
	TransactionObject*	txnObj;
	JulianPtrArray*		recipients = 0;
	JulianPtrArray*		capiModifiers = 0;
	MWContext*			this_context = (*jf.getCallbacks()->FindSomeContext)();

	recipients = new JulianPtrArray();
	capiModifiers = new JulianPtrArray();

	if (capiModifiers)
	{
		if (recipients)
		{
			URI orgUri(orgName);
			URI meUri(LoginName);
			User* uFrom = new User(LoginName, meUri.getName());
			User* uToOrg = new User(orgName, orgUri.getName());
			recipients->Add(uToOrg);
			txnObj = TransactionObjectFactory::Make(calendar, *(calendar.getEvents()),
													*uFrom, *recipients, subject, *capiModifiers,
													&jf, this_context, LoginName);
			if (txnObj != 0)
			{
				txnObj->execute(0, txnStatus);
				delete txnObj; txnObj = 0;
			}

			User::deleteUserVector(recipients);
			delete recipients;
			if (uFrom) delete uFrom;
		}
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
julian_handle_accept(JulianForm *jf, Attendee::STATUS newStatus)
{
	NSCalendar*		imipCal = jf->getCalendar();
	ICalComponent*	component;
    UnicodeString	orgName;
    char*			name = nil;
	UnicodeString	nameU;
	UnicodeString	subject = UnicodeString(jf->getLabel());

     // this should be set to the logged in user
	if (*jf->getCallbacks()->CopyCharPref)
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

		for (int32 i = 0; i < imipCal->getEvents()->GetSize(); i++)
		{
			component = (ICalComponent *) imipCal->getEvents()->GetAt(i);

			if (component->GetType() == ICalComponent::ICAL_COMPONENT_VEVENT ||
				component->GetType() == ICalComponent::ICAL_COMPONENT_VEVENT ||
				component->GetType() == ICalComponent::ICAL_COMPONENT_VEVENT)
			{
				// Set org
				if (orgName.size() == 0) orgName = ((TimeBasedEvent *)component)->getOrganizer();

				// Find out who logged in user is
				((TimeBasedEvent *)component)->setAttendeeStatus(nameU, newStatus);
				if (jf->hasComment())
				{
					((TimeBasedEvent *)component)->setNewComments(jf->getComment());
				}
				((TimeBasedEvent *)component)->stamp();

				// Right now allways use the last Organizer. Should be the same for all
				// events etc.
				orgName = ((TimeBasedEvent *)component)->getOrganizer();

				subject += JulianFormFactory::SubjectSep;
				subject += ((TimeBasedEvent *)component)->getSummary();
			}
		}

		if (firstTime)
		{
			// set method to reply
			imipCal->setMethod(NSCalendar::METHOD_REPLY);
			julian_send_response(subject, orgName, nameU, *jf, *imipCal);
			firstTime = FALSE;
		}
    }

	if (name) XP_FREE(name);
}


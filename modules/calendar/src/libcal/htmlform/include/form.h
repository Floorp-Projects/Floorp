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

#ifndef _JULIAN_FORMS_H
#define _JULIAN_FORMS_H

#include "julianform.h"

typedef struct
{
	char*	type;
	char*	data;
} form_data_combo;

class JULIAN_PUBLIC JulianForm
{
private:

	UnicodeString					htmlForm;
	char*							mimedata;
	char							buttonLabel[2048];
	NSCalendar*						imipCal;
	pJulian_Form_Callback_Struct	JulianForm_CallBacks;
	MWContext*						moredetails_context;

public:
			JulianForm();
	virtual ~JulianForm();

	int32							formDataCount; /* number of pointers in formData */
	/* number of possible pointers in formData */
	#define formDataIndex 10
	form_data_combo					formData[formDataIndex]; /* a pointer to an array of pointers that point to a type/data string */

	char*							contextName;

	char*							getHTMLForm(XP_Bool Want_Detail);
	void							setMimeData(char *mimedata);
	void							setCallbacks(pJulian_Form_Callback_Struct callBacks) { JulianForm_CallBacks = callBacks; };
	pJulian_Form_Callback_Struct	getCallbacks() { return JulianForm_CallBacks; };

    /* John Sun added 4-29-98 */
    NSCalendar * getCalendar()		{ return imipCal; }
    JulianPtrArray * getEvents()	{ if (imipCal) { return imipCal->getEvents(); } else return 0; }

	MWContext*		getContext()	{ return (*JulianForm_CallBacks->FindNamedContextInList)((*JulianForm_CallBacks->FindSomeContext)(), contextName); }
	void			setContext(MWContext* new_context) { moredetails_context = new_context; } 

	XP_Bool			hasComment()	{ return getComment() != nil; } 
	char*			getComment();

	char*			getLabel()		{ return buttonLabel; }
	void			setLabel(char *newlabel) { if (newlabel) XP_STRCPY(buttonLabel, newlabel); (*getCallbacks()->PlusToSpace)(buttonLabel); }
};


#ifdef XP_CPLUSPLUS
extern "C" {
#endif

JulianForm*	jform_CreateNewForm	(char *calendar_mime_data, pJulian_Form_Callback_Struct callbacks);
void		jform_DeleteForm	(JulianForm *jf);
char*		jform_GetForm		(JulianForm *jf);
void		jform_CallBack		(JulianForm *jf, char *type);

#ifdef XP_CPLUSPLUS
    };
#endif

class JULIAN_PUBLIC JulianServerProxy 
{
public:
	ICalComponent*	ic;

					JulianServerProxy() {};
	virtual			~JulianServerProxy() {};
	ICalComponent*	getByUid(char *uid) { return ic; };

	void			setICal(ICalComponent* i) { ic = i; };

};

#endif

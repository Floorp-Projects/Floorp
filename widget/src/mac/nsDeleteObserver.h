/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef DeleteObserver_h__
#define DeleteObserver_h__

class nsVoidArray;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
//
// This is a small utility class which allows to notify an object
// (the "observer") holding a reference on another object 
// (the "observed") that the observed object has been deleted.
//
// As you imagine, it's reserved to cases where XPCOM refCouting doesn't fit.
// It is used by the MacEventHandler and the Toolkit to hold references
// on the focused widget and the the last widget hit.



//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

class nsDeleteObserver
{
public:
		virtual void	NotifyDelete(void* aDeletedObject) = 0;
};

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

class nsDeleteObserved
{
public:
							nsDeleteObserved(void* aObject);
	virtual			~nsDeleteObserved();

	void				AddDeleteObserver(nsDeleteObserver* aDeleteObserver);
	void				RemoveDeleteObserver(nsDeleteObserver* aDeleteObserver);

private:
	nsVoidArray*			mDeleteObserverArray;
	void*							mObject;
};


#endif //DeleteObserver_h__
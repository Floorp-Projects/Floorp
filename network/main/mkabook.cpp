/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
// 
// mkabook.cpp -- Handles "addbook:" " URLs for core Navigator, without
//               requiring libmsg. mkabook is intended for adding 
//               to the address-book.
//
//

#include "mkutils.h" 

#include "xp.h"
#include "xp_str.h"

#include "mkgeturl.h"
#include "mkabook.h"
#include "addrbook.h"

#include "abcom.h"
#include "xpgetstr.h"
#include "pw_public.h"

extern "C" 
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MALFORMED_URL_ERROR;
	extern int MK_ADDR_IMPORT_CARDS;
	extern int MK_ADDR_IMPORT_MAILINGLISTS;
	extern int MK_ADDR_EXPORT_CARDS;
	extern int MK_MSG_ADDRESS_BOOK;
	extern int MK_ADDR_EXPORT_TITLE;
	extern int MK_ADDR_IMPORT_TITLE;
}

typedef enum {
	abImportVCard,
	abImport,
	abExport,
	abCopy   /* copying, deleting, moving address book entries */
} net_abUrlType;

class NET_AddressBookConnData 
{
public:
	NET_AddressBookConnData (ActiveEntry * ce) 
	{
		m_commandData = NULL;
		m_addressBook = NULL;
		m_urlType = abImportVCard;
		m_started = FALSE;
		m_ce = ce;
		m_container = NULL;

		// Netlib magic so we get a timeslice without waiting for a read-ready socket. 
		// It would be nice to ditch this, but the LDAP SDK manages the sockets.
		if (m_ce)
		{
			m_ce->socket = NULL;
			NET_SetCallNetlibAllTheTime(m_ce->window_id, "mkabook");
		}
	}

	virtual ~NET_AddressBookConnData () 
	{
		if (m_container)
		{
			AB_ReleaseContainer(m_container);
			m_container = NULL;
		}

		// Netlib magic so we won't get called anymore for this URL
		// It would be nice to ditch this, but the LDAP SDK manages the sockets.
		NET_ClearCallNetlibAllTheTime(m_ce->window_id, "mkabook");
		m_ce = NULL;
	}

#ifdef MOZ_NEWADDR
	virtual int Interrupt() = 0; 
	virtual int Process() = 0;
	virtual int Load() = 0;
#else
	virtual int Interrupt() {return 0;}
	virtual int Process() { return 0;}
	virtual int Load() {return 0;}
#endif

#ifdef MOZ_NEWADDR
	virtual int Initialize(const char * url) {return 0;}
#else
	XP_Bool Initialize(const char *url) { return FALSE;}

#endif


	void *m_cookie;
	char *m_commandData;
	ABook *m_addressBook;  // only used by old address book APIs....remove when everyone is using the new APIs...
	ActiveEntry *m_ce;
	AB_ContainerInfo * m_container;

	net_abUrlType m_urlType;
	XP_Bool m_started;
};

/**********************************************************************************************
	The connection data class for importing a vcard URL into an address book. This operation is
	actually a synchronous so we shouldn't have to fill in implementations for interrupt...
***********************************************************************************************/
class net_AddressBookAddVCard : public NET_AddressBookConnData
{
public:
	net_AddressBookAddVCard(ActiveEntry * ce);
	virtual ~net_AddressBookAddVCard();

	virtual int Load();
	virtual int Process(); 
	virtual int Initialize(const char * url);
	virtual int Interrupt();
	
protected:
	int LoadContainerToAddTo();
};

net_AddressBookAddVCard::net_AddressBookAddVCard(ActiveEntry *ce) : NET_AddressBookConnData(ce)
{
	m_urlType = abImportVCard;
}

net_AddressBookAddVCard::~net_AddressBookAddVCard()
{}

int net_AddressBookAddVCard::LoadContainerToAddTo()
{
	// acquire list of personal address books, then grab the first one...
	XP_List * containers = AB_AcquireAddressBookContainers(m_ce->window_id);
	if (containers)
	{
		// extract the first one
		int index = 1;
		m_container = NULL;
		while (!m_container && index <= XP_ListCount(containers))
			m_container = (AB_ContainerInfo *) XP_ListGetObjectNum (containers, index++);
		if (m_container)
			AB_AcquireContainer(m_container); // acquire our own ref count

		AB_ReleaseContainersList(containers); // release ref count on all the ctrs
	}

	return 0;
}

int net_AddressBookAddVCard::Load()
{ 
	// load m_container...right now this is just the first address book available...
	return LoadContainerToAddTo();
}

int net_AddressBookAddVCard::Process()
{
	// since this is a synchronous operation, do the import...
	int status = 0;
	if (m_container)
		status = AB_ImportVCards(m_container, m_commandData);

	return MK_CONNECTED;
}

int net_AddressBookAddVCard::Interrupt()
{
	XP_ASSERT(FALSE); // this should not happen because vcard happens synch....
	return 0;
}

int net_AddressBookAddVCard::Initialize(const char * url)
{ 
	char *search = NET_ParseURL (url, GET_SEARCH_PART);
	m_urlType = abImportVCard;
	if (search)
	{
		m_commandData = XP_STRDUP (search + 7);
		NET_UnEscape (m_commandData);
	}
	XP_FREEIF(search);
	return 0;
}

/**********************************************************************************************
	All of our asynch operations in the new address book (export and import right now), use
	progress windows. We use this class to pull out the progress window commonality between
	export and import. net_AddressBookAsynchData handles creation and destruction of the 
	progress panes. We use virtual methods to determine the appropriate title and description
	text for the progress windows
************************************************************************************************/

// call back function for canceling asynch operations...
static void CancelAsynch(void *closure)
{
	MWContext *progressContext = (MWContext *)closure;
	if (progressContext)
		XP_InterruptContext(progressContext);
}

class net_AddressBookAsynchData : public NET_AddressBookConnData
{
public:
	net_AddressBookAsynchData(ActiveEntry * ce);
	virtual ~net_AddressBookAsynchData();

	// all address book connect data classes support Load, Process, Initialize and Interrupt
	// as these 4 methods are called by the NET_ functions in mkabook.h
	virtual int Load(); // creates progress windows...

	virtual int Process() = 0; 
	virtual int Initialize(const char * url) = 0;
	virtual int Interrupt() = 0;

	// string functions used to build the progress window
	virtual char * GetPWTitle() { return NULL;}
	virtual char * GetPWLine1Text() { return NULL;} // caller must free this string...
	virtual char * GetPWLine2Text() { return NULL;} // caller must free this string..
	
protected:
	// Some asynch urls use progress windows....
	MWContext *m_progressContext;
	pw_ptr	   m_progressWindow;
};

net_AddressBookAsynchData::net_AddressBookAsynchData(ActiveEntry * ce) : NET_AddressBookConnData(ce)
{
	m_progressContext = NULL;
	m_progressWindow = NULL;
}

net_AddressBookAsynchData::~net_AddressBookAsynchData()
{
	if (m_progressWindow)
	{
		PW_Hide(m_progressWindow);
		PW_Destroy(m_progressWindow);
	}

	if (m_progressContext)
		PW_DestroyProgressContext(m_progressContext);
}

int net_AddressBookAsynchData::Load()
{
	int status = 0;
	// get m_container from the url struct...
	if (m_ce && m_ce->URL_s)
		m_container = (AB_ContainerInfo *)m_ce->URL_s->owner_data; // it was already acquired when it was inserted into the url_struct...

	if (m_container)
	{
		m_progressContext = PW_CreateProgressContext();
		if (m_progressContext)
		{
			m_progressWindow = PW_Create(NULL, pwStandard);
			if (m_progressWindow)
			{
				PW_AssociateWindowWithContext(m_progressContext, m_progressWindow);
				char * title = GetPWTitle();
				char * line1Text = GetPWLine1Text();
				char * line2Text = GetPWLine2Text();

				// it is okay to set a NULL title or line...
				PW_SetWindowTitle(m_progressWindow, title);  // resource string
				PW_SetLine1(m_progressWindow, line1Text);// do we want anything here?
				PW_SetLine2(m_progressWindow, line2Text); // mscott: again...add a real resource string for this
				PW_SetProgressRange(m_progressWindow,0,100);
				PW_SetCancelCallback(m_progressWindow, CancelAsynch, m_ce->window_id);
				PW_Show(m_progressWindow);
				
				// free any allocated strings
				XP_FREEIF(line1Text);
				XP_FREEIF(line2Text);
				XP_FREEIF(title);
			}
			else
				PW_DestroyProgressContext(m_progressContext);
		} // if context
	}  // if container

	return status;		
}

/**********************************************************************************************
	The connection data class for copying/moving entries between address books. This operation is
	asynch. We assume that the AB_AddressCopyInfo struct for the transfer is stored in the owner_data
	field of the url_struct. In addition, status information will be shown through the current event's
	context. 
***********************************************************************************************/
class net_AddressBookCopy : public net_AddressBookAsynchData
{
public:
	net_AddressBookCopy(ActiveEntry * ce);
	virtual ~net_AddressBookCopy();

	virtual int Load();
	virtual int Process(); 
	virtual int Initialize(const char * url);
	virtual int Interrupt();
	
protected:
	// any state we may ned to store...
	AB_AddressBookCopyInfo * m_abCopyInfo;
};

net_AddressBookCopy::net_AddressBookCopy(ActiveEntry * ce) : net_AddressBookAsynchData(ce)
{
	if (ce && ce->URL_s)
		m_abCopyInfo = (AB_AddressBookCopyInfo *) ce->URL_s->owner_data;
	else
		m_abCopyInfo = NULL;
}

net_AddressBookCopy::~net_AddressBookCopy()
{
	if (m_abCopyInfo)
		XP_ASSERT(FALSE);  // struct should have been released by finish or interrupt calls...
}

int net_AddressBookCopy::Load()  // over ride this from base class so we don't create the progress window...
{
	return 0;
}

int net_AddressBookCopy::Initialize(const char * /* url */)
{
	// we don't have any command data for address book copy urls....
	m_urlType = abCopy;
	return 0;
}

int net_AddressBookCopy::Process()
{
	int status = 0;
	XP_Bool copyFinished = FALSE;
	if (m_abCopyInfo)
	{
		if (!m_started) // if we haven't begun the copy...use the begin APIs...
			status = AB_BeginEntryCopy (m_abCopyInfo->srcContainer, m_ce->window_id, m_abCopyInfo, &m_cookie, &copyFinished);
		else // otherwise do more...
			status = AB_MoreEntryCopy (m_abCopyInfo->srcContainer, m_ce->window_id, m_abCopyInfo, &m_cookie, &copyFinished);

		if (status != AB_SUCCESS) // then mark ourselves as finished!
		{
			XP_ASSERT(FALSE); // what condition failed to cause us to be here?? 
			copyFinished = TRUE;
		}

		// all progress is handled by the src container
	
		if (copyFinished)  // if we are finished,
		{
			AB_FinishEntryCopy(m_abCopyInfo->srcContainer, m_ce->window_id, m_abCopyInfo, &m_cookie); // clean up everything..
			m_abCopyInfo = NULL;
			status = MK_CONNECTED;
		}
		else
			status = MK_WAITING_FOR_CONNECTION;
	}
	else
		status = MK_CONNECTED; // this copy is done...we didn't have a copy info struct!!!!

	return status;
}

int net_AddressBookCopy::Interrupt()
{
	// we are shutting down...
	if (m_abCopyInfo)
		AB_InterruptEntryCopy(m_abCopyInfo->srcContainer, m_ce->window_id, m_abCopyInfo, &m_cookie); 
	
	m_abCopyInfo = NULL;
	return 0;
}

/**********************************************************************************************
	The connection data class for import. Importing in the new address book is asynch. The 
	file to import is contained in the URL. However, the particular address book to import into
	is stored in the owner_data field of the URL struct which is in the active entry. When the
	URL_struct is created, the container was reference counted. So when you are done importing, 
	or if importing is canceled, we are now responsible for releasing the reference count
	on the container.

	m_cookie is actually a struct which represents the import state for the container. When the
	import is finished or interrupted, the container will free this cookie struct...
************************************************************************************************/


class net_AddressBookImport : public net_AddressBookAsynchData
{
public:
	net_AddressBookImport(ActiveEntry * ce);
	virtual ~net_AddressBookImport();

	// leave it to the subclass to implement these..
	virtual int Process();
	virtual int Initialize(const char * url);
	virtual int Interrupt();
	
	int			UpdateProgress();
	
	// string functions used to build the progress window
	virtual char * GetPWTitle(); 
	virtual char * GetPWLine1Text(); // caller must free this string...
	virtual char * GetPWLine2Text(); // caller must free this string..
	
protected:
	// import in the new address book is asynch..we need a progress window to show it off....
	uint32 m_passCount; // importing LDIF requires 2 passes....we want to keep track of which pass we are on for progress purposes...
};

net_AddressBookImport::net_AddressBookImport(ActiveEntry * ce) : net_AddressBookAsynchData(ce)
{
	m_passCount = 1; // always start with first pass...
}

net_AddressBookImport::~net_AddressBookImport()
{
	if (m_cookie)
		XP_ASSERT(FALSE); // should have been freed and cleared by container import code....

	// container is released by base class...
}

int net_AddressBookImport::Initialize(const char * url)
{
	char * search = NET_ParseURL(url, GET_SEARCH_PART);

	m_urlType = abImport;
	m_commandData = XP_STRDUP(search+6);
	NET_UnEscape(m_commandData);
	return 0;
}

char * net_AddressBookImport::GetPWTitle()
{
	return XP_STRDUP(XP_GetString(MK_ADDR_IMPORT_TITLE));
}

char * net_AddressBookImport::GetPWLine1Text()
{
	return NULL;  // no line 1 text for import pane
}

char * net_AddressBookImport::GetPWLine2Text()
{
	AB_ContainerAttribValue * attrib = NULL;
	AB_GetContainerAttribute(m_container, attribName, &attrib);

	char * lineText = NULL;
	if (m_passCount <= 1)
	{
		if (attrib->u.string)
			lineText = PR_smprintf (XP_GetString(MK_ADDR_IMPORT_CARDS), attrib->u.string);
		else
			lineText = PR_smprintf (XP_GetString(MK_ADDR_IMPORT_CARDS),XP_GetString(MK_MSG_ADDRESS_BOOK)); 
	}
	else
	{
		if (attrib->u.string)
			lineText = PR_smprintf (XP_GetString(MK_ADDR_IMPORT_MAILINGLISTS), attrib->u.string);
		else
			lineText = PR_smprintf (XP_GetString(MK_ADDR_IMPORT_MAILINGLISTS), XP_GetString(MK_MSG_ADDRESS_BOOK)); 
	}
	
	AB_FreeContainerAttribValue(attrib);

	return lineText;
}


int net_AddressBookImport::Process()
{
	int status = 0;
	XP_Bool importFinished;
	if (!m_started) // if we haven't begun the import...use the begin APIs...
		status = AB_ImportBegin (m_container, NULL, m_commandData /* file name */, &m_cookie /* import state */, &importFinished);
	else // otherwise do more...
		status = AB_ImportMore (m_container, &m_cookie, &importFinished);

	UpdateProgress();

	// are we done?
	if (importFinished)  // if we are finished,
	{
		AB_ImportFinish(m_container, &m_cookie);
		status = MK_CONNECTED;
	}
	else
		status = MK_WAITING_FOR_CONNECTION;

	return status;
}

int net_AddressBookImport::UpdateProgress ()
{
	// now update progress bar...
	uint32 position = 0; // position in the import file
	uint32 fileLength = 0; 
	uint32 passCount = m_passCount;
	int status = AB_ImportProgress(m_container, m_cookie, &position, &fileLength, &passCount);
	if (passCount != m_passCount)
	{
		m_passCount = passCount;
		char * text = GetPWLine1Text();
		PW_SetLine1(m_progressWindow, text); // mscott: again...add a real resource string for this
		XP_FREEIF(text);
	}

	if (fileLength)
	{
		int32 percent = (position * 100) / fileLength;
		PW_SetProgressValue(m_progressWindow, percent);
	}
	return 0;
}

int net_AddressBookImport::Interrupt()
{
	// we are shutting down...
	AB_ImportInterrupt(m_container, &m_cookie); // destroys the container...
	m_cookie = NULL;
	// destruction will clean up the progress pane stuff...
	return 0;
}


/**********************************************************************************************
	The connection data class for export. Exporting in the new address book is asynch. The 
	file to export to is contained in the URL. However, the particular address book to export from
	is stored in the owner_data field of the URL struct which is in the active entry. When the
	URL_struct is created, the container was reference counted. So when you are done exporting, 
	or if export is canceled, we are responsible for releasing the reference count
	on the container.

	m_cookie is actually a struct which represents the export state for the container. When the
	export is finished or interrupted, the container will free this cookie struct...
************************************************************************************************/


class net_AddressBookExport : public net_AddressBookAsynchData
{
public:
	net_AddressBookExport(ActiveEntry * ce);
	virtual ~net_AddressBookExport();

	virtual int Process(); 
	virtual int Initialize(const char * url);
	virtual int Interrupt();

	int			UpdateProgress();

	// string functions used to build the progress window
	virtual char * GetPWTitle(); 
	virtual char * GetPWLine1Text(); // caller must free this string...
	virtual char * GetPWLine2Text(); // caller must free this string..
	
protected:
};

net_AddressBookExport::net_AddressBookExport(ActiveEntry * ce) : net_AddressBookAsynchData(ce)
{}

net_AddressBookExport::~net_AddressBookExport()
{
	if (m_cookie)
		XP_ASSERT(FALSE); // should have been freed and cleared by container import code....

	// container is released by base class...
	// progress window is destroyed by base class..
}

int net_AddressBookExport::Initialize(const char * url)
{
	char * search = NET_ParseURL(url, GET_SEARCH_PART);
	m_urlType = abExport;
	m_commandData = XP_STRDUP(search+6); // + 6 to skip over the "?file=" part
	NET_UnEscape(m_commandData);
	return 0;
}

char * net_AddressBookExport::GetPWTitle()
{
	return XP_STRDUP(XP_GetString(MK_ADDR_EXPORT_TITLE));
}

char * net_AddressBookExport::GetPWLine1Text()
{
	return NULL;  // no line 1 text for import pane
}

char * net_AddressBookExport::GetPWLine2Text()
{
	AB_ContainerAttribValue * attrib = NULL;
	AB_GetContainerAttribute(m_container, attribName, &attrib);

	char * lineText = NULL;
	if (attrib->u.string)
		lineText = PR_smprintf (XP_GetString(MK_ADDR_EXPORT_CARDS), attrib->u.string);
	else
		lineText = PR_smprintf (XP_GetString(MK_ADDR_EXPORT_CARDS),XP_GetString(MK_MSG_ADDRESS_BOOK)); 	
	AB_FreeContainerAttribValue(attrib);

	return lineText;
}

int net_AddressBookExport::Process()
{
	int status = 0;
	XP_Bool exportFinished;
	if (!m_started) // if we haven't begun the import...use the begin APIs...
		status = AB_ExportBegin (m_container, NULL, m_commandData /* file name */, &m_cookie /* import state */, &exportFinished);
	else // otherwise do more...
		status = AB_ExportMore (m_container, &m_cookie, &exportFinished);

	UpdateProgress();
	
	// are we done?
	if (exportFinished)  // if we are finished,
	{
		AB_ExportFinish(m_container, &m_cookie);
		status = MK_CONNECTED;
	}
	else
		status = MK_WAITING_FOR_CONNECTION;

	return status;
}

int net_AddressBookExport::UpdateProgress ()
{
	// now update progress bar...
	uint32 numberExported = 0;
	uint32 totalEntries = 0; 

	int status = AB_ExportProgress(m_container, m_cookie, &numberExported, &totalEntries);

	if (totalEntries)
	{
		int32 percent = (numberExported * 100) / totalEntries;
		PW_SetProgressValue(m_progressWindow, percent);
	}
	return 0;
}

int net_AddressBookExport::Interrupt()
{
	// we are shutting down...
	AB_ExportInterrupt(m_container, &m_cookie); // destroys the container...
	m_cookie = NULL;
	// destruction will clean up the progress pane stuff...
	return 0;
}

//
// Callbacks from NET_GetURL 
//
#ifdef MOZ_NEWADDR

extern "C" int32 net_ProcessAddressBook (ActiveEntry *ce)
{
	int status = 0;
	NET_AddressBookConnData *cd = (NET_AddressBookConnData*) ce->con_data;
	if (cd)
	{
		status = cd->Process(); // should return MK_WAITING_FOR_CONNECTION
		cd->m_started = TRUE;
		if (status < 0 || status == MK_CONNECTED)
		{
			delete cd;
			ce->con_data = NULL;
		}
	}

	if(status == MK_CONNECTED) // i don't understand why mkgeturl won't end the url on MK_CONNECTED, but it doesn't...
		status = -1;

	ce->status = status;
	return ce->status;
}

extern "C" int32 net_AddressBookLoad (ActiveEntry *ce)
{
	ce->status = -1;
	int status = ce->status;
	// examine URL to determine what type of connection data we need to create...
	char * url = ce->URL_s->address;
	char *path = NET_ParseURL (url, GET_PATH_PART);
	char * search = NET_ParseURL(url, GET_SEARCH_PART);
		NET_AddressBookConnData *cd = NULL;
	switch (path[0])
	{
		case 'a':
		case 'A':
			if (!XP_STRNCASECMP(path,"add",3)) 
			{
				if (!XP_STRNCASECMP (search, "?vcard=", 7)) 
					cd = new net_AddressBookAddVCard(ce);
				if (!cd)
					status = MK_OUT_OF_MEMORY;
			}
			else
				XP_ASSERT(FALSE);
			break;
		case 'i':
		case 'I':
			if (!XP_STRNCASECMP (path, "import", 6))
			{
				if (!XP_STRNCASECMP (search, "?file=", 6))
				{
					cd = new net_AddressBookImport(ce);
					if (!cd)
						status = MK_OUT_OF_MEMORY;
				} //if ?file=...
			}
			break;
		case 'e':
		case 'E':
			if (!XP_STRNCASECMP (path, "export", 6))
			{
				if (!XP_STRNCASECMP (search, "?file=", 6))
				{
					cd = new net_AddressBookExport(ce);
					if (!cd)
						status = MK_OUT_OF_MEMORY;
				} //if ?file=...
			}
			break;
		case 'c':
		case 'C':
			if (!XP_STRNCASECMP(path, "copy", 4))
			{
				// we have no search part on copy urls....
				cd = new net_AddressBookCopy(ce);
				if (!cd)
					status = MK_OUT_OF_MEMORY;
			}
			break;
		default: 
			status = MK_MALFORMED_URL_ERROR;
			XP_ASSERT(FALSE);

	}

	ce->status = status; // catch any errors like MK_OUT_OF_MEMORY...
	if (cd)
	{
		cd->Initialize(url);
		cd->Load();
		ce->con_data = cd;   // store the connection data
		ce->status = net_ProcessAddressBook (ce);
	}
	
	XP_FREEIF(path);
	XP_FREEIF(search);
	return ce->status;
}

extern "C" int32 net_InterruptAddressBook (ActiveEntry * ce)
{
	ce->status = MK_INTERRUPTED;
	NET_AddressBookConnData *cd = (NET_AddressBookConnData*) ce->con_data;

	if (cd)
	{
		cd->Interrupt();
		delete cd;
	}

	return ce->status;
}
extern "C" void net_CleanupAddressBook(void)
{
	// I'm not sure what cleanup stuff I have to do here...
	// leaving it empty for now...
}

#else
extern "C" int32 net_ProcessAddressBook (ActiveEntry *ce)
{
	NET_AddressBookConnData *cd = (NET_AddressBookConnData*) ce->con_data;
	switch (cd->m_urlType)
	{
	case abImportVCard:
		AB_ImportFromVcardURL(cd->m_addressBook, ce->window_id, cd->m_commandData); //mscott
		ce->status = -1; //phil/mscott should work, but seems not to. MK_CONNECTED;
		break;
	case abImport:
		if (!cd->m_started)
			ce->status = MK_CONNECTED; //mscott AB_ImportBegin (cd->m_addressBook, ce->window_id, cd->m_commandData, &cd->m_cookie);
		else
			ce->status = MK_CONNECTED; //mscott AB_ImportMore (cd->m_addressBook, ce->window_id, cd->m_cookie);
		break;
	}

	cd->m_started = TRUE;

	// Either an error, or we're already done
	if (ce->status != MK_WAITING_FOR_CONNECTION)
		delete cd;

	return ce->status;
	return -1;
}

extern "C" int32 net_AddressBookLoad (ActiveEntry *ce)
{
	ce->status = -1;

	NET_AddressBookConnData *cd = new NET_AddressBookConnData(ce);
	if (!cd)
		return MK_OUT_OF_MEMORY;
	
	ce->con_data = cd;

	if (cd->Initialize (ce->URL_s->address))
		ce->status = net_ProcessAddressBook(ce);

	return ce->status;
}

extern "C" void
net_CleanupAddressBook(void)
{}

extern "C" int32 net_InterruptAddressBook (ActiveEntry * ce)
{
	ce->status = MK_INTERRUPTED;
	NET_AddressBookConnData *cd = (NET_AddressBookConnData*) ce->con_data;

	switch (cd->m_urlType)
	{
	case abImportVCard:
		XP_ASSERT(FALSE); // this shouldn't happen because vCard import happens synchronously
		break;
	case abImport:
		//mscott AB_ImportInterrupt (cd->m_addressBook, ce->window_id, cd->m_cookie);
		break;
	default:
		XP_ASSERT(FALSE);
	}

	delete cd;

	return ce->status;
}
#endif

MODULE_PRIVATE void
NET_InitAddressBookProtocol(void)
{
    static NET_ProtoImpl abook_proto_impl;

    abook_proto_impl.init = net_AddressBookLoad;
    abook_proto_impl.process = net_ProcessAddressBook;
    abook_proto_impl.interrupt = net_InterruptAddressBook;
    abook_proto_impl.cleanup = net_CleanupAddressBook;

    NET_RegisterProtocolImplementation(&abook_proto_impl, ADDRESS_BOOK_TYPE_URL);
}

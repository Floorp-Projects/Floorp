/*
	mac_xpidl_panel.cpp
 */

#define CW_STRICT_DIALOGS 1

/* standard headers */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* system headers */
#include <AERegistry.h>
#include <Drag.h>
#include <Palettes.h>
#include <Resources.h>
#include <Scrap.h>
#include <TextUtils.h>
#include <Sound.h>

/* compiler headers */
#include <A4Stuff.h>
#include <SetUpA4.h>
#include <DropInPanel.h>

/* project headers */
#include "mac_xpidl_panel.h"

/* current version number for our prefs data */
#define PSAMPLEPANELVERSION		3


enum {
	kFactoryPrefsID = 128,
	kCW7ItemListID = 128,
	kCW8ItemListID = 129,

	kXPIDLModeItem = 1,
	kXPIDLWarningsItem,
	kXPIDLVerboseItem,

	kXPTLinkerOutputItem = 4,
	
	kLinkSymItem = 1,
	kProjTypeItem,
	kOutFileItem,
	kOutFileLabelItem,
	kPictLabelItem,
	kPictItem,
	kBoxItem,
	
	kSampleStringsID = 128,
	kSampleInfoStr = 1
};


/* local variables */
static Boolean		sPictActive;
static Point		sDotPosition;
static RgnHandle	sDragRgn;
static Boolean		sHighlightOn;


/* prototypes of local functions */
static void		PanelDrawBox(PanelParameterBlock *pb, long whichItem, short strIndex);
static void		DrawDot(Point dotPos, const Rect* itemRect);
static void		DrawPictBox(PanelParameterBlock *pb);
static void		ActivatePictBox(PanelParameterBlock *pb, Boolean activateIt);
static short	InitDialog(PanelParameterBlock *pb);
static void		TermDialog(PanelParameterBlock *pb);
static void		PutData(PanelParameterBlock *pb, Handle options);
static short	GetData(PanelParameterBlock *pb, Handle options, Boolean noisy);
static void		ByteSwapData(XPIDLSettingsHandle options);
static short	Filter(PanelParameterBlock *pb, EventRecord *event, short *itemHit);
static void		HandleKey(PanelParameterBlock *pb, EventRecord *event);
static void		HandleClick(PanelParameterBlock *pb, EventRecord *event);
static void		ItemHit(PanelParameterBlock *pb);
static void		FindStatus(PanelParameterBlock *pb);
static void		ObeyCommand(PanelParameterBlock *pb);
static void		Validate(Handle original, Handle current, Boolean *recompile, Boolean *relink, Boolean *reset);
static short	GetPref(AEKeyword keyword, AEDesc *prefsDesc, Handle settings);
static short	SetPref(AEKeyword keyword, const AEDesc *prefsDesc, Handle settings);
static short	GetFactory(Handle settings);
static short	UpdatePref(Handle settings);
static Boolean	ComparePrefs(Handle prefsHand1, Handle prefsHand2);
static Boolean	ComparePrefs(XPIDLSettings& prefs1, XPIDLSettings& prefs2);
static void		OutlineRect(const Rect* focusRect, Boolean outlineOn);
static OSErr	DragEnter(PanelParameterBlock *pb);
static void		DragWithin(PanelParameterBlock *pb);
static void		DragExit(PanelParameterBlock *pb);
static void		DragDrop(PanelParameterBlock *pb);
static pascal void	PanelDrawBoxCB(DialogPtr dp, short item);

extern "C" {

pascal short	xpidl_panel(PanelParameterBlock *pb);

}

/*
 *	main	-	entry-point for Drop-In Preferences Panel
 *
 */
 
pascal short xpidl_panel(PanelParameterBlock *pb)
{
	short	result, theItem;
	
	EnterCodeResource();
	PrepareCallback();
	
	result = noErr;
			
	switch (pb->request)
	{
	case reqInitPanel:
		/* panel has just been loaded into memory */
		break;

	case reqTermPanel:
		/* panel is about to be unloaded from memory */
		break;

	case reqFirstLoad:
		/* first time panel was loaded. */
		break;

	case reqInitDialog:
		/* hook our dialog item list into the preferences dialog */
		result = InitDialog(pb);
		break;
	
	case reqTermDialog:
		/* unhook our dialog item list from the preferences dialog */
		TermDialog(pb);
		break;
	
	case reqPutData:
		/* put the data in the given handle into our dialog items */
		PutData(pb, pb->currentPrefs);
		break;

	case reqGetData:
		/* fill in the given handle with our dialog items */
		result = GetData(pb, pb->currentPrefs, true);
		break;

	case reqByteSwapData:
		/* byte swap the data in the handle */
		ByteSwapData((XPIDLSettingsHandle)pb->currentPrefs);
		break;
		
	case reqFilter:
		/* filter an event in the dialog */
		result = Filter(pb, pb->event, &pb->itemHit);
		break;
		
	case reqItemHit:
		/* handle a hit on one of our dialog items */
		ItemHit(pb);
		break;
		
	case reqDrawCustomItem:
		/* handle a request to draw one of our user items (CW/8 and later) */
		theItem = pb->itemHit - pb->baseItems;
		switch (theItem)
		{
		case kBoxItem:
			PanelDrawBox(pb, theItem, kSampleInfoStr);
			break;
			
		case kPictItem:
			DrawPictBox(pb);
			break;
		}
		break;
		
	case reqActivateItem:
		theItem = pb->itemHit - pb->baseItems;
		if (theItem == kPictItem)
		{
			ActivatePictBox(pb, true);
		}
		break;
		
	case reqDeactivateItem:
		theItem = pb->itemHit - pb->baseItems;
		if (theItem == kPictItem)
		{
			ActivatePictBox(pb, false);
		}
		break;
		
	case reqHandleKey:
		theItem = pb->itemHit - pb->baseItems;
		if (theItem == kPictItem)
		{
			HandleKey(pb, pb->event);
		}
		break;
		
	case reqHandleClick:
		theItem = pb->itemHit - pb->baseItems;
		if (theItem == kPictItem)
		{
			HandleClick(pb, pb->event);
		}
		break;
		
	case reqFindStatus:
		FindStatus(pb);
		break;
		
	case reqObeyCommand:
		ObeyCommand(pb);
		break;
		
	case reqAEGetPref:
		/* return one item in the given handle as an Apple Event descriptor */
		result = GetPref(pb->prefsKeyword, &pb->prefsDesc, pb->currentPrefs);
		break;

	case reqAESetPref:
		/* change one item in the given handle according to the given Apple Event descriptor */
		result = SetPref(pb->prefsKeyword, &pb->prefsDesc, pb->currentPrefs);
		break;

	case reqValidate:
		/* determine if we need to reset paths, recompile, or relink */
		Validate(pb->originalPrefs, pb->currentPrefs, &pb->recompile, &pb->relink, &pb->reset);
		break;

	case reqGetFactory:
		/* return our factory settings */
		result = GetFactory(pb->factoryPrefs);
		break;

	case reqUpdatePref:
		/* update the given handle to use the current format for our prefs data */
		result = UpdatePref(pb->currentPrefs);
		break;
		
	case reqDragEnter:
		/* determine if we can accept the drag and, if so, start tracking */
		result = DragEnter(pb);
		break;
	
	case reqDragWithin:
		/* continue tracking */
		DragWithin(pb);
		break;
	
	case reqDragExit:
		/* stop tracking */
		DragExit(pb);
		break;
	
	case reqDragDrop:
		/* the user has dropped in our panel */
		DragDrop(pb);
		break;
	
	default:
		result = paramErr;
		break;
	}
	
	ExitCodeResource();
	
	return (result);
}


/*
 *	PanelDrawBoxCB  -	user item proc to draw box
 *
 */
 
static pascal void PanelDrawBoxCB(DialogPtr dp, short item)
{
	Str255	str;
	
	EnterCallback();
	GetIndString(str, kSampleStringsID, kSampleInfoStr);
	CWPanlDrawUserItemBox(dp, item, str);
	ExitCallback();
}


/*
 *	PanelDrawBox
 *
 */
 
static void PanelDrawBox(PanelParameterBlock *pb, long whichItem, short strIndex)
{
	Str255	str;
	
	GetIndString(str, kSampleStringsID, strIndex);
	CWPanlDrawPanelBox(pb, whichItem, str);
}


/*
 *	DrawDot
 *
 */
 
static void DrawDot(Point dotPos, const Rect* itemRect)
{
	Rect		frameRect, dotRect;
	RgnHandle	savedClip;
	
	frameRect = *itemRect;
	SetRect(&dotRect, dotPos.h, dotPos.v, dotPos.h, dotPos.v);
	InsetRect(&dotRect, -6, -6);
	OffsetRect(&dotRect, frameRect.left, frameRect.top);
	
	savedClip = NewRgn();
	GetClip(savedClip);
	FrameRect(&frameRect);
	InsetRect(&frameRect, 1, 1);
	ClipRect(&frameRect);
	EraseRect(&frameRect);
	PaintOval(&dotRect);
	SetClip(savedClip);
}


/*
 *	PanelDrawDotCB  -	user item proc to draw dot
 *
 */
 
static pascal void PanelDrawDotCB(DialogPtr dp, short item)
{
	short		itemType;
	Handle		itemHand;
	Rect		itemRect;
	GrafPtr		savedPort;
	PenState	savedPen;
	
	EnterCallback();
	GetDialogItem(dp, item, &itemType, &itemHand, &itemRect);
	GetPort(&savedPort);
	SetPort(dp);
	GetPenState(&savedPen);
	PenNormal();
	DrawDot(sDotPosition, &itemRect);
	SetPenState(&savedPen);
	SetPort(savedPort);
	ExitCallback();
}


/*
 *	DrawPictBox
 *
 */
 
static void DrawPictBox(PanelParameterBlock *pb)
{
	Rect	itemRect;
	
	CWPanlGetItemRect(pb, kPictItem, &itemRect);
	OutlineRect(&itemRect, sPictActive);
	InsetRect(&itemRect, 3, 3);
	DrawDot(sDotPosition, &itemRect);
}


/*
 *	ActivatePictBox
 *
 */
 
static void ActivatePictBox(PanelParameterBlock *pb, Boolean activateIt)
{
	Rect	itemRect;
	
	sPictActive = activateIt;
	CWPanlGetItemRect(pb, kPictItem, &itemRect);
	OutlineRect(&itemRect, sPictActive);
}
 
/*
 *	InitDialog  -	initialize Dialog Box items for this panel
 *
 */

static short InitDialog(PanelParameterBlock *pb)
{
	short	ditlID;
	OSErr	err;
	
	// The library function will call the IDE to append the dialog items 
	// if possible;  else it will call AppendDITL itself.  This way, you 
	// don't have to worry about it.
	
	if (pb->version < DROPINPANELAPIVERSION_2)
		ditlID = kCW7ItemListID;
	else
		ditlID = kCW8ItemListID;
	
	err = CWPanlAppendItems(pb, ditlID);
	if (err != noErr)
		return (err);
	
	sPictActive = false;
	
	if (pb->version < DROPINPANELAPIVERSION_2) {
		// CW/7 API -- we have to do install user item procs ourselves
		
		CWPanlInstallUserItem(pb, kBoxItem, PanelDrawBoxCB);
		CWPanlInstallUserItem(pb, kPictItem, PanelDrawDotCB);
	}
	
	sDragRgn = NewRgn();
	
	return (err);
}

/*
 *	TermDialog	-	destroy Dialog Box items for this panel
 *
 */

static void TermDialog(PanelParameterBlock *pb)
{
	if (pb->version < DROPINPANELAPIVERSION_2) {
		// CW/7 API -- we have to release the memory we allocated in InitDialog
		
		CWPanlRemoveUserItem(pb, kBoxItem);
		CWPanlRemoveUserItem(pb, kPictItem);
	}
	
	DisposeRgn(sDragRgn);
}

/*
 *	PutData		-	copy the options data from the handle to the screen
 *
 */

static void PutData(PanelParameterBlock *pb, Handle options)
{
	// make sure the options are the right size.
	UpdatePref(options);
	
	XPIDLSettings prefsData = **(XPIDLSettingsHandle) options;

	CWPanlSetItemValue(pb, kXPIDLModeItem, prefsData.mode);
	CWPanlSetItemValue(pb, kXPIDLWarningsItem, prefsData.warnings);
	CWPanlSetItemValue(pb, kXPIDLVerboseItem, prefsData.verbose);

	CWPanlEnableItem(pb, kXPTLinkerOutputItem, prefsData.mode == kXPIDLModeTypelib);
	CWPanlSetItemText(pb, kXPTLinkerOutputItem, prefsData.output);
}

/*
 *	GetData		-	copy the options data from screen to the handle
 *
 */

static short GetData(PanelParameterBlock *pb, Handle options, Boolean noisy)
{
	XPIDLSettings prefsData	= **(XPIDLSettingsHandle) options;
	long mode, warnings, verbose;
	
	CWPanlGetItemValue(pb, kXPIDLModeItem, &mode);
	CWPanlGetItemValue(pb, kXPIDLWarningsItem, &warnings);
	CWPanlGetItemValue(pb, kXPIDLVerboseItem, &verbose);
	
	prefsData.mode = (short) mode;
	prefsData.warnings = (Boolean) warnings;
	prefsData.verbose = (Boolean) verbose;
	
	CWPanlGetItemText(pb, kXPTLinkerOutputItem, prefsData.output, sizeof(Str32));
	
	** (XPIDLSettingsHandle) options = prefsData;
	
	return (noErr);
}

static void ByteSwapShort(short* x)
{
	union {
		short	s;
		char	c[2];
	}	from,to;

	from.s=*x;
	to.c[0]=from.c[1];
	to.c[1]=from.c[0];
	*x = to.s;
}

/*
 *	ByteSwapData		-	byte-swap the options data
 *
 */

static void ByteSwapData(XPIDLSettingsHandle options)
{
	ByteSwapShort(&(**options).version);
	ByteSwapShort(&(**options).mode);
}

/*
 *	Filter		-	filter an event for the Preferences panel
 *
 */
static short Filter(PanelParameterBlock *pb, EventRecord *event, short *itemHit)
{
#pragma unused(pb, event, itemHit)
	
	return (noErr);
}

/*
 *	HandleKey
 *
 */
static void HandleKey(PanelParameterBlock *pb, EventRecord *event)
{
	Point	tempPos			= sDotPosition;
	short	simulatedItem	= kPictItem;
	Rect	itemRect, dotRect;
	
	CWPanlGetItemRect(pb, kPictItem, &itemRect);
	dotRect = itemRect;
	InsetRect(&itemRect, 4, 4);
	OffsetRect(&itemRect, -itemRect.left, -itemRect.top);
	
	switch (event->message & charCodeMask)
	{
	case 0x1C:	// left arrow
		if (tempPos.h-- <= itemRect.left)
			return;
		break;
		
	case 0x1D:	// right arrow
		if (tempPos.h++ > itemRect.right)
			return;
		break;
		
	case 0x1E:	// up arrow
		if (tempPos.v-- <= itemRect.top)
			return;
		break;
		
	case 0x1F:	// down arrow
		if (tempPos.v++ > itemRect.bottom)
			return;
		break;
		
	default:
		return;
	}
	
	sDotPosition = tempPos;
	InsetRect(&dotRect, 3, 3);
	DrawDot(sDotPosition, &dotRect);
	
	pb->itemHit = simulatedItem + pb->baseItems;
}

/*
 *	HandleClick
 *
 */
static void HandleClick(PanelParameterBlock *pb, EventRecord *event)
{
	short	simulatedItem	= kPictItem;
	Rect	itemRect, dotRect;
	GrafPtr	savedPort, panelPort;
	Point	oldPt, newPt;
	
	CWPanlGetMacPort(pb, &panelPort);
	GetPort(&savedPort);
	SetPort(panelPort);
	
	CWPanlGetItemRect(pb, kPictItem, &itemRect);
	InsetRect(&itemRect, 3, 3);
	dotRect = itemRect;
	InsetRect(&itemRect, 1, 1);
	
	oldPt = event->where;
	GlobalToLocal(&oldPt);
	
	if (PtInRect(oldPt, &itemRect))
	{
		newPt.h = newPt.v = -32000;
		
		while (StillDown())
		{
			long	pinned;
			
			if (!EqualPt(oldPt, newPt))
			{
				sDotPosition.h = newPt.h - itemRect.left;
				sDotPosition.v = newPt.v - itemRect.top;
				
				DrawDot(sDotPosition, &dotRect);
				oldPt = newPt;
			}
			
			GetMouse(&newPt);
			pinned = PinRect(&itemRect, newPt);
			newPt = * (Point *) &pinned;
		}
		
		simulatedItem = kPictItem;
	}
	else
	{
		simulatedItem = 0;
	}
	
	pb->itemHit = simulatedItem + pb->baseItems;
}

/*
 *	ItemHit		-	handle an itemHit in a Preferences panel
 *
 */

static void ItemHit(PanelParameterBlock *pb)
{
	short	theItem	= pb->itemHit - pb->baseItems;
	long	oldValue;
	
	switch (theItem) {
	case kXPIDLModeItem:
		CWPanlGetItemValue(pb, theItem, &oldValue);
		CWPanlEnableItem(pb, kXPTLinkerOutputItem, oldValue == kXPIDLModeTypelib);
		break;
		
	case kXPIDLWarningsItem:
	case kXPIDLVerboseItem:
		CWPanlGetItemValue(pb, theItem, &oldValue);
		break;
	}
	
	GetData(pb, pb->currentPrefs, false);

	pb->canRevert	= !ComparePrefs(pb->originalPrefs, pb->currentPrefs);
	pb->canFactory	= !ComparePrefs(pb->factoryPrefs,  pb->currentPrefs);
}

/*
 *	FindStatus
 *
 */
static void FindStatus(PanelParameterBlock *pb)
{
	short	command	= pb->itemHit;
	Boolean	enabled	= false;
	
	if (sPictActive)
	{
		// itemHit contains the menu command we want the status for
		
		switch (command)
		{
		case menu_Copy:
			enabled = true;
			break;
		}
	}
	
	pb->itemHit = enabled;
}

/*
 *	ObeyCommand
 *
 */

static void ObeyCommand(PanelParameterBlock *pb)
{
	short		command			= pb->itemHit;
	short		simulatedItem	= 0;
#if 0
	Rect		itemRect;
	PicHandle	thePict;
	Handle		pictH;
#endif
	
	switch (command)
	{
	case menu_Copy:
#if 0
		CWPanlGetItemRect(pb, kPictItem, &itemRect);
		InsetRect(&itemRect, 3, 3);
		thePict = OpenPicture(&itemRect);
		ClipRect(&itemRect);
		DrawDot(sDotPosition, &itemRect);
		ClosePicture();
		ZeroScrap();
		pictH = (Handle) thePict;
		HLock(pictH);
		PutScrap(GetHandleSize(pictH), 'PICT', *pictH);
		KillPicture(thePict);
#endif
		break;
	}
	
	pb->itemHit = simulatedItem /* ????? + pb->baseItems */;
}

/*
 *	Validate	-	check if panel's changes require a recompile or relink
 *
 */

static void Validate(Handle original, Handle current, Boolean *recompile, Boolean *relink, Boolean *reset)
{
#pragma unused(original, current)
	XPIDLSettings& origSettings = **(XPIDLSettingsHandle) original;
	XPIDLSettings& currentSettings = **(XPIDLSettingsHandle) current;
	
	*recompile	= currentSettings.mode != origSettings.mode;
	*relink		= *recompile && (currentSettings.mode == kXPIDLModeTypelib);
	*reset		= false;
}

/*
 *	GetPref		-	get a specified Preference setting for an AppleEvent request
 *
 */
static short GetPref(AEKeyword keyword, AEDesc *prefsDesc, Handle settings)
{
	XPIDLSettings	prefsData	= ** (XPIDLSettingsHandle) settings;
	DescType	anEnum;
	OSErr		err;

	switch (keyword)  {
#if 0	
	case prefsLN_GenerateSymFile:
		err = AECreateDesc(typeBoolean, &prefsData.linksym, sizeof(Boolean), prefsDesc);
		break;
		
	case prefsPR_ProjectType:
		switch (prefsData.projtype)
		{
		case kProjTypeApplication:	anEnum = enum_Project_Application;		break;
		case kProjTypeLibrary:		anEnum = enum_Project_Library;			break;
		case kProjTypeSharedLib:	anEnum = enum_Project_SharedLibrary;	break;
		case kProjTypeCodeResource:	anEnum = enum_Project_CodeResource;		break;
		case kProjTypeMPWTool:		anEnum = enum_Project_MPWTool;			break;
		default:					return (paramErr);
		}
		err = AECreateDesc(typeEnumeration, &anEnum, sizeof(anEnum), prefsDesc);
		break;
		
	case prefsPR_FileName:
		err = AECreateDesc(typeChar, prefsData.outfile+1, StrLength(prefsData.outfile), prefsDesc);
		break;
#endif

	default:
		err = errAECantHandleClass;
		break;
	}
	
	return (err);
}

/*
 *	SetPref		-	set a specified Preference setting from an AppleEvent request
 *
 */

static short SetPref(AEKeyword keyword, const AEDesc *prefsDesc, Handle settings)
{
	XPIDLSettings	prefsData	= ** (XPIDLSettingsHandle) settings;
	AEDesc			toDesc	= { typeNull, NULL };
	OSErr			err		= noErr;
	Handle			dataHand;
	Size			textLength;
	DescType		anEnum;
	
	switch (keyword)
	{
#if 0
	case prefsLN_GenerateSymFile:
		if (prefsDesc->descriptorType == typeBoolean)
		{
			dataHand = prefsDesc->dataHandle;
		}
		else
		{
			err = AECoerceDesc(prefsDesc, typeBoolean, &toDesc);
			if (err == noErr)
				dataHand = toDesc.dataHandle;
		}
		if (err == noErr)
		{
			prefsData.linksym = ** (Boolean **) dataHand;
		}
		break;
		
	case prefsPR_ProjectType:
		if (prefsDesc->descriptorType != typeEnumeration)
		{
			err = errAETypeError;
			break;
		}

		anEnum = ** (DescType **) prefsDesc->dataHandle;
		
		switch (anEnum)
		{
		case enum_Project_Application:		prefsData.projtype = kProjTypeApplication;	break;
		case enum_Project_Library:			prefsData.projtype = kProjTypeLibrary;		break;
		case enum_Project_SharedLibrary:	prefsData.projtype = kProjTypeSharedLib;	break;
		case enum_Project_CodeResource:		prefsData.projtype = kProjTypeCodeResource;	break;
		case enum_Project_MPWTool:			prefsData.projtype = kProjTypeMPWTool;		break;
		default:							return (errAECoercionFail);
		}
		break;
		
	case prefsPR_FileName:
		if (prefsDesc->descriptorType == typeChar)
		{
			dataHand = prefsDesc->dataHandle;
		}
		else
		{
			err = AECoerceDesc(prefsDesc, typeChar, &toDesc);
			if (err == noErr)
				dataHand = toDesc.dataHandle;
		}
		if (err == noErr)
		{
			textLength = GetHandleSize(dataHand);
			if (textLength > sizeof(prefsData.outfile) - 1)
				textLength = sizeof(prefsData.outfile) - 1;
			BlockMoveData(*dataHand, prefsData.outfile+1, textLength);
			prefsData.outfile[0] = textLength;
		}
		break;
#endif

	default:
		err = errAECantHandleClass;
		break;
	}
	
	if (err == noErr)
	{
		** (XPIDLSettingsHandle) settings = prefsData;
	}
	
	AEDisposeDesc(&toDesc);
	
	return (err);
}

/*
 *	GetFactory	-	retrieve factory settings
 *
 */

static short GetFactory(Handle settings)
{
	Handle	factory;
	Size	size;
	OSErr	err;
	
	factory = Get1Resource('pref', kFactoryPrefsID);
	if (factory == NULL) {
		err = ResError();
		if (err == noErr)
			err = resNotFound;
		return (err);
	}
	
	size = GetHandleSize(factory);
	SetHandleSize(settings, size);
	err = MemError();
	
	if (err == noErr) {
		BlockMoveData(*factory, *settings, size);
	}
	
	return (err);
}

/*
 *	UpdatePref	-	"upgrade" a pref to the current version
 */
static short UpdatePref(Handle settings)
{
	if (GetHandleSize(settings) != sizeof(XPIDLSettings))
		GetFactory(settings);

	return (noErr);
}

/*
 *	ComparePrefs
 *
 */
static Boolean ComparePrefs(Handle prefsHand1, Handle prefsHand2)
{
	XPIDLSettings& prefs1	= **(XPIDLSettingsHandle) prefsHand1;
	XPIDLSettings& prefs2	= **(XPIDLSettingsHandle) prefsHand2;
	
	return ((prefs1.mode  == prefs2.mode) && 
			(prefs1.warnings == prefs2.warnings) && 
			(prefs1.verbose == prefs2.verbose) &&
			(EqualString(prefs1.output, prefs2.output, true, true)));
}

static Boolean ComparePrefs(XPIDLSettings& prefs1, XPIDLSettings& prefs2)
{
	return ((prefs1.mode  == prefs2.mode) && 
			(prefs1.warnings == prefs2.warnings) && 
			(prefs1.verbose == prefs2.verbose) &&
			(EqualString(prefs1.output, prefs2.output, true, true)));
}

/*
 *	OutlineRect
 *
 */
static void	OutlineRect(const Rect* focusRect, Boolean outlineOn)
{
	ColorSpec	savedForeColor, backColor;
	PenState	savedPen;
	
	GetPenState(&savedPen);
	PenNormal();
	
	if (!outlineOn)
	{
		SaveFore(&savedForeColor);
		SaveBack(&backColor);
		RestoreFore(&backColor);
	}
	
	PenSize(2, 2);
	FrameRect(focusRect);
	
	SetPenState(&savedPen);
	
	if (!outlineOn)
	{
		RestoreFore(&savedForeColor);
	}
}

/*
 *	DragEnter
 *
 */
static OSErr	DragEnter(PanelParameterBlock *pb)
{
	short			theItem	= pb->itemHit - pb->baseItems;
	unsigned short	itemCount;
	Rect			itemRect;
	OSErr			err;
	
	/* Return paramErr if the user is on a item that can't be dropped on */
	if (theItem != kPictItem)
		return (paramErr);
	
	/* At this point, test if the item can accept the dragged data.  We don't */
	/* actually handle dropping data, so we just check if there are any drag */
	/* items.  You should return dragNotAcceptedErr if you can't handle the */
	/* data being dragged. */
	err = CountDragItems(pb->dragref, &itemCount);
	if ((err != noErr) || (itemCount < 1))
		return (dragNotAcceptedErr);
	
	/* Compute the highlight region */
	CWPanlGetItemRect(pb, kPictItem, &itemRect);
	InsetRect(&itemRect, 4, 4);
	RectRgn(sDragRgn, &itemRect);
	
	/* If the mouse isn't in the highlight region, act as if we can't handle */
	/* the drag. */
	if (!PtInRgn(pb->dragmouse, sDragRgn))
		return (paramErr);
	
///	SysBreakStr("\preqDragEnter");
	
	/* Return a modified item rect in dragrect.  This is useful if the highlight */
	/* region is a different shape than the item rect (as is the case here).  You */
	/* can also make the dragrect larger, to handle autoscrolling for example. */
	/* The IDE uses the modified dragrect for determining which item the mouse is */
	/* in in the reqDragWithin and reqDragExit requests. */
	pb->dragrect = itemRect;
	
	/* draw the highlight region */
	sHighlightOn = false;
	err = ShowDragHilite(pb->dragref, sDragRgn, true);
	if (err == noErr)
		sHighlightOn = true;
	
	return (err);
}

/*
 *	DragWithin
 *
 */
static void	DragWithin(PanelParameterBlock *pb)
{
#pragma unused(pb)

	/* there's nothing to do */
	
///	SysBreakStr("\preqDragWithin");
}

/*
 *	DragExit
 *
 */
static void	DragExit(PanelParameterBlock *pb)
{
	OSErr	err;
	
///	SysBreakStr("\preqDragExit");
	
	if (sHighlightOn)
	{
		err = HideDragHilite(pb->dragref);
		if (err == noErr)
			sHighlightOn = false;
	}
}

/*
 *	DragDrop
 *
 */
static void	DragDrop(PanelParameterBlock *pb)
{
	Rect	itemRect, dotRect;
	
///	SysBreakStr("\preqDragDrop");
	
	DragExit(pb);
	
	CWPanlGetItemRect(pb, kPictItem, &itemRect);
	InsetRect(&itemRect, 3, 3);
	dotRect = itemRect;
	InsetRect(&itemRect, 1, 1);
	
	sDotPosition.h = pb->dragmouse.h - itemRect.left;
	sDotPosition.v = pb->dragmouse.v - itemRect.top;
	
	DrawDot(sDotPosition, &dotRect);
}

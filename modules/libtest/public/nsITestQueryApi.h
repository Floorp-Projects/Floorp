#ifndef nsITestQueryApi_h__
#define nsITestQueryApi_h__

#include "nsISupports.h"
#include "layprobe.h"

// {47A6D121-3C0C-11d2-9A9E-0080C8845D91}
#define NS_ITESTQUERYAPI_IID \
	{0x47a6d121, 0x3c0c, 0x11d2, \
	{0x9a, 0x9e, 0x0, 0x80, 0xc8, 0x84, 0x5d, 0x91 }}

// 
//	nsITestQueryApi Interface declaration
//////////////////////////////////////

class nsITestQueryApi: public nsISupports {
public:

	NS_IMETHOD GetFrames(
		XP_List** lppList
	)=0; 	

	NS_IMETHOD FrameGetStringProperty( 
		MWContext*		FrameID,
		char*			PropertyName,
		char**			lpszPropVal
	)=0;

	NS_IMETHOD FrameGetNumProperty( 
		MWContext*		FrameID,
		char*			PropertyName,
		int32*			lpPropVal			
	)=0;
	
	NS_IMETHOD FrameGetElements(
		XP_List*	lpList,
		MWContext*	FrameID,
		int16		ElementLAPIType,
		char*		ElementName
	)=0;


	NS_IMETHOD FrameGetElementFromPoint(
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		int xPos,
		int yPos
	)=0;


	NS_IMETHOD GetFirstElement (
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID
	)=0;


	NS_IMETHOD GetNextElement (
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		LO_Element*		ElementID
	)=0;

  
	NS_IMETHOD GetPrevElement (
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		LO_Element*		ElementID
	)=0;

  
	NS_IMETHOD GetChildElement (
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		LO_Element*		ElementID
	)=0;

	NS_IMETHOD ElementGetStringProperty( 
		MWContext*		FrameID,
		LO_Element*		ElementID,
		char*			PropertyName,
		char**			lpszPropVal
	)=0;


	NS_IMETHOD ElementGetNumProperty( 
		MWContext*		FrameID,
		LO_Element*		ElementID,
		char*			PropertyName,
		int32*			lpPropVal			
	)=0;


	NS_IMETHOD GetParnetElement ( 
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		LO_Element*		ElementID
	)=0;
};

#endif /* nsITestQueryApi_h__ */
/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1997                                                 *
*   (C) Copyright International Business Machines Corporation,  1997                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*/
//===============================================================================
//
// File chrstring.h
//
// Created by: Alan Liu
//
// Modification History:
//
//	Date         Name          Description
//
//===============================================================================

#ifndef _CHRSTRING
#define _CHRSTRING

//------------------------------------------------------------------------------

// CharString is a very simple wrapper class for a char*.  When the wrapper
// object is destructed, the char* pointer is deleted.

#ifdef NLS_MAC
#pragma export on
#endif

class CharString
{
public:
	CharString(char* adopted) { fString = adopted; }
	~CharString() { delete[] fString; }
	operator char*() { return fString; }
protected:
    // These are really private, but some compilers might not like that.
    // These are not implemented, not to be called.
    CharString(const CharString& other) {}
    CharString& operator=(const CharString& other) { return *this; }
private:
	char* fString;
};

#ifdef NLS_MAC
#pragma export off
#endif

#endif //_CHRSTRING
//eof

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

// icalfrdr.cpp
// John Sun
// 3:03 PM February 10 1998

#include <stdlib.h>
#include <stdio.h>

#include "stdafx.h"
#include "jdefines.h"

#include "icalfrdr.h"

//---------------------------------------------------------------------

ICalFileReader::ICalFileReader() {}

//---------------------------------------------------------------------

ICalFileReader::ICalFileReader(char * filename, ErrorCode & status)
{
    m_filename = filename;
    m_file = fopen(filename, "r");

    if (m_file == 0) 
    {
        printf("Can't open %s\n", filename);
        status = 1;
    }
}
//---------------------------------------------------------------------
ICalFileReader::~ICalFileReader()
{
    if (m_file) { fclose(m_file); m_file = 0; }
}

//---------------------------------------------------------------------

t_int8 ICalFileReader::read(ErrorCode & status)
{
    int c = fgetc(m_file);
    if (c == EOF)
    {
        status = 1;
        return -1;
    }
    else 
        return (t_int8) c;
}
//---------------------------------------------------------------------
// TODO: handle quoted-printable
UnicodeString & 
ICalFileReader::readLine(UnicodeString & aLine, ErrorCode & status)
{
    status = ZERO_ERROR;
    aLine = "";
    
	if ( NULL != fgets(m_pBuffer,1023,m_file) )
	{
		t_int32 iLen = strlen(m_pBuffer);
		if (m_pBuffer[iLen-1] == '\n')
			m_pBuffer[iLen-1] = 0;
        {
            aLine = m_pBuffer;
            return aLine;
            //return 0;
        }
	}
    status = 1;
    return aLine;
	//return 1;
}
//---------------------------------------------------------------------
// NOTE: TODO: make faster profiling?
UnicodeString & 
ICalFileReader::readFullLine(UnicodeString & aLine, ErrorCode & status, t_int32 iTemp)
{
    status = ZERO_ERROR;
    t_int8 i;

    readLine(aLine, status);
    //if (FALSE) TRACE("rfl(1) %s\r\n", aLine.toCString(""));

    if (FAILURE(status))
    {
        return aLine;
    }
    UnicodeString aSubLine;
    while (TRUE)
    {
        i = read(status);
        if (i != -1 && i == ' ')
        {
            aLine += readLine(aSubLine, status);
            //if (FALSE) TRACE("rfl(2) %s\r\n", aLine.toCString(""));
        }
        else if (i == -1)
        {
            return aLine;
        }
        else
        {
            ungetc(i, m_file);
            break;
        }
    }
    //if (FALSE) TRACE("end of rfl: ---%s---\r\n", aLine.toCString(""));
    return aLine;
}
//---------------------------------------------------------------------


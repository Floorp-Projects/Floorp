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

// jlog.cpp
// John Sun
// 10:52 AM March 2 1998

#include "stdafx.h"
#include "jdefines.h"
#include <unistring.h>
#include "jlog.h"

//---------------------------------------------------------------------

const t_int32 JLog::m_DEFAULT_LEVEL = 200;
const char * JLog::m_DEFAULT_FILENAME = "JLog.txt";

//JLog * JLog::m_Log = 0;

//t_int32 JLog::m_Level = m_DEFAULT_LEVEL;
//const char * JLog::m_FileName = m_DEFAULT_FILENAME;
//---------------------------------------------------------------------

JLog::JLog(t_int32 initLevel) :
 m_Level(initLevel),
 m_FileName(0),
 m_WriteToString(TRUE)
{
    //m_String = new UnicodeString();
}
 
//---------------------------------------------------------------------

JLog::JLog(const char * initFileName, t_int32 initLevel) :
 m_Level(initLevel),
 m_FileName(initFileName),
 m_WriteToString(FALSE)
{
    m_Stream = fopen(m_FileName, "w");
    assert(m_Stream != 0);
}

//---------------------------------------------------------------------

JLog::~JLog()
{
    //fclose(m_Stream);
    ////delete m_Log;
    //if (m_String != 0)
    //{
    //    delete m_String; m_String = 0;
    //}
}

//---------------------------------------------------------------------
/*
JLog * JLog::Instance()
{
    if (m_Log == 0)
        m_Log = new JLog();
    assert(m_Log != 0);
    return m_Log;
}
*/
//---------------------------------------------------------------------

void JLog::log(char * message, t_int32 level)
{
    if (m_WriteToString)
    {
        if (level >= m_Level)
        {
            m_String += message;
            m_String += "\r\n";
        }
    }
    else
    {
        if (level >= m_Level)
            fprintf(m_Stream, "%s\r\n", message);
    }
}

//---------------------------------------------------------------------

void JLog::log(char * error, char * classname, char * comment, 
        t_int32 level)
{
    if (m_WriteToString)
    {
        if (level >= m_Level)
        {
            m_String += error;
            m_String += " ";
            m_String += classname;
            m_String += ":";
            m_String += comment;
            m_String += "\r\n";
        }
    }
    else
    {
        if (level >= m_Level)
            fprintf(m_Stream, "%s %s:%s\r\n", error, classname, comment);
    }
}

//---------------------------------------------------------------------

void JLog::log(char * error, char * classname, char * propName, 
               char * propValue, 
        t_int32 level)
{
    if (m_WriteToString)
    {
        if (level >= m_Level)
        {
            m_String += error;
            m_String += " ";
            m_String += classname;
            m_String += ";";
            m_String += propName;
            m_String += ":";
            m_String += propValue;
            m_String += "\r\n";
        }
    }
    else
    {
        if (level >= m_Level)
            fprintf(m_Stream, "%s %s;%s:%s\r\n", error, classname, propName, propValue);
    }
}
//---------------------------------------------------------------------
void JLog::logString(const UnicodeString & message, t_int32 level)
{
    log(message.toCString(""), level);
}
 //---------------------------------------------------------------------
   
void JLog::logString(const UnicodeString & error, const UnicodeString & classname, 
               UnicodeString & comment, t_int32 level)
{
    log(error.toCString(""), classname.toCString(""), 
        comment.toCString(""), level);
}
//---------------------------------------------------------------------

void JLog::logString(const UnicodeString & error, const UnicodeString & classname, 
               UnicodeString & propName, UnicodeString & propValue,
               t_int32 level)
{
    log(error.toCString(""), classname.toCString(""), 
        propName.toCString(""), propValue.toCString(""), level);
}

//---------------------------------------------------------------------



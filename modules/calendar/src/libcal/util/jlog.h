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

/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*- */
/* 
 * jlog.h
 * John Sun
 * 3/2/98 10:46:48 AM
 */

#ifndef __JLOG_H_
#define __JLOG_H_

#include <unistring.h> 

/** 
 *  The JLog class defines a log file to write to while parsing
 *  iCal objects.  All iCal objects should take a JLog object during
 *  creation.  
 *  The log currently writes to a file or to a JulianString in memory.
 *  To write to a file, the logString or log method is called, passing
 *  in the strings to write to the log, and a priority level of the 
 *  message.  If the priority level of the message is greater than
 *  the current log's internal priority level, then the message is 
 *  written.  
 *  Here is the current priority levels:
 *  Setting to Defaults = 0
 *  Minor parse error: (i.e. Illegal duplicate properties, parameters) = 100
 *  Invalid Attendee, Freebusy, property name, property value = 200.
 *  Invalid Number format = 200.
 *  Abrupt end of parsing iCal object = 300.
 *  Invalid Event, Todo, Journal, VFreebusy, VTimeZone = 300.
 */
class JLog
{
private:

    /** 
     * Default log level
     */
    static const t_int32 m_DEFAULT_LEVEL;

    /**
     * Default log filename.
     */
    static const char * m_DEFAULT_FILENAME;

    /* data members */

    /** current log filename */
    const char * m_FileName;

    /** current string of errors */
    UnicodeString m_String;

    /** TRUE = writeToString, FALSE = writeToFile, immutable */
    t_bool m_WriteToString;

    /** current log level */
    t_int32 m_Level;

    /** ptr to File */
    FILE * m_Stream;

public:



    /**
     * Constructor defines new log file.  Using this constructor
     * will write messages to the private string data-member.
     * @param           t_int32 initLevel = m_DEFAULT_LEVEL
     */
    JLog(t_int32 initLevel = m_DEFAULT_LEVEL);

    /**
     * Constructor defines new log file.  Using this constructor
     * will write message to a file.  If no initFileName,
     * defaults to default filename,  If no level, defaults
     * to default priority level
     * @param           initFilename    initial filename
     * @param           initLevel       initial priority level
     */
    JLog(const char * initFilename, 
        t_int32 initLevel = m_DEFAULT_LEVEL);
    
    /**
     * Destructor.
     */
    ~JLog();

    /**
     * Sets priority level.
     * @param           level   new priority level
     *
     * @return          void 
     */
    void SetLevel(t_int32 level) { m_Level = level; }

    /**
     * Gets priority level.
     *
     * @return          current priority level
     */
    t_int32 GetLevel() const { return m_Level; }

    /**
     * Return log string
     * @return          log string
     */
    UnicodeString GetString() const { return m_String; }

    /* methods to write to log with char* */
    void log(char * message, t_int32 level = m_DEFAULT_LEVEL);
    void log(char * error, char * classname, char * comment, 
        t_int32 level = m_DEFAULT_LEVEL);
    void log(char * error, char * classname, char * propName, char * propValue, 
        t_int32 level = m_DEFAULT_LEVEL);

    /* methods to write to log with UnicodeString */
    void logString(const UnicodeString & message, t_int32 level = m_DEFAULT_LEVEL);
    void logString(const UnicodeString & error, const UnicodeString & classname, 
        UnicodeString & comment, t_int32 level = m_DEFAULT_LEVEL);
    void logString(const UnicodeString & error, const UnicodeString & classname, 
        UnicodeString & propName, UnicodeString & propValue, 
        t_int32 level = m_DEFAULT_LEVEL);
};

#endif /* __JLOG_H_ */



/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the 'NPL'); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an 'AS IS' basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */
/* 
 * functbl.h
 * John Sun
 * 7/27/98 1:57:12 PM
 */

#include "valarm.h"
#include "tmbevent.h"
#include "tzpart.h"
#include "vevent.h"
#include "vfrbsy.h"
#include "vtodo.h"

#ifndef __JULIANFUNCTIONTABLE_H_
#define __JULIANFUNCTIONTABLE_H_

/**
 *  These structs are used by
 *  classes that need to contain ICalProperty's to
 *  quickly find the function to store data-members.
 *  Its keeps a hashCode and a ptr to a store-property
 *  function
 */
typedef struct 
{
    t_int32 hashCode;
    VAlarm::SetOp op;
    void set(t_int32 i, VAlarm::SetOp sOp) { hashCode = i; op = sOp; }
} VAlarmStoreTable;

typedef struct
{
    t_int32 hashCode;
    TimeBasedEvent::SetOp op;
    void set(t_int32 i, TimeBasedEvent::SetOp sOp) { hashCode = i; op = sOp; }
} TimeBasedEventStoreTable;

typedef struct
{
    t_int32 hashCode;
    TZPart::SetOp op;
    void set(t_int32 i, TZPart::SetOp sOp) { hashCode = i; op = sOp; }
} TZPartStoreTable;

typedef struct
{
    t_int32 hashCode;
    VEvent::SetOp op;
    void set(t_int32 i, VEvent::SetOp sOp) { hashCode = i; op = sOp; }
} VEventStoreTable;

typedef struct
{
    t_int32 hashCode;
    VFreebusy::SetOp op;
    void set(t_int32 i, VFreebusy::SetOp sOp) { hashCode = i; op = sOp; }
} VFreebusyStoreTable;

typedef struct
{
    t_int32 hashCode;
    VTodo::SetOp op;
    void set(t_int32 i, VTodo::SetOp sOp) { hashCode = i; op = sOp; }
} VTodoStoreTable;

class JulianFunctionTable
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    static JulianFunctionTable * m_Instance;
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    JulianFunctionTable();
public:
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    ~JulianFunctionTable();
    static JulianFunctionTable * Instance();

    /* VAlarm storeData function table */
    VAlarmStoreTable alarmStoreTable[10]; 
    TimeBasedEventStoreTable tbeStoreTable[25];
    TZPartStoreTable tzStoreTable[8];
    VEventStoreTable veStoreTable[8];
    /*    VFreebusyStoreTable vfStoreTable[16]; */
    VFreebusyStoreTable vfStoreTable[15];
    VTodoStoreTable vtStoreTable[9];
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 
    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
    /*----------------------------- 
    ** STATIC METHODS 
    **---------------------------*/ 
    /*----------------------------- 
    ** OVERLOADED OPERATORS 
    **---------------------------*/ 
};

#endif /* __JULIANFUNCTIONTABLE_H_ */


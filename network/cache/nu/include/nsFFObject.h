/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* The nsFFObject class represent a single object block that is the smallest 
 * page unit on the flat file. This class is part of the flat file cache architecture.
 * It consists of the offset of the block and the size in use. 
 * -Gagan Saksena 09/15/98 */

#ifndef nsFFObject_h__
#define nsFFObject_h__

#include "prtypes.h"

class nsFFObject
{

public:
            nsFFObject(
                PRUint32 i_ID, 
                PRUint32 i_Offset, 
                PRUint32 i_Size = 0);

    virtual ~nsFFObject();

    PRBool      Add(const nsFFObject* i_object);

    PRUint32    ID(void) const;
    void        ID(const PRUint32 i_ID);

    nsFFObject* Next(void) const;
    void        Next(nsFFObject* io_pNext);

    PRUint32    Offset(void) const;
    void        Offset(const PRUint32 i_offset);

    PRUint32    Size(void) const;
    void        Size(const PRUint32 i_Size);

    PRUint32    TotalSize(void) const;

protected:

private:
    nsFFObject(const nsFFObject& o);
    nsFFObject& operator=(const nsFFObject& o);
    
    PRUint32 m_ID;
    PRUint32 m_Offset;
    PRUint32 m_Size;

    nsFFObject* m_pNext;
};

inline
PRUint32 nsFFObject::ID(void) const
{
    return m_ID;
}

inline
void nsFFObject::ID(const PRUint32 i_ID)
{
    m_ID = i_ID;
}

inline
nsFFObject* nsFFObject::Next(void) const
{
    return m_pNext;
}

inline 
void nsFFObject::Next(nsFFObject* io_pNext)
{
    if (io_pNext)
        io_pNext->ID(m_ID);
    m_pNext = io_pNext;
    // Overlap check! //TODO
    //PR_ASSERT(io_pNext->Offset() > m_Offset + m_Size) ||
        // (io_pNext->Offset() + io_pNext->Size() < m_Offset)
}

inline
PRUint32 nsFFObject::Offset(void) const
{
    return m_Offset;
}

inline
void nsFFObject::Offset(const PRUint32 i_Offset)
{
    m_Offset = i_Offset;
    //TODO - overlap check. 
}

inline
PRUint32 nsFFObject::Size(void) const
{
    return m_Size;
}

inline
void nsFFObject::Size(const PRUint32 i_Size)
{
    m_Size = i_Size;
    //TODO - Overlap check. 
}

inline 
PRUint32 nsFFObject::TotalSize(void) const
{
    return m_Size + (m_pNext ? m_pNext->TotalSize() : 0);
}
#endif // nsFFObject_h__


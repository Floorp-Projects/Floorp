/*
********************************************************************************
*                                                                              *
* COPYRIGHT:                                                                   *
*   (C) Copyright Taligent, Inc.,  1997                                        *
*   (C) Copyright International Business Machines Corporation,  1997           *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.         *
*   US Government Users Restricted Rights - Use, duplication, or disclosure    *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                     *
*                                                                              *
********************************************************************************
*
* File PARSEPOS.H
*
* Modification History:
*
*   Date        Name        Description
*   07/09/97    helena      Converted from java.
********************************************************************************
*/
// *****************************************************************************
// This file was generated from the java source file Format.java
// *****************************************************************************

#ifndef _PARSEPOS
#define _PARSEPOS

#ifndef _PTYPES
#include "ptypes.h"
#endif
     
/**
 * <code>ParsePosition</code> is a simple class used by <code>Format</code>
 * and its subclasses to keep track of the current position during parsing.
 * The <code>parseObject</code> method in the various <code>Format</code>
 * classes requires a <code>ParsePosition</code> object as an argument.
 *
 * <p> 
 * By design, as you parse through a string with different formats,
 * you can use the same <code>ParsePosition</code>, since the index parameter
 * records the current position.
 *
 * @version     1.3 10/30/97
 * @author      Mark Davis, Helena Shih
 * @see         java.text.Format
 */
#ifdef NLS_MAC
#pragma export on
#endif
class T_FORMAT_API ParsePosition {
public:
    /**
     * Default constructor, the index starts with 0 as default.
     */
    ParsePosition() { this->index = 0; }

    /**
     * Create a new ParsePosition with the given initial index.
     * @param index the new text offset.
     */
    ParsePosition(TextOffset index) { this->index = index; } 

    /**
     * Copy constructor
     * @param copy the object to be copied from.
     */
    ParsePosition(const ParsePosition& copy) { this->index = copy.index; }

    /**
     * Destructor
     */
    ~ParsePosition() {}

    /**
     * Assignment operator
     */
    ParsePosition&      operator=(const ParsePosition& copy);

    /** 
     * Equality operator.
     * @return TRUE if the two parse positions are equal, FALSE otherwise.
     */
    t_bool              operator==(const ParsePosition& that) const;

    /** 
     * Equality operator.
     * @return TRUE if the two parse positions are not equal, FALSE otherwise.
     */
    t_bool              operator!=(const ParsePosition& that) const;

    /**
     * Retrieve the current parse position.  On input to a parse method, this
     * is the index of the character at which parsing will begin; on output, it
     * is the index of the character following the last character parsed.
     * @return the current index.
     */
    TextOffset getIndex() const;

    /**
     * Set the current parse position.
     * @param index the new index.
     */
    void setIndex(TextOffset index);

private:
    /**
     * Input: the place you start parsing.
     * <br>Output: position where the parse stopped.
     * This is designed to be used serially,
     * with each call setting index up for the next one.
     */
    TextOffset index;

};
#ifdef NLS_MAC
#pragma export off
#endif

inline ParsePosition&
ParsePosition::operator=(const ParsePosition& copy)
{
    index = copy.index;
    return *this;
}

inline t_bool
ParsePosition::operator==(const ParsePosition& copy) const
{
    t_bool result;

    if (index != copy.index) result=FALSE;
    else result=TRUE;

    return result;
}

inline t_bool
ParsePosition::operator!=(const ParsePosition& copy) const
{
    return !operator==(copy);
}

inline TextOffset
ParsePosition::getIndex() const
{
    return index;
}

inline void
ParsePosition::setIndex(TextOffset idx)
{
    this->index = idx;
}

#endif

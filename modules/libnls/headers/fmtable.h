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
* File FMTABLE.H
*
* Modification History:
*
*   Date        Name        Description
*   02/29/97    aliu        Creation.
********************************************************************************
*/
#ifndef _FMTABLE
#define _FMTABLE

#include "ptypes.h"
#include "unistring.h"
#include <memory.h>

/**
 * Formattable objects can be passed to the Format class or
 * its subclasses for formatting.  Formattable is a thin wrapper
 * class which interconverts between the primitive numeric types
 * (double, long, etc.) as well as Date and UnicodeString.
 * <P>
 * Note that this is fundamentally different from the Java behavior, since
 * in this case the various formattable objects do not occupy a hierarchy,
 * but are all wrapped within this one class.  Formattable encapsulates all
 * the polymorphism in itself.
 * <P>
 * It would be easy to change this so that Formattable was an abstract base
 * class of a genuine hierarchy, and that would clean up the code that
 * currently must explicitly check for type, but that seems like overkill at
 * this point.
 */
#ifdef NLS_MAC
#pragma export on
#endif
class T_FORMAT_API Formattable {
public:
    /**
     * This enum is only used to let callers distinguish between
     * the Formattable(Date) constructor and the Formattable(double)
     * constructor; the compiler cannot distinguish the signatures,
     * since Date is currently typedefed to be either double or long.
     * If Date is changed later to be a bonafide class
     * or struct, then we no longer need this enum.
     */
                    enum ISDATE { kIsDate };

                    Formattable(); // Type kLong, value 0
    /**
     * Creates a Formattable object with a Date instance.
     * @param d the Date instance.
     * @param ISDATE the flag to indicate this is a date.
     */
                    Formattable(Date d, ISDATE);
    /**
     * Creates a Formattable object with a double number.
     * @param d the double number.
     */
                    Formattable(double d);
    /**
     * Creates a Formattable object with a long number.
     * @param d the long number.
     */
                    Formattable(long l);
    /**
     * Creates a Formattable object with a char string pointer.
     * Assumes that the char string is null terminated.
     * @param strToCopy the char string.
     */
                    Formattable(const char* strToCopy);
    /**
     * Creates a Formattable object with a UnicodeString object to copy from.
     * @param strToCopy the UnicodeString string.
     */
                    Formattable(const UnicodeString& stringToCopy);
    /**
     * Creates a Formattable object with a UnicodeString object to adopt from.
     * @param strToAdopt the UnicodeString string.
     */
                    Formattable(UnicodeString* stringToAdopt);
    /**
     * Creates a Formattable object with an array of Formattable objects.
     * @param arrayToCopy the Formattable object array.
     * @param count the array count.
     */
                    Formattable(const Formattable* arrayToCopy, long count);

    /**
     * Copy constructor.
     */
                    Formattable(const Formattable&);
    /**
     * Assignment operator.
     */
    Formattable&    operator=(const Formattable&);
    /**
     * Equality comparison.
     */
    t_bool          operator==(const Formattable&) const;
    t_bool          operator!=(const Formattable& other) const
      { return !operator==(other); }

    /** Destructor.
    */
    virtual         ~Formattable();

    /** 
     * The list of possible data types of this Formattable object.
     */
    enum Type {
        kDate,      // Date
        kDouble,    // double
        kLong,      // long
        kString,    // UnicodeString
        kArray      // Formattable[]
    };

    /**
     * Gets the data type of this Formattable object.
     */
    Type            getType() const;
    
    /**
     * Gets the double value of this object.
     */ 
    double          getDouble() const { return fValue.fDouble; }
    /**
     * Gets the long value of this object.
     */ 
    long            getLong() const { return fValue.fLong; }
    /**
     * Gets the Date value of this object.
     */ 
    Date            getDate() const { return fValue.fDate; }

    /**
     * Gets the string value of this object.
     */ 
    UnicodeString&  getString(UnicodeString& result) const
      { result=*fValue.fString; return result; }

    /**
     * Gets the array value and count of this object.
     */ 
    const Formattable* getArray(t_int32& count) const
      { count=fValue.fArrayAndCount.fCount; return fValue.fArrayAndCount.fArray; }

    /**
     * Accesses the specified element in the array value of this Formattable object.
     * @param index the specified index.
     * @return the accessed element in the array.
     */
    Formattable&    operator[](t_int32 index) { return fValue.fArrayAndCount.fArray[index]; }

    /**
     * Sets the double value of this object.
     */ 
    void            setDouble(double d);
    /**
     * Sets the long value of this object.
     */ 
    void            setLong(long l);
    /**
     * Sets the Date value of this object.
     */ 
    void            setDate(Date d);
    /**
     * Sets the string value of this object.
     */ 
    void            setString(const UnicodeString& stringToCopy);
    /**
     * Sets the array value and count of this object.
     */ 
    void            setArray(const Formattable* array, long count);
    /**
     * Sets and adopts the string value and count of this object.
     */ 
    void            adoptString(UnicodeString* stringToAdopt);
    /**
     * Sets and adopts the array value and count of this object.
     */ 
    void            adoptArray(Formattable* array, long count);
        
private:
    /**
     * Cleans up the memory for unwanted values.  For example, the adopted
     * string or array objects.
     */
    void            dispose();

    /**
     * Creates a new Formattable array and copies the values from the specified
     * original.
     * @param array the original array
     * @param count the original array count
     * @return the new Formattable array.
     */
    static Formattable* createArrayCopy(const Formattable* array, t_int32 count);

    // Note: For now, we do not handle unsigned long and unsigned
    // double types.  Smaller unsigned types, such as unsigned
    // short, can fit within a long.
    union {
        UnicodeString*  fString;
        double          fDouble;
        long            fLong;
        Date            fDate;
        struct
        {
          Formattable*  fArray;
          long          fCount;
        }               fArrayAndCount;
    }                   fValue;

    Type                fType;
};
#ifdef NLS_MAC
#pragma export off
#endif

#if 0
T_FORMAT_API ostream& operator<<(ostream&               stream,
                              const Formattable&    obj);
#endif

inline Formattable*
Formattable::createArrayCopy(const Formattable* array, t_int32 count)
{
    Formattable *result = new Formattable[count];
    for (t_int32 i=0; i<count; ++i) result[i] = array[i]; // Don't memcpy!
    return result;
}

#endif //_FMTABLE
//eof
    
 

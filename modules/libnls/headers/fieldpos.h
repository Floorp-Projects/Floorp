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
* File FIELDPOS.H
*
* Modification History:
*
*	Date		Name		Description
*	02/25/97	aliu		Converted from java.
*	03/17/97	clhuang		Updated per Format implementation.
********************************************************************************
*/
// *****************************************************************************
// This file was generated from the java source file FieldPosition.java
// *****************************************************************************
 
#ifndef _FIELDPOS
#define _FIELDPOS

#include "ptypes.h"

/**
 * <code>FieldPosition</code> is a simple class used by <code>Format</code>
 * and its subclasses to identify fields in formatted output. Fields are
 * identified by constants, whose names typically end with <code>_FIELD</code>,
 * defined in the various subclasses of <code>Format</code>. See
 * <code>ERA_FIELD</code> and its friends in <code>DateFormat</code> for
 * an example.
 *
 * <p>
 * <code>FieldPosition</code> keeps track of the position of the
 * field within the formatted output with two indices: the index
 * of the first character of the field and the index of the last
 * character of the field.
 *
 * <p>
 * One version of the <code>format</code> method in the various
 * <code>Format</code> classes requires a <code>FieldPosition</code>
 * object as an argument. You use this <code>format</code> method
 * to perform partial formatting or to get information about the
 * formatted output (such as the position of a field).
 *
 * <p>
 * Below is an example of using <code>FieldPosition</code> to aid
 * alignment of an array of formatted floating-point numbers on
 * their decimal points:
 *
 *	double doubleNum[] = {123456789.0, -12345678.9, 1234567.89, -123456.789,
 *		12345.6789, -1234.56789, 123.456789, -12.3456789, 1.23456789};
 *	int dNumSize = (int)(sizeof(doubleNum)/sizeof(double));
 *
 *	ErrorCode status = ZERO_ERROR;
 *	DecimalFormat* fmt = (DecimalFormat*) NumberFormat::createInstance(status);
 *	fmt->setDecimalSeparatorAlwaysShown(true);
 *
 *	const int tempLen = 20;
 *	char temp[tempLen];
 *
 *	for (int i=0; i<dNumSize; i++) {
 *		FieldPosition pos(NumberFormat::INTEGER_FIELD);
 *		UnicodeString buf;
 *		char fmtText[tempLen];
 *		ToCharString(fmt->format(doubleNum[i], buf, pos), fmtText);
 *		for (int j=0; j<tempLen; j++) temp[j] = ' '; // clear with spaces
 *		temp[__min(tempLen, tempLen-pos.getEndIndex())] = '\0';
 *		cout << temp << fmtText	<< endl;
 *	}
 *	delete fmt;
 *
 * <p>
 * The code will generate the following output:
 *
 *         123,456,789.000
 *         -12,345,678.900
 *           1,234,567.880
 *            -123,456.789
 *              12,345.678
 *              -1,234.567
 *                 123.456
 *                 -12.345
 *                   1.234
 *
*/
#ifdef NLS_MAC
#pragma export on
#endif

class T_FORMAT_API FieldPosition {
public:
    /**
	 * DONT_CARE may be specified as the field to indicate that the
	 * caller doesn't need to specify a field.  Do not subclass.
	 */
    enum { DONT_CARE = -1 };

    /**
     * Creates a FieldPosition object for the given field.  Fields are
     * identified by constants, whose names typically end with _FIELD,
     * in the various subclasses of Format.
     *
     * @see NumberFormat#INTEGER_FIELD
     * @see NumberFormat#FRACTION_FIELD
     * @see DateFormat#YEAR_FIELD
     * @see DateFormat#MONTH_FIELD
     */
    FieldPosition(int field) :
	fField(field), fBeginIndex(0), fEndIndex(0) {};

    /**
     * Retrieve the field identifier.
     */
    int getField() const { return fField; }

    /**
     * Retrieve the index of the first character in the requested field.
     */
    int getBeginIndex() const { return fBeginIndex; }

    /**
     * Retrieve the index of the character following the last character in the
     * requested field.
     */
    int getEndIndex() const { return fEndIndex; }
 
    /**
     * Set the field.
     */
    void setField(int f) { fField = f; }

    /**
     * Set the begin index.  For use by subclasses of Format.
     */
    void setBeginIndex(int bi) { fBeginIndex = bi; }

    /**
     * Set the end index.  For use by subclasses of Format.
     */
    void setEndIndex(int ei) { fEndIndex = ei; }

private:
    /**
     * Input: Desired field to determine start and end offsets for.
     * The meaning depends on the subclass of Format.
     */
    int fField;

    /**
     * Output: End offset of field in text.
     * If the field does not occur in the text, 0 is returned.
     */
    int fEndIndex;

    /**
     * Output: Start offset of field in text.
     * If the field does not occur in the text, 0 is returned.
     */
    int fBeginIndex;
};

#ifdef NLS_MAC
#pragma export off
#endif
 
#endif // _FIELDPOS
//eof

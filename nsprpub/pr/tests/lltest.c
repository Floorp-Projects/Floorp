/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** testll.c -- test suite for 64bit integer (longlong) operations
**
** Summary: testll [-d] | [-h]
**
** Where:
** -d       set debug mode on; displays individual test failures
** -v       verbose mode; displays progress in test, plus -d
** -h       gives usage message.
**
** Description:
** lltest.c tests the functions defined in NSPR 2.0's prlong.h.
**
** Successive tests begin to depend on other LL functions working
** correctly. So, ... Do not change the order of the tests as run
** from main().
**
** Caveats:
** Do not even begin to think that this is an exhaustive test!
**
** These tests try a little of everything, but not all boundary
** conditions and limits are tested.
** You want better coverage? ... Add it.
**
** ---
** Author: Lawrence Hardiman <larryh@netscape.com>.
** ---
** Revision History:
** 01-Oct-1997. Original implementation.
**
*/

#include "nspr.h"
#include "plgetopt.h"

/* --- Local Definitions --- */
#define ReportProgress(m) if (verboseMode) PR_fprintf(output, (m));


/* --- Global variables --- */
static PRIntn  failedAlready = 0;
static PRFileDesc* output = NULL;
static PRBool  debugMode = PR_FALSE;
static PRBool  verboseMode = PR_FALSE;

/*
** Constants used in tests.
*/
const PRInt64 bigZero        = LL_INIT( 0, 0 );
const PRInt64 bigOne         = LL_INIT( 0, 1 );
const PRInt64 bigTwo         = LL_INIT( 0, 2 );
const PRInt64 bigSixTeen     = LL_INIT( 0, 16 );
const PRInt64 bigThirtyTwo   = LL_INIT( 0, 32 );
const PRInt64 bigMinusOne    = LL_INIT( 0xffffffff, 0xffffffff );
const PRInt64 bigMinusTwo    = LL_INIT( 0xffffffff, 0xfffffffe );
const PRInt64 bigNumber      = LL_INIT( 0x7fffffff, 0xffffffff );
const PRInt64 bigMinusNumber = LL_INIT( 0x80000000, 0x00000001 );
const PRInt64 bigMaxInt32    = LL_INIT( 0x00000000, 0x7fffffff );
const PRInt64 big2To31       = LL_INIT( 0x00000000, 0x80000000 );
const PRUint64 bigZeroFox    = LL_INIT( 0x00000000, 0xffffffff );
const PRUint64 bigFoxFox     = LL_INIT( 0xffffffff, 0xffffffff );
const PRUint64 bigFoxZero    = LL_INIT( 0xffffffff, 0x00000000 );
const PRUint64 bigEightZero  = LL_INIT( 0x80000000, 0x00000000 );
const PRUint64 big64K        = LL_INIT( 0x00000000, 0x00010000 );
const PRInt64 bigInt0        = LL_INIT( 0x01a00000, 0x00001000 );
const PRInt64 bigInt1        = LL_INIT( 0x01a00000, 0x00001100 );
const PRInt64 bigInt2        = LL_INIT( 0x01a00000, 0x00000100 );
const PRInt64 bigInt3        = LL_INIT( 0x01a00001, 0x00001000 );
const PRInt64 bigInt4        = LL_INIT( 0x01a00001, 0x00001100 );
const PRInt64 bigInt5        = LL_INIT( 0x01a00001, 0x00000100 );
const PRInt64 bigInt6        = LL_INIT( 0xb1a00000, 0x00001000 );
const PRInt64 bigInt7        = LL_INIT( 0xb1a00000, 0x00001100 );
const PRInt64 bigInt8        = LL_INIT( 0xb1a00000, 0x00000100 );
const PRInt64 bigInt9        = LL_INIT( 0xb1a00001, 0x00001000 );
const PRInt64 bigInt10       = LL_INIT( 0xb1a00001, 0x00001100 );
const PRInt64 bigInt11       = LL_INIT( 0xb1a00001, 0x00000100 );
const PRInt32 one = 1l;
const PRInt32 minusOne = -1l;
const PRInt32 sixteen  = 16l;
const PRInt32 thirtyTwo = 32l;
const PRInt32   sixtyThree = 63l;

/*
** SetFailed() -- Report individual test failure
**
*/
static void
SetFailed( char *what, char *how )
{
    failedAlready = 1;
    if ( debugMode ) {
        PR_fprintf(output, "%s: failed: %s\n", what, how );
    }
    return;
}

static void
ResultFailed( char *what, char *how, PRInt64 expected, PRInt64 got)
{
    if ( debugMode)
    {
        SetFailed( what, how );
        PR_fprintf(output, "Expected: 0x%llx   Got: 0x%llx\n", expected, got );
    }
    return;
}


/*
** TestAssignment() -- Test the assignment
*/
static void TestAssignment( void )
{
    PRInt64 zero = LL_Zero();
    PRInt64 min = LL_MinInt();
    PRInt64 max = LL_MaxInt();
    if (!LL_EQ(zero, bigZero)) {
        SetFailed("LL_EQ(zero, bigZero)", "!=");
    }
    if (!LL_CMP(max, >, min)) {
        SetFailed("LL_CMP(max, >, min)", "!>");
    }
}

/*
** TestComparisons() -- Test the longlong comparison operations
*/
static void
TestComparisons( void )
{
    ReportProgress("Testing Comparisons Operations\n");

    /* test for zero */
    if ( !LL_IS_ZERO( bigZero )) {
        SetFailed( "LL_IS_ZERO", "Zero is not zero" );
    }

    if ( LL_IS_ZERO( bigOne )) {
        SetFailed( "LL_IS_ZERO", "One tests as zero" );
    }

    if ( LL_IS_ZERO( bigMinusOne )) {
        SetFailed( "LL_IS_ZERO", "Minus One tests as zero" );
    }

    /* test equal */
    if ( !LL_EQ( bigZero, bigZero )) {
        SetFailed( "LL_EQ", "zero EQ zero");
    }

    if ( !LL_EQ( bigOne, bigOne )) {
        SetFailed( "LL_EQ", "one EQ one" );
    }

    if ( !LL_EQ( bigNumber, bigNumber )) {
        SetFailed( "LL_EQ", "bigNumber EQ bigNumber" );
    }

    if ( !LL_EQ( bigMinusOne, bigMinusOne )) {
        SetFailed( "LL_EQ", "minus one EQ minus one");
    }

    if ( LL_EQ( bigZero, bigOne )) {
        SetFailed( "LL_EQ", "zero EQ one");
    }

    if ( LL_EQ( bigOne, bigZero )) {
        SetFailed( "LL_EQ", "one EQ zero" );
    }

    if ( LL_EQ( bigMinusOne, bigOne )) {
        SetFailed( "LL_EQ", "minus one EQ one");
    }

    if ( LL_EQ( bigNumber, bigOne )) {
        SetFailed( "LL_EQ", "bigNumber EQ one");
    }

    /* test not equal */
    if ( LL_NE( bigZero, bigZero )) {
        SetFailed( "LL_NE", "0 NE 0");
    }

    if ( LL_NE( bigOne, bigOne )) {
        SetFailed( "LL_NE", "1 NE 1");
    }

    if ( LL_NE( bigMinusOne, bigMinusOne )) {
        SetFailed( "LL_NE", "-1 NE -1");
    }

    if ( LL_NE( bigNumber, bigNumber )) {
        SetFailed( "LL_NE", "n NE n");
    }

    if ( LL_NE( bigMinusNumber, bigMinusNumber )) {
        SetFailed( "LL_NE", "-n NE -n");
    }

    if ( !LL_NE( bigZero, bigOne)) {
        SetFailed( "LL_NE", "0 NE 1");
    }

    if ( !LL_NE( bigOne, bigMinusNumber)) {
        SetFailed( "LL_NE", "1 NE -n");
    }

    /* Greater than or equal to zero */
    if ( !LL_GE_ZERO( bigZero )) {
        SetFailed( "LL_GE_ZERO", "0");
    }

    if ( !LL_GE_ZERO( bigOne )) {
        SetFailed( "LL_GE_ZERO", "1");
    }

    if ( !LL_GE_ZERO( bigNumber )) {
        SetFailed( "LL_GE_ZERO", "n");
    }

    if ( LL_GE_ZERO( bigMinusOne )) {
        SetFailed( "LL_GE_ZERO", "-1");
    }

    if ( LL_GE_ZERO( bigMinusNumber )) {
        SetFailed( "LL_GE_ZERO", "-n");
    }

    /* Algebraic Compare two values */
    if ( !LL_CMP( bigZero, ==, bigZero )) {
        SetFailed( "LL_CMP", "0 == 0");
    }

    if ( LL_CMP( bigZero, >, bigZero )) {
        SetFailed( "LL_CMP", "0 > 0");
    }

    if ( LL_CMP( bigZero, <, bigZero )) {
        SetFailed( "LL_CMP", "0 < 0");
    }

    if ( LL_CMP( bigNumber, <, bigOne )) {
        SetFailed( "LL_CMP", "n < 1");
    }

    if ( !LL_CMP( bigNumber, >, bigOne )) {
        SetFailed( "LL_CMP", "n <= 1");
    }

    if ( LL_CMP( bigOne, >, bigNumber )) {
        SetFailed( "LL_CMP", "1 > n");
    }

    if ( LL_CMP( bigMinusNumber, >, bigNumber )) {
        SetFailed( "LL_CMP", "-n > n");
    }

    if ( LL_CMP( bigNumber, !=, bigNumber)) {
        SetFailed( "LL_CMP", "n != n");
    }

    if ( !LL_CMP( bigMinusOne, >, bigMinusTwo )) {
        SetFailed( "LL_CMP", "-1 <= -2");
    }

    if ( !LL_CMP( bigMaxInt32, <, big2To31 )) {
        SetFailed( "LL_CMP", "Max 32-bit signed int >= 2^31");
    }

    /* Two positive numbers */
    if ( !LL_CMP( bigInt0, <=, bigInt0 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( !LL_CMP( bigInt0, <=, bigInt1 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( LL_CMP( bigInt0, <=, bigInt2 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( !LL_CMP( bigInt0, <=, bigInt3 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( !LL_CMP( bigInt0, <=, bigInt4 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( !LL_CMP( bigInt0, <=, bigInt5 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    /* Two negative numbers */
    if ( !LL_CMP( bigInt6, <=, bigInt6 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( !LL_CMP( bigInt6, <=, bigInt7 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( LL_CMP( bigInt6, <=, bigInt8 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( !LL_CMP( bigInt6, <=, bigInt9 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( !LL_CMP( bigInt6, <=, bigInt10 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( !LL_CMP( bigInt6, <=, bigInt11 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    /* One positive, one negative */
    if ( LL_CMP( bigInt0, <=, bigInt6 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( LL_CMP( bigInt0, <=, bigInt7 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    if ( LL_CMP( bigInt0, <=, bigInt8 )) {
        SetFailed( "LL_CMP", "LL_CMP(<=) failed");
    }

    /* Bitwise Compare two numbers */
    if ( !LL_UCMP( bigZero, ==, bigZero )) {
        SetFailed( "LL_UCMP", "0 == 0");
    }

    if ( LL_UCMP( bigZero, >, bigZero )) {
        SetFailed( "LL_UCMP", "0 > 0");
    }

    if ( LL_UCMP( bigZero, <, bigZero )) {
        SetFailed( "LL_UCMP", "0 < 0");
    }

    if ( LL_UCMP( bigNumber, <, bigOne )) {
        SetFailed( "LL_UCMP", "n < 1");
    }

    if ( !LL_UCMP( bigNumber, >, bigOne )) {
        SetFailed( "LL_UCMP", "n < 1");
    }

    if ( LL_UCMP( bigOne, >, bigNumber )) {
        SetFailed( "LL_UCMP", "1 > n");
    }

    if ( LL_UCMP( bigMinusNumber, <, bigNumber )) {
        SetFailed( "LL_UCMP", "-n < n");
    }

    /* Two positive numbers */
    if ( !LL_UCMP( bigInt0, <=, bigInt0 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt0, <=, bigInt1 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( LL_UCMP( bigInt0, <=, bigInt2 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt0, <=, bigInt3 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt0, <=, bigInt4 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt0, <=, bigInt5 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    /* Two negative numbers */
    if ( !LL_UCMP( bigInt6, <=, bigInt6 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt6, <=, bigInt7 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( LL_UCMP( bigInt6, <=, bigInt8 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt6, <=, bigInt9 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt6, <=, bigInt10 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt6, <=, bigInt11 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    /* One positive, one negative */
    if ( !LL_UCMP( bigInt0, <=, bigInt6 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt0, <=, bigInt7 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    if ( !LL_UCMP( bigInt0, <=, bigInt8 )) {
        SetFailed( "LL_UCMP", "LL_UCMP(<=) failed");
    }

    return;
}

/*
**  TestLogicalOperations() -- Tests for AND, OR, ...
**
*/
static void
TestLogicalOperations( void )
{
    PRUint64    result, result2;

    ReportProgress("Testing Logical Operations\n");

    /* Test AND */
    LL_AND( result, bigZero, bigZero );
    if ( !LL_IS_ZERO( result )) {
        ResultFailed( "LL_AND", "0 & 0", bigZero, result );
    }

    LL_AND( result, bigOne, bigOne );
    if ( LL_IS_ZERO( result )) {
        ResultFailed( "LL_AND", "1 & 1", bigOne, result );
    }

    LL_AND( result, bigZero, bigOne );
    if ( !LL_IS_ZERO( result )) {
        ResultFailed( "LL_AND", "1 & 1", bigZero, result );
    }

    LL_AND( result, bigMinusOne, bigMinusOne );
    if ( !LL_UCMP( result, ==, bigMinusOne )) {
        ResultFailed( "LL_AND", "-1 & -1", bigMinusOne, result );
    }

    /* test OR */
    LL_OR( result, bigZero, bigZero );
    if ( !LL_IS_ZERO( result )) {
        ResultFailed( "LL_OR", "0 | 1", bigZero, result);
    }

    LL_OR( result, bigZero, bigOne );
    if ( LL_IS_ZERO( result )) {
        ResultFailed( "LL_OR", "0 | 1", bigOne, result );
    }

    LL_OR( result, bigZero, bigMinusNumber );
    if ( !LL_UCMP( result, ==, bigMinusNumber )) {
        ResultFailed( "LL_OR", "0 | -n", bigMinusNumber, result);
    }

    LL_OR( result, bigMinusNumber, bigZero );
    if ( !LL_UCMP( result, ==, bigMinusNumber )) {
        ResultFailed( "LL_OR", "-n | 0", bigMinusNumber, result );
    }

    /* test XOR */
    LL_XOR( result, bigZero, bigZero );
    if ( LL_UCMP( result, !=, bigZero )) {
        ResultFailed( "LL_XOR", "0 ^ 0", bigZero, result);
    }

    LL_XOR( result, bigOne, bigZero );
    if ( LL_UCMP( result, !=, bigOne )) {
        ResultFailed( "LL_XOR", "1 ^ 0", bigZero, result );
    }

    LL_XOR( result, bigMinusNumber, bigZero );
    if ( LL_UCMP( result, !=, bigMinusNumber )) {
        ResultFailed( "LL_XOR", "-n ^ 0", bigMinusNumber, result );
    }

    LL_XOR( result, bigMinusNumber, bigMinusNumber );
    if ( LL_UCMP( result, !=, bigZero )) {
        ResultFailed( "LL_XOR", "-n ^ -n", bigMinusNumber, result);
    }

    /* test OR2.  */
    result = bigZero;
    LL_OR2( result, bigOne );
    if ( LL_UCMP( result, !=, bigOne )) {
        ResultFailed( "LL_OR2", "(r=0) |= 1", bigOne, result);
    }

    result = bigOne;
    LL_OR2( result, bigNumber );
    if ( LL_UCMP( result, !=, bigNumber )) {
        ResultFailed( "LL_OR2", "(r=1) |= n", bigNumber, result);
    }

    result = bigMinusNumber;
    LL_OR2( result, bigMinusNumber );
    if ( LL_UCMP( result, !=, bigMinusNumber )) {
        ResultFailed( "LL_OR2", "(r=-n) |= -n", bigMinusNumber, result);
    }

    /* test NOT */
    LL_NOT( result, bigMinusNumber);
    LL_NOT( result2, result);
    if ( LL_UCMP( result2, !=, bigMinusNumber )) {
        ResultFailed( "LL_NOT", "r != ~(~-n)", bigMinusNumber, result);
    }

    /* test Negation */
    LL_NEG( result, bigMinusNumber );
    LL_NEG( result2, result );
    if ( LL_CMP( result2, !=, bigMinusNumber )) {
        ResultFailed( "LL_NEG", "r != -(-(-n))", bigMinusNumber, result);
    }

    return;
}



/*
**  TestConversion() -- Test Conversion Operations
**
*/
static void
TestConversion( void )
{
    PRInt64     result;
    PRInt64     resultU;
    PRInt32     result32;
    PRUint32    resultU32;
    float       resultF;
    PRFloat64   resultD;

    ReportProgress("Testing Conversion Operations\n");

    /* LL_L2I  -- Convert to signed 32bit */
    LL_L2I(result32, bigOne );
    if ( result32 != one ) {
        SetFailed( "LL_L2I", "r != 1");
    }

    LL_L2I(result32, bigMinusOne );
    if ( result32 != minusOne ) {
        SetFailed( "LL_L2I", "r != -1");
    }

    /* LL_L2UI -- Convert 64bit to unsigned 32bit */
    LL_L2UI( resultU32, bigMinusOne );
    if ( resultU32 != (PRUint32) minusOne ) {
        SetFailed( "LL_L2UI", "r != -1");
    }

    LL_L2UI( resultU32, bigOne );
    if ( resultU32 != (PRUint32) one ) {
        SetFailed( "LL_L2UI", "r != 1");
    }

    /* LL_L2F  -- Convert to 32bit floating point */
    LL_L2F( resultF, bigOne );
    if ( resultF != 1.0 ) {
        SetFailed( "LL_L2F", "r != 1.0");
    }

    LL_L2F( resultF, bigMinusOne );
    if ( resultF != -1.0 ) {
        SetFailed( "LL_L2F", "r != 1.0");
    }

    /* LL_L2D  -- Convert to 64bit floating point */
    LL_L2D( resultD, bigOne );
    if ( resultD != 1.0L ) {
        SetFailed( "LL_L2D", "r != 1.0");
    }

    LL_L2D( resultD, bigMinusOne );
    if ( resultD != -1.0L ) {
        SetFailed( "LL_L2D", "r != -1.0");
    }

    /* LL_I2L  -- Convert 32bit signed to 64bit signed */
    LL_I2L( result, one );
    if ( LL_CMP(result, !=, bigOne )) {
        SetFailed( "LL_I2L", "r != 1");
    }

    LL_I2L( result, minusOne );
    if ( LL_CMP(result, !=, bigMinusOne )) {
        SetFailed( "LL_I2L", "r != -1");
    }

    /* LL_UI2L -- Convert 32bit unsigned to 64bit unsigned */
    LL_UI2L( resultU, (PRUint32) one );
    if ( LL_CMP(resultU, !=, bigOne )) {
        SetFailed( "LL_UI2L", "r != 1");
    }

    /* [lth.] This did not behave as expected, but it is correct
    */
    LL_UI2L( resultU, (PRUint32) minusOne );
    if ( LL_CMP(resultU, !=, bigZeroFox )) {
        ResultFailed( "LL_UI2L", "r != -1", bigZeroFox, resultU);
    }

    /* LL_F2L  -- Convert 32bit float to 64bit signed */
    LL_F2L( result, 1.0 );
    if ( LL_CMP(result, !=, bigOne )) {
        SetFailed( "LL_F2L", "r != 1");
    }

    LL_F2L( result, -1.0 );
    if ( LL_CMP(result, !=, bigMinusOne )) {
        SetFailed( "LL_F2L", "r != -1");
    }

    /* LL_D2L  -- Convert 64bit Float to 64bit signed */
    LL_D2L( result, 1.0L );
    if ( LL_CMP(result, !=, bigOne )) {
        SetFailed( "LL_D2L", "r != 1");
    }

    LL_D2L( result, -1.0L );
    if ( LL_CMP(result, !=, bigMinusOne )) {
        SetFailed( "LL_D2L", "r != -1");
    }

    return;
}

static void ShiftCompileOnly()
{
    /*
    ** This function is only compiled, never called.
    ** The real test is to see if it compiles w/o
    ** warnings. This is no small feat, by the way.
    */
    PRInt64 ia, ib;
    PRUint64 ua, ub;
    LL_SHR(ia, ib, 32);
    LL_SHL(ia, ib, 32);

    LL_USHR(ua, ub, 32);
    LL_ISHL(ia, 49, 32);

}  /* ShiftCompileOnly */


/*
**  TestShift() -- Test Shifting Operations
**
*/
static void
TestShift( void )
{
    static const PRInt64 largeTwoZero = LL_INIT( 0x00000002, 0x00000000 );
    PRInt64     result;
    PRUint64    resultU;

    ReportProgress("Testing Shifting Operations\n");

    /* LL_SHL  -- Shift left algebraic */
    LL_SHL( result, bigOne, one );
    if ( LL_CMP( result, !=, bigTwo )) {
        ResultFailed( "LL_SHL", "r != 2", bigOne, result );
    }

    LL_SHL( result, bigTwo, thirtyTwo );
    if ( LL_CMP( result, !=, largeTwoZero )) {
        ResultFailed( "LL_SHL", "r != twoZero", largeTwoZero, result);
    }

    /* LL_SHR  -- Shift right algebraic */
    LL_SHR( result, bigFoxZero, thirtyTwo );
    if ( LL_CMP( result, !=, bigMinusOne )) {
        ResultFailed( "LL_SHR", "r != -1", bigMinusOne, result);
    }

    LL_SHR( result, bigTwo, one );
    if ( LL_CMP( result, !=, bigOne )) {
        ResultFailed( "LL_SHR", "r != 1", bigOne, result);
    }

    LL_SHR( result, bigFoxFox, thirtyTwo );
    if ( LL_CMP( result, !=, bigMinusOne )) {
        ResultFailed( "LL_SHR", "r != -1 (was ff,ff)", bigMinusOne, result);
    }

    /* LL_USHR -- Logical shift right */
    LL_USHR( resultU, bigZeroFox, thirtyTwo );
    if ( LL_UCMP( resultU, !=, bigZero )) {
        ResultFailed( "LL_USHR", "r != 0 ", bigZero, result);
    }

    LL_USHR( resultU, bigFoxFox, thirtyTwo );
    if ( LL_UCMP( resultU, !=, bigZeroFox )) {
        ResultFailed( "LL_USHR", "r != 0 ", bigZeroFox, result);
    }

    /* LL_ISHL -- Shift a 32bit integer into a 64bit result */
    LL_ISHL( resultU, minusOne, thirtyTwo );
    if ( LL_UCMP( resultU, !=, bigFoxZero )) {
        ResultFailed( "LL_ISHL", "r != ff,00 ", bigFoxZero, result);
    }

    LL_ISHL( resultU, one, sixtyThree );
    if ( LL_UCMP( resultU, !=, bigEightZero )) {
        ResultFailed( "LL_ISHL", "r != 80,00 ", bigEightZero, result);
    }

    LL_ISHL( resultU, one, sixteen );
    if ( LL_UCMP( resultU, !=, big64K )) {
        ResultFailed( "LL_ISHL", "r != 64K ", big64K, resultU);
    }

    return;
}


/*
**  TestArithmetic() -- Test arithmetic operations.
**
*/
static void
TestArithmetic( void )
{
    PRInt64 largeVal          = LL_INIT( 0x00000001, 0xffffffff );
    PRInt64 largeValPlusOne   = LL_INIT( 0x00000002, 0x00000000 );
    PRInt64 largeValTimesTwo  = LL_INIT( 0x00000003, 0xfffffffe );
    PRInt64 largeMultCand     = LL_INIT( 0x00000000, 0x7fffffff );
    PRInt64 largeMinusMultCand = LL_INIT( 0xffffffff, 0x10000001 );
    PRInt64 largeMultCandx64K = LL_INIT( 0x00007fff, 0xffff0000 );
    PRInt64 largeNumSHL5      = LL_INIT( 0x0000001f, 0xffffffe0 );
    PRInt64 result, result2;

    /* Addition */
    LL_ADD( result, bigOne, bigOne );
    if ( LL_CMP( result, !=, bigTwo )) {
        ResultFailed( "LL_ADD", "r != 1 + 1", bigTwo, result);
    }

    LL_ADD( result, bigMinusOne, bigOne );
    if ( LL_CMP( result, !=, bigZero )) {
        ResultFailed( "LL_ADD", "r != -1 + 1", bigOne, result);
    }

    LL_ADD( result, largeVal, bigOne );
    if ( LL_CMP( result, !=, largeValPlusOne )) {
        ResultFailed( "LL_ADD", "lVP1 != lV + 1", largeValPlusOne, result);
    }

    /* Subtraction */
    LL_SUB( result, bigOne, bigOne );
    if ( LL_CMP( result, !=, bigZero )) {
        ResultFailed( "LL_SUB", "r != 1 - 1", bigZero, result);
    }

    LL_SUB( result, bigTwo, bigOne );
    if ( LL_CMP( result, !=, bigOne )) {
        ResultFailed( "LL_SUB", "r != 2 - 1", bigOne, result);
    }

    LL_SUB( result, largeValPlusOne, bigOne );
    if ( LL_CMP( result, !=, largeVal )) {
        ResultFailed( "LL_SUB", "r != lVP1 - 1", largeVal, result);
    }


    /* Multiply */
    LL_MUL( result, largeVal, bigTwo );
    if ( LL_CMP( result, !=, largeValTimesTwo )) {
        ResultFailed( "LL_MUL", "r != lV*2", largeValTimesTwo, result);
    }

    LL_MUL( result, largeMultCand, big64K );
    if ( LL_CMP( result, !=, largeMultCandx64K )) {
        ResultFailed( "LL_MUL", "r != lV*64K", largeMultCandx64K, result);
    }

    LL_NEG( result2, largeMultCand );
    LL_MUL( result, largeMultCand, bigMinusOne );
    if ( LL_CMP( result, !=, result2  )) {
        ResultFailed( "LL_MUL", "r != -lMC", result2, result);
    }

    LL_SHL( result2, bigZeroFox, 5);
    LL_MUL( result, bigZeroFox, bigThirtyTwo );
    if ( LL_CMP( result, !=, largeNumSHL5  )) {
        ResultFailed( "LL_MUL", "r != 0f<<5", largeNumSHL5, result );
    }



    /* LL_DIV() Division */
    LL_DIV( result, bigOne, bigOne);
    if ( LL_CMP( result, !=, bigOne )) {
        ResultFailed( "LL_DIV", "1 != 1", bigOne, result);
    }

    LL_DIV( result, bigNumber, bigOne );
    if ( LL_CMP( result, !=, bigNumber  )) {
        ResultFailed( "LL_DIV", "r != n / 1", bigNumber, result);
    }

    LL_DIV( result, bigNumber, bigMinusOne );
    if ( LL_CMP( result, !=, bigMinusNumber  )) {
        ResultFailed( "LL_DIV", "r != n / -1", bigMinusNumber, result);
    }

    LL_DIV( result, bigMinusNumber, bigMinusOne );
    if ( LL_CMP( result, !=, bigNumber  )) {
        ResultFailed( "LL_DIV", "r != -n / -1", bigNumber, result);
    }

    LL_SHL( result2, bigZeroFox, 5 );
    LL_DIV( result, result2, bigOne );
    if ( LL_CMP( result, !=, result2  )) {
        ResultFailed( "LL_DIV", "0f<<5 != 0f<<5", result2, result);
    }

    LL_SHL( result2, bigZeroFox, 5 );
    LL_NEG( result2, result2 );
    LL_DIV( result, result2, bigOne );
    if ( LL_CMP( result, !=, result2  )) {
        ResultFailed( "LL_DIV", "-0f<<5 != -0f<<5", result2, result);
    }

    LL_SHL( result2, bigZeroFox, 17 );
    LL_DIV( result, result2, bigMinusOne );
    LL_NEG( result2, result2 );
    if ( LL_CMP( result, !=, result2  )) {
        ResultFailed( "LL_DIV", "-0f<<17 != -0f<<17", result2, result);
    }


    /* LL_MOD() Modulo Division */
    LL_ADD( result2, bigThirtyTwo, bigOne );
    LL_MOD( result, result2, bigSixTeen );
    if ( LL_CMP( result, !=, bigOne )) {
        ResultFailed( "LL_MOD", "r != 1", bigSixTeen, result);
    }


    LL_MUL( result2, bigZeroFox, bigThirtyTwo );
    LL_ADD( result2, result2, bigSixTeen);
    LL_MOD( result, result2, bigThirtyTwo );
    if ( LL_CMP( result, !=, bigSixTeen )) {
        ResultFailed( "LL_MOD", "r != 16", bigSixTeen, result);
    }

    /* LL_UDIVMOD */
    LL_DIV( result, bigOne, bigOne);
    if ( LL_CMP( result, !=, bigOne )) {
        ResultFailed( "LL_DIV", "r != 16", bigSixTeen, result);
    }


    return;
}

static void TestWellknowns(void)
{
    PRInt64 max = LL_MAXINT, min = LL_MININT, zero = LL_ZERO;
    PRInt64 mmax = LL_MaxInt(), mmin = LL_MinInt(), mzero = LL_Zero();
    if (LL_NE(max, mmax)) {
        ResultFailed( "max, mmax", "max != mmax", max, mmax);
    }
    if (LL_NE(min, mmin)) {
        ResultFailed( "min, mmin", "min != mmin", max, mmin);
    }
    if (LL_NE(zero, mzero)) {
        ResultFailed( "zero, mzero", "zero != mzero", zero, mzero);
    }
}  /* TestWellknowns */

/*
** Initialize() -- Initialize the test case
**
** Parse command line options
**
*/
static PRIntn
Initialize( PRIntn argc, char **argv )
{
    PLOptState *opt = PL_CreateOptState(argc, argv, "dvh");
    PLOptStatus os;

    /*
    ** Parse command line options
    */
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option)
        {
            case 'd':  /* set debug mode */
                debugMode = PR_TRUE;
                break;

            case 'v':  /* set verbose mode */
                verboseMode = PR_TRUE;
                debugMode = PR_TRUE;
                break;

            case 'h':  /* user wants some guidance */
            default:
                PR_fprintf(output, "You get help.\n");
                return(1);
        }
    }
    PL_DestroyOptState(opt);
    return(0);
}

int main(int argc, char **argv)
{
    PR_STDIO_INIT();
    output = PR_GetSpecialFD(PR_StandardError);

    if ( Initialize( argc, argv )) {
        return(1);
    }

    TestAssignment();
    TestComparisons();
    TestLogicalOperations();
    TestConversion();
    TestShift();
    TestArithmetic();
    TestWellknowns();

    /*
    ** That's all folks!
    */
    if ( failedAlready )
    {
        PR_fprintf(output, "FAIL\n"); \
    }
    else
    {
        PR_fprintf(output, "PASS\n"); \
    }
    return failedAlready;
} /* end main() */

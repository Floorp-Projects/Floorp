/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Benoit Jacob <bjacob@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "CheckedInt.h"
#include <iostream>

namespace CheckedInt_test {

using namespace mozilla::CheckedInt_internal;
using mozilla::CheckedInt;

int g_tests_passed = 0;
int g_tests_failed = 0;

void verify_impl_function(bool x, bool expected,
                          const char* file, int line,
                          int T_size, bool T_is_signed)
{
    if (x == expected) {
        g_tests_passed++;
    } else {
        g_tests_failed++;
        std::cerr << "Test failed at " << file << ":" << line;
        std::cerr << " with T a ";
        if(T_is_signed)
            std::cerr << "signed";
        else
            std::cerr << "unsigned";
        std::cerr << " " << CHAR_BIT*T_size << "-bit integer type" << std::endl;
    }
}

#define VERIFY_IMPL(x, expected) \
    verify_impl_function((x), (expected), __FILE__, __LINE__, sizeof(T), integer_traits<T>::is_signed)

#define VERIFY(x)            VERIFY_IMPL(x, true)
#define VERIFY_IS_FALSE(x)   VERIFY_IMPL(x, false)
#define VERIFY_IS_VALID(x)   VERIFY_IMPL((x).valid(), true)
#define VERIFY_IS_INVALID(x) VERIFY_IMPL((x).valid(), false)
#define VERIFY_IS_VALID_IF(x,condition) VERIFY_IMPL((x).valid(), (condition))

template<typename T, unsigned int size = sizeof(T)>
struct test_twice_bigger_type
{
    static void run()
    {
        VERIFY(integer_traits<T>::twice_bigger_type_is_supported);
        VERIFY(sizeof(typename integer_traits<T>::twice_bigger_type)
                    == 2 * sizeof(T));
        VERIFY(bool(integer_traits<
                    typename integer_traits<T>::twice_bigger_type
                >::is_signed) == bool(integer_traits<T>::is_signed));
    }
};

template<typename T>
struct test_twice_bigger_type<T, 8>
{
    static void run()
    {
        VERIFY_IS_FALSE(integer_traits<T>::twice_bigger_type_is_supported);
    }
};


template<typename T>
void test()
{
    static bool already_run = false;
    if (already_run) {
        g_tests_failed++;
        std::cerr << "You already tested this type. Copy/paste typo??" << std::endl;
        return;
    }
    already_run = true;

    VERIFY(integer_traits<T>::is_supported);
    VERIFY(integer_traits<T>::size == sizeof(T));
    enum{ is_signed = integer_traits<T>::is_signed };
    VERIFY(bool(is_signed) == !bool(T(-1) > T(0)));

    test_twice_bigger_type<T>::run();

    typedef typename integer_traits<T>::unsigned_type unsigned_T;

    VERIFY(sizeof(unsigned_T) == sizeof(T));
    VERIFY(integer_traits<unsigned_T>::is_signed == false);

    CheckedInt<T> max_value(integer_traits<T>::max_value());
    CheckedInt<T> min_value(integer_traits<T>::min_value());

    // check min_value() and max_value(), since they are custom implementations and a mistake there
    // could potentially NOT be caught by any other tests... while making everything wrong!

    T bit = 1;
    for(unsigned int i = 0; i < sizeof(T) * CHAR_BIT - 1; i++)
    {
        VERIFY((min_value.value() & bit) == 0);
        bit <<= 1;
    }
    VERIFY((min_value.value() & bit) == (is_signed ? bit : T(0)));
    VERIFY(max_value.value() == T(~(min_value.value())));

    CheckedInt<T> zero(0);
    CheckedInt<T> one(1);
    CheckedInt<T> two(2);
    CheckedInt<T> three(3);
    CheckedInt<T> four(4);

    /* addition / substraction checks */

    VERIFY_IS_VALID(zero+zero);
    VERIFY(zero+zero == zero);
    VERIFY_IS_FALSE(zero+zero == one); // check that == doesn't always return true
    VERIFY_IS_VALID(zero+one);
    VERIFY(zero+one == one);
    VERIFY_IS_VALID(one+one);
    VERIFY(one+one == two);

    CheckedInt<T> max_value_minus_one = max_value - one;
    CheckedInt<T> max_value_minus_two = max_value - two;
    VERIFY_IS_VALID(max_value_minus_one);
    VERIFY_IS_VALID(max_value_minus_two);
    VERIFY_IS_VALID(max_value_minus_one + one);
    VERIFY_IS_VALID(max_value_minus_two + one);
    VERIFY_IS_VALID(max_value_minus_two + two);
    VERIFY(max_value_minus_one + one == max_value);
    VERIFY(max_value_minus_two + one == max_value_minus_one);
    VERIFY(max_value_minus_two + two == max_value);

    VERIFY_IS_VALID(max_value + zero);
    VERIFY_IS_VALID(max_value - zero);
    VERIFY_IS_INVALID(max_value + one);
    VERIFY_IS_INVALID(max_value + two);
    VERIFY_IS_INVALID(max_value + max_value_minus_one);
    VERIFY_IS_INVALID(max_value + max_value);

    CheckedInt<T> min_value_plus_one = min_value + one;
    CheckedInt<T> min_value_plus_two = min_value + two;
    VERIFY_IS_VALID(min_value_plus_one);
    VERIFY_IS_VALID(min_value_plus_two);
    VERIFY_IS_VALID(min_value_plus_one - one);
    VERIFY_IS_VALID(min_value_plus_two - one);
    VERIFY_IS_VALID(min_value_plus_two - two);
    VERIFY(min_value_plus_one - one == min_value);
    VERIFY(min_value_plus_two - one == min_value_plus_one);
    VERIFY(min_value_plus_two - two == min_value);

    CheckedInt<T> min_value_minus_one = min_value - one;
    VERIFY_IS_VALID(min_value + zero);
    VERIFY_IS_VALID(min_value - zero);
    VERIFY_IS_INVALID(min_value - one);
    VERIFY_IS_INVALID(min_value - two);
    VERIFY_IS_INVALID(min_value - min_value_minus_one);
    VERIFY_IS_VALID(min_value - min_value);

    CheckedInt<T> max_value_over_two = max_value / two;
    VERIFY_IS_VALID(max_value_over_two + max_value_over_two);
    VERIFY_IS_VALID(max_value_over_two + one);
    VERIFY((max_value_over_two + one) - one == max_value_over_two);
    VERIFY_IS_VALID(max_value_over_two - max_value_over_two);
    VERIFY(max_value_over_two - max_value_over_two == zero);

    CheckedInt<T> min_value_over_two = min_value / two;
    VERIFY_IS_VALID(min_value_over_two + min_value_over_two);
    VERIFY_IS_VALID(min_value_over_two + one);
    VERIFY((min_value_over_two + one) - one == min_value_over_two);
    VERIFY_IS_VALID(min_value_over_two - min_value_over_two);
    VERIFY(min_value_over_two - min_value_over_two == zero);

    VERIFY_IS_INVALID(min_value - one);
    VERIFY_IS_INVALID(min_value - two);

    if (is_signed) {
        VERIFY_IS_INVALID(min_value + min_value);
        VERIFY_IS_INVALID(min_value_over_two + min_value_over_two + min_value_over_two);
        VERIFY_IS_INVALID(zero - min_value + min_value);
        VERIFY_IS_INVALID(one - min_value + min_value);
    }

    /* unary operator- checks */

    CheckedInt<T> neg_one = -one;
    CheckedInt<T> neg_two = -two;

    if (is_signed) {
        VERIFY_IS_VALID(-max_value);
        VERIFY_IS_VALID(-max_value - one);
        VERIFY_IS_VALID(neg_one);
        VERIFY_IS_VALID(-max_value + neg_one);
        VERIFY_IS_VALID(neg_one + one);
        VERIFY(neg_one + one == zero);
        VERIFY_IS_VALID(neg_two);
        VERIFY_IS_VALID(neg_one + neg_one);
        VERIFY(neg_one + neg_one == neg_two);
    } else {
        VERIFY_IS_INVALID(neg_one);
    }

    /* multiplication checks */

    VERIFY_IS_VALID(zero*zero);
    VERIFY(zero*zero == zero);
    VERIFY_IS_VALID(zero*one);
    VERIFY(zero*one == zero);
    VERIFY_IS_VALID(one*zero);
    VERIFY(one*zero == zero);
    VERIFY_IS_VALID(one*one);
    VERIFY(one*one == one);
    VERIFY_IS_VALID(one*three);
    VERIFY(one*three == three);
    VERIFY_IS_VALID(two*two);
    VERIFY(two*two == four);

    VERIFY_IS_INVALID(max_value * max_value);
    VERIFY_IS_INVALID(max_value_over_two * max_value);
    VERIFY_IS_INVALID(max_value_over_two * max_value_over_two);

    CheckedInt<T> max_value_approx_sqrt(T(T(1) << (CHAR_BIT*sizeof(T)/2)));

    VERIFY_IS_VALID(max_value_approx_sqrt);
    VERIFY_IS_VALID(max_value_approx_sqrt * two);
    VERIFY_IS_INVALID(max_value_approx_sqrt * max_value_approx_sqrt);
    VERIFY_IS_INVALID(max_value_approx_sqrt * max_value_approx_sqrt * max_value_approx_sqrt);

    if (is_signed) {
        VERIFY_IS_INVALID(min_value * min_value);
        VERIFY_IS_INVALID(min_value_over_two * min_value);
        VERIFY_IS_INVALID(min_value_over_two * min_value_over_two);

        CheckedInt<T> min_value_approx_sqrt = -max_value_approx_sqrt;

        VERIFY_IS_VALID(min_value_approx_sqrt);
        VERIFY_IS_VALID(min_value_approx_sqrt * two);
        VERIFY_IS_INVALID(min_value_approx_sqrt * max_value_approx_sqrt);
        VERIFY_IS_INVALID(min_value_approx_sqrt * min_value_approx_sqrt);
    }

    // make sure to check all 4 paths in signed multiplication validity check.
    // test positive * positive
    VERIFY_IS_VALID(max_value * one);
    VERIFY(max_value * one == max_value);
    VERIFY_IS_INVALID(max_value * two);
    VERIFY_IS_VALID(max_value_over_two * two);
    VERIFY((max_value_over_two + max_value_over_two) == (max_value_over_two * two));

    if (is_signed) {
        // test positive * negative
        VERIFY_IS_VALID(max_value * neg_one);
        VERIFY_IS_VALID(-max_value);
        VERIFY(max_value * neg_one == -max_value);
        VERIFY_IS_VALID(one * min_value);
        VERIFY_IS_INVALID(max_value * neg_two);
        VERIFY_IS_VALID(max_value_over_two * neg_two);
        VERIFY_IS_VALID(two * min_value_over_two);
        VERIFY_IS_VALID((max_value_over_two + one) * neg_two);
        VERIFY_IS_INVALID((max_value_over_two + two) * neg_two);
        VERIFY_IS_INVALID(two * (min_value_over_two - one));

        // test negative * positive
        VERIFY_IS_VALID(min_value * one);
        VERIFY_IS_VALID(min_value_plus_one * one);
        VERIFY_IS_INVALID(min_value * two);
        VERIFY_IS_VALID(min_value_over_two * two);
        VERIFY(min_value_over_two * two == min_value);
        VERIFY_IS_INVALID((min_value_over_two - one) * neg_two);
        VERIFY_IS_INVALID(neg_two * max_value);
        VERIFY_IS_VALID(min_value_over_two * two);
        VERIFY(min_value_over_two * two == min_value);
        VERIFY_IS_VALID(neg_two * max_value_over_two);
        VERIFY_IS_INVALID((min_value_over_two - one) * two);
        VERIFY_IS_VALID(neg_two * (max_value_over_two + one));
        VERIFY_IS_INVALID(neg_two * (max_value_over_two + two));

        // test negative * negative
        VERIFY_IS_INVALID(min_value * neg_one);
        VERIFY_IS_VALID(min_value_plus_one * neg_one);
        VERIFY(min_value_plus_one * neg_one == max_value);
        VERIFY_IS_INVALID(min_value * neg_two);
        VERIFY_IS_INVALID(min_value_over_two * neg_two);
        VERIFY_IS_INVALID(neg_one * min_value);
        VERIFY_IS_VALID(neg_one * min_value_plus_one);
        VERIFY(neg_one * min_value_plus_one == max_value);
        VERIFY_IS_INVALID(neg_two * min_value);
        VERIFY_IS_INVALID(neg_two * min_value_over_two);
    }

    /* division checks */

    VERIFY_IS_VALID(one / one);
    VERIFY(one / one == one);
    VERIFY_IS_VALID(three / three);
    VERIFY(three / three == one);
    VERIFY_IS_VALID(four / two);
    VERIFY(four / two == two);
    VERIFY((four*three)/four == three);

    // check that div by zero is invalid
    VERIFY_IS_INVALID(zero / zero);
    VERIFY_IS_INVALID(one / zero);
    VERIFY_IS_INVALID(two / zero);
    VERIFY_IS_INVALID(neg_one / zero);
    VERIFY_IS_INVALID(max_value / zero);
    VERIFY_IS_INVALID(min_value / zero);

    if (is_signed) {
        // check that min_value / -1 is invalid
        VERIFY_IS_INVALID(min_value / neg_one);

        // check that the test for div by -1 isn't banning other numerators than min_value
        VERIFY_IS_VALID(one / neg_one);
        VERIFY_IS_VALID(zero / neg_one);
        VERIFY_IS_VALID(neg_one / neg_one);
        VERIFY_IS_VALID(max_value / neg_one);
    }

    /* check that invalidity is correctly preserved by arithmetic ops */

    CheckedInt<T> some_invalid = max_value + max_value;
    VERIFY_IS_INVALID(some_invalid + zero);
    VERIFY_IS_INVALID(some_invalid - zero);
    VERIFY_IS_INVALID(zero + some_invalid);
    VERIFY_IS_INVALID(zero - some_invalid);
    VERIFY_IS_INVALID(-some_invalid);
    VERIFY_IS_INVALID(some_invalid * zero);
    VERIFY_IS_INVALID(some_invalid * one);
    VERIFY_IS_INVALID(zero * some_invalid);
    VERIFY_IS_INVALID(one * some_invalid);
    VERIFY_IS_INVALID(some_invalid / zero);
    VERIFY_IS_INVALID(some_invalid / one);
    VERIFY_IS_INVALID(zero / some_invalid);
    VERIFY_IS_INVALID(one / some_invalid);
    VERIFY_IS_INVALID(some_invalid + some_invalid);
    VERIFY_IS_INVALID(some_invalid - some_invalid);
    VERIFY_IS_INVALID(some_invalid * some_invalid);
    VERIFY_IS_INVALID(some_invalid / some_invalid);

    /* check that mixing checked integers with plain integers in expressions is allowed */

    VERIFY(one + T(2) == three);
    VERIFY(2 + one == three);
    {
        CheckedInt<T> x = one;
        x += 2;
        VERIFY(x == three);
    }
    VERIFY(two - 1 == one);
    VERIFY(2 - one == one);
    {
        CheckedInt<T> x = two;
        x -= 1;
        VERIFY(x == one);
    }
    VERIFY(one * 2 == two);
    VERIFY(2 * one == two);
    {
        CheckedInt<T> x = one;
        x *= 2;
        VERIFY(x == two);
    }
    VERIFY(four / 2 == two);
    VERIFY(4 / two == two);
    {
        CheckedInt<T> x = four;
        x /= 2;
        VERIFY(x == two);
    }

    VERIFY(one == 1);
    VERIFY(1 == one);
    VERIFY_IS_FALSE(two == 1);
    VERIFY_IS_FALSE(1 == two);
    VERIFY_IS_FALSE(some_invalid == 1);
    VERIFY_IS_FALSE(1 == some_invalid);

    /* Check that construction of CheckedInt from an integer value of a mismatched type is checked */

    #define VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(U) \
    { \
        bool is_U_signed = integer_traits<U>::is_signed; \
        VERIFY_IS_VALID(CheckedInt<T>(U(0))); \
        VERIFY_IS_VALID(CheckedInt<T>(U(1))); \
        VERIFY_IS_VALID(CheckedInt<T>(U(100))); \
        if (is_U_signed) \
            VERIFY_IS_VALID_IF(CheckedInt<T>(U(-1)), is_signed); \
        if (sizeof(U) > sizeof(T)) \
            VERIFY_IS_INVALID(CheckedInt<T>(U(integer_traits<T>::max_value())+1)); \
        VERIFY_IS_VALID_IF(CheckedInt<T>(integer_traits<U>::max_value()), \
            (sizeof(T) > sizeof(U) || ((sizeof(T) == sizeof(U)) && (is_U_signed || !is_signed)))); \
        VERIFY_IS_VALID_IF(CheckedInt<T>(integer_traits<U>::min_value()), \
            is_U_signed == false ? 1 : \
            bool(is_signed) == false ? 0 : \
            sizeof(T) >= sizeof(U)); \
    }
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(PRInt8)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(PRUint8)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(PRInt16)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(PRUint16)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(PRInt32)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(PRUint32)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(PRInt64)
    VERIFY_CONSTRUCTION_FROM_INTEGER_TYPE(PRUint64)

    /* Test increment/decrement operators */

    CheckedInt<T> x, y;
    x = one;
    y = x++;
    VERIFY(x == two);
    VERIFY(y == one);
    x = one;
    y = ++x;
    VERIFY(x == two);
    VERIFY(y == two);
    x = one;
    y = x--;
    VERIFY(x == zero);
    VERIFY(y == one);
    x = one;
    y = --x;
    VERIFY(x == zero);
    VERIFY(y == zero);
    x = max_value;
    VERIFY_IS_VALID(x++);
    x = max_value;
    VERIFY_IS_INVALID(++x);
    x = min_value;
    VERIFY_IS_VALID(x--);
    x = min_value;
    VERIFY_IS_INVALID(--x);
}

} // end namespace CheckedInt_test

int main()
{
    CheckedInt_test::test<PRInt8>();
    CheckedInt_test::test<PRUint8>();
    CheckedInt_test::test<PRInt16>();
    CheckedInt_test::test<PRUint16>();
    CheckedInt_test::test<PRInt32>();
    CheckedInt_test::test<PRUint32>();
    CheckedInt_test::test<PRInt64>();
    CheckedInt_test::test<PRUint64>();

    std::cerr << CheckedInt_test::g_tests_failed << " tests failed, "
              << CheckedInt_test::g_tests_passed << " tests passed out of "
              << CheckedInt_test::g_tests_failed + CheckedInt_test::g_tests_passed
              << " tests." << std::endl;

    return CheckedInt_test::g_tests_failed > 0;
}

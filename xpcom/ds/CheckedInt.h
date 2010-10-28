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
 *  Jeff Muizelaar <jmuizelaar@mozilla.com>
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

#ifndef mozilla_CheckedInt_h
#define mozilla_CheckedInt_h

#include "prtypes.h"

#include <climits>

namespace mozilla {

namespace CheckedInt_internal {

/* we don't want to use std::numeric_limits here because PRInt... types may not support it,
 * depending on the platform, e.g. on certain platforms they use nonstandard built-in types
 */

/*** Step 1: manually record information for all the types that we want to support
 ***/

struct unsupported_type {};

template<typename T> struct integer_type_manually_recorded_info
{
    enum { is_supported = 0 };
    typedef unsupported_type twice_bigger_type;
    typedef unsupported_type unsigned_type;
};


#define CHECKEDINT_REGISTER_SUPPORTED_TYPE(T,_twice_bigger_type,_unsigned_type)  \
template<> struct integer_type_manually_recorded_info<T>       \
{                                                              \
    enum { is_supported = 1 };                                 \
    typedef _twice_bigger_type twice_bigger_type;              \
    typedef _unsigned_type unsigned_type;                      \
    static void TYPE_NOT_SUPPORTED_BY_CheckedInt() {}          \
};

//                                 Type      Twice Bigger Type     Unsigned Type
CHECKEDINT_REGISTER_SUPPORTED_TYPE(PRInt8,   PRInt16,              PRUint8)
CHECKEDINT_REGISTER_SUPPORTED_TYPE(PRUint8,  PRUint16,             PRUint8)
CHECKEDINT_REGISTER_SUPPORTED_TYPE(PRInt16,  PRInt32,              PRUint16)
CHECKEDINT_REGISTER_SUPPORTED_TYPE(PRUint16, PRUint32,             PRUint16)
CHECKEDINT_REGISTER_SUPPORTED_TYPE(PRInt32,  PRInt64,              PRUint32)
CHECKEDINT_REGISTER_SUPPORTED_TYPE(PRUint32, PRUint64,             PRUint32)
CHECKEDINT_REGISTER_SUPPORTED_TYPE(PRInt64,  unsupported_type,     PRUint64)
CHECKEDINT_REGISTER_SUPPORTED_TYPE(PRUint64, unsupported_type,     PRUint64)


/*** Step 2: record some info about a given integer type,
 ***         including whether it is supported, whether a twice bigger integer type
 ***         is supported, what that twice bigger type is, and some stuff as found
 ***         in std::numeric_limits (which we don't use because PRInt.. types may
 ***         not support it, if they are defined directly from compiler built-in types).
 ***         We use function names min_value() and max_value() instead of min() and max()
 ***         because of stupid min/max macros in Windows headers.
 ***/

template<typename T> struct is_unsupported_type { enum { answer = 0 }; };
template<> struct is_unsupported_type<unsupported_type> { enum { answer = 1 }; };

template<typename T> struct integer_traits
{
    typedef typename integer_type_manually_recorded_info<T>::twice_bigger_type twice_bigger_type;
    typedef typename integer_type_manually_recorded_info<T>::unsigned_type unsigned_type;

    enum {
        is_supported = integer_type_manually_recorded_info<T>::is_supported,
        twice_bigger_type_is_supported
            = is_unsupported_type<
                  typename integer_type_manually_recorded_info<T>::twice_bigger_type
              >::answer ? 0 : 1,
        size = sizeof(T),
        position_of_sign_bit = CHAR_BIT * size - 1,
        is_signed = (T(-1) > T(0)) ? 0 : 1
    };

    static T min_value()
    {
        // bitwise ops may return a larger type, that's why we cast explicitly to T
        // in C++, left bit shifts on signed values is undefined by the standard unless the shifted value is representable.
        // notice that signed-to-unsigned conversions are always well-defined in the standard,
        // as the value congruent to 2^n as expected. By contrast, unsigned-to-signed is only well-defined if the value is
        // representable.
        return is_signed ? T(unsigned_type(1) << position_of_sign_bit) : T(0);
    }

    static T max_value()
    {
        return ~min_value();
    }
};

/*** Step 3: Implement the actual validity checks --- ideas taken from IntegerLib, code different.
 ***/

// bitwise ops may return a larger type, so it's good to use these inline helpers guaranteeing that
// the result is really of type T

template<typename T> inline T has_sign_bit(T x)
{
    // in C++, right bit shifts on negative values is undefined by the standard.
    // notice that signed-to-unsigned conversions are always well-defined in the standard,
    // as the value congruent modulo 2^n as expected. By contrast, unsigned-to-signed is only well-defined if the value is
    // representable. Here the unsigned-to-signed conversion is OK because the value (the result of the shift) is 0 or 1.
    typedef typename integer_traits<T>::unsigned_type unsigned_T;
    return T(unsigned_T(x) >> integer_traits<T>::position_of_sign_bit);
}

template<typename T> inline T binary_complement(T x)
{
    return ~x;
}

template<typename T, typename U,
         bool is_T_signed = integer_traits<T>::is_signed,
         bool is_U_signed = integer_traits<U>::is_signed>
struct is_in_range_impl {};

template<typename T, typename U>
struct is_in_range_impl<T, U, true, true>
{
    static T run(U x)
    {
        return (x <= integer_traits<T>::max_value()) &&
               (x >= integer_traits<T>::min_value());
    }
};

template<typename T, typename U>
struct is_in_range_impl<T, U, false, false>
{
    static T run(U x)
    {
        return x <= integer_traits<T>::max_value();
    }
};

template<typename T, typename U>
struct is_in_range_impl<T, U, true, false>
{
    static T run(U x)
    {
        if (sizeof(T) > sizeof(U))
            return 1;
        else
            return x <= U(integer_traits<T>::max_value());
    }
};

template<typename T, typename U>
struct is_in_range_impl<T, U, false, true>
{
    static T run(U x)
    {
        if (sizeof(T) >= sizeof(U))
            return x >= 0;
        else
            return (x >= 0) && (x <= U(integer_traits<T>::max_value()));
    }
};

template<typename T, typename U> inline T is_in_range(U x)
{
    return is_in_range_impl<T, U>::run(x);
}

template<typename T> inline T is_add_valid(T x, T y, T result)
{
    return integer_traits<T>::is_signed ?
                        // addition is valid if the sign of x+y is equal to either that of x or that of y.
                        // Beware! These bitwise operations can return a larger integer type, if T was a
                        // small type like int8, so we explicitly cast to T.
                        has_sign_bit(binary_complement(T((result^x) & (result^y))))
                    :
                        binary_complement(x) >= y;
}

template<typename T> inline T is_sub_valid(T x, T y, T result)
{
    return integer_traits<T>::is_signed ?
                        // substraction is valid if either x and y have same sign, or x-y and x have same sign
                        has_sign_bit(binary_complement(T((result^x) & (x^y))))
                    :
                        x >= y;
}

template<typename T,
         bool is_signed =  integer_traits<T>::is_signed,
         bool twice_bigger_type_is_supported = integer_traits<T>::twice_bigger_type_is_supported>
struct is_mul_valid_impl {};

template<typename T, bool is_signed>
struct is_mul_valid_impl<T, is_signed, true>
{
    static T run(T x, T y)
    {
        typedef typename integer_traits<T>::twice_bigger_type twice_bigger_type;
        twice_bigger_type product = twice_bigger_type(x) * twice_bigger_type(y);
        return is_in_range<T>(product);
    }
};

template<typename T>
struct is_mul_valid_impl<T, true, false>
{
    static T run(T x, T y)
    {
        const T max_value = integer_traits<T>::max_value();
        const T min_value = integer_traits<T>::min_value();

        if (x == 0 || y == 0) return true;

        if (x > 0) {
            if (y > 0)
                return x <= max_value / y;
            else
                return y >= min_value / x;
        } else {
            if (y > 0)
                return x >= min_value / y;
            else
                return y >= max_value / x;
        }
    }
};

template<typename T>
struct is_mul_valid_impl<T, false, false>
{
    static T run(T x, T y)
    {
        const T max_value = integer_traits<T>::max_value();
        if (x == 0 || y == 0) return true;
        return x <= max_value / y;
    }
};

template<typename T> inline T is_mul_valid(T x, T y, T /*result not used*/)
{
    return is_mul_valid_impl<T>::run(x, y);
}

template<typename T> inline T is_div_valid(T x, T y)
{
    return integer_traits<T>::is_signed ?
                        // keep in mind that min/-1 is invalid because abs(min)>max
                        (y != 0) && (x != integer_traits<T>::min_value() || y != T(-1))
                    :
                        y != 0;
}

// this is just to shut up msvc warnings about negating unsigned ints.
template<typename T, bool is_signed = integer_traits<T>::is_signed>
struct opposite_if_signed_impl
{
    static T run(T x) { return -x; }
};
template<typename T>
struct opposite_if_signed_impl<T, false>
{
    static T run(T x) { return x; }
};
template<typename T>
inline T opposite_if_signed(T x) { return opposite_if_signed_impl<T>::run(x); }



} // end namespace CheckedInt_internal


/*** Step 4: Now define the CheckedInt class.
 ***/

/** \class CheckedInt
  * \brief Integer wrapper class checking for integer overflow and other errors
  * \param T the integer type to wrap. Can be any of PRInt8, PRUint8, PRInt16, PRUint16,
  *          PRInt32, PRUint32, PRInt64, PRUint64.
  *
  * This class implements guarded integer arithmetic. Do a computation, check that
  * valid() returns true, you then have a guarantee that no problem, such as integer overflow,
  * happened during this computation.
  *
  * The arithmetic operators in this class are guaranteed not to crash your app
  * in case of a division by zero.
  *
  * For example, suppose that you want to implement a function that computes (x+y)/z,
  * that doesn't crash if z==0, and that reports on error (divide by zero or integer overflow).
  * You could code it as follows:
    \code
    PRBool compute_x_plus_y_over_z(PRInt32 x, PRInt32 y, PRInt32 z, PRInt32 *result)
    {
        CheckedInt<PRInt32> checked_result = (CheckedInt<PRInt32>(x) + y) / z;
        *result = checked_result.value();
        return checked_result.valid();
    }
    \endcode
  *
  * Implicit conversion from plain integers to checked integers is allowed. The plain integer
  * is checked to be in range before being casted to the destination type. This means that the following
  * lines all compile, and the resulting CheckedInts are correctly detected as valid or invalid:
  * \code
    CheckedInt<PRUint8> x(1);   // 1 is of type int, is found to be in range for PRUint8, x is valid
    CheckedInt<PRUint8> x(-1);  // -1 is of type int, is found not to be in range for PRUint8, x is invalid
    CheckedInt<PRInt8> x(-1);   // -1 is of type int, is found to be in range for PRInt8, x is valid
    CheckedInt<PRInt8> x(PRInt16(1000)); // 1000 is of type PRInt16, is found not to be in range for PRInt8, x is invalid
    CheckedInt<PRInt32> x(PRUint32(3123456789)); // 3123456789 is of type PRUint32, is found not to be in range
                                             // for PRInt32, x is invalid
  * \endcode
  * Implicit conversion from
  * checked integers to plain integers is not allowed. As shown in the
  * above example, to get the value of a checked integer as a normal integer, call value().
  *
  * Arithmetic operations between checked and plain integers is allowed; the result type
  * is the type of the checked integer.
  *
  * Checked integers of different types cannot be used in the same arithmetic expression.
  *
  * There are convenience typedefs for all PR integer types, of the following form (these are just 2 examples):
    \code
    typedef CheckedInt<PRInt32> CheckedInt32;
    typedef CheckedInt<PRUint16> CheckedUint16;
    \endcode
  */
template<typename T>
class CheckedInt
{
protected:
    T mValue;
    T mIsValid; // stored as a T to limit the number of integer conversions when
                // evaluating nested arithmetic expressions.

    template<typename U>
    CheckedInt(U value, T isValid) : mValue(value), mIsValid(isValid)
    {
        CheckedInt_internal::integer_type_manually_recorded_info<T>
            ::TYPE_NOT_SUPPORTED_BY_CheckedInt();
    }

public:
    /** Constructs a checked integer with given \a value. The checked integer is initialized as valid or invalid
      * depending on whether the \a value is in range.
      *
      * This constructor is not explicit. Instead, the type of its argument is a separate template parameter,
      * ensuring that no conversion is performed before this constructor is actually called.
      * As explained in the above documentation for class CheckedInt, this constructor checks that its argument is
      * valid.
      */
    template<typename U>
    CheckedInt(U value)
        : mValue(T(value)),
          mIsValid(CheckedInt_internal::is_in_range<T>(value))
    {
        CheckedInt_internal::integer_type_manually_recorded_info<T>
            ::TYPE_NOT_SUPPORTED_BY_CheckedInt();
    }

    /** Constructs a valid checked integer with initial value 0 */
    CheckedInt() : mValue(0), mIsValid(1)
    {
        CheckedInt_internal::integer_type_manually_recorded_info<T>
            ::TYPE_NOT_SUPPORTED_BY_CheckedInt();
    }

    /** \returns the actual value */
    T value() const { return mValue; }

    /** \returns PR_TRUE if the checked integer is valid, i.e. is not the result
      * of an invalid operation or of an operation involving an invalid checked integer
      */
    PRBool valid() const
    {
        return PRBool(mIsValid);
    }

    /** \returns the sum. Checks for overflow. */
    template<typename U> friend CheckedInt<U> operator +(const CheckedInt<U>& lhs, const CheckedInt<U>& rhs);
    /** Adds. Checks for overflow. \returns self reference */
    template<typename U> CheckedInt& operator +=(U rhs);
    /** \returns the difference. Checks for overflow. */
    template<typename U> friend CheckedInt<U> operator -(const CheckedInt<U>& lhs, const CheckedInt<U> &rhs);
    /** Substracts. Checks for overflow. \returns self reference */
    template<typename U> CheckedInt& operator -=(U rhs);
    /** \returns the product. Checks for overflow. */
    template<typename U> friend CheckedInt<U> operator *(const CheckedInt<U>& lhs, const CheckedInt<U> &rhs);
    /** Multiplies. Checks for overflow. \returns self reference */
    template<typename U> CheckedInt& operator *=(U rhs);
    /** \returns the quotient. Checks for overflow and for divide-by-zero. */
    template<typename U> friend CheckedInt<U> operator /(const CheckedInt<U>& lhs, const CheckedInt<U> &rhs);
    /** Divides. Checks for overflow and for divide-by-zero. \returns self reference */
    template<typename U> CheckedInt& operator /=(U rhs);

    /** \returns the opposite value. Checks for overflow. */
    CheckedInt operator -() const
    {
        // circumvent msvc warning about - applied to unsigned int.
        // if we're unsigned, the only valid case anyway is 0 in which case - is a no-op.
        T result = CheckedInt_internal::opposite_if_signed(value());
        /* give the compiler a good chance to perform RVO */
        return CheckedInt(result,
                          mIsValid & CheckedInt_internal::is_sub_valid(T(0), value(), result));
    }

    /** \returns true if the left and right hand sides are valid and have the same value. */
    PRBool operator ==(const CheckedInt& other) const
    {
        return PRBool(mIsValid & other.mIsValid & (value() == other.mValue));
    }

    /** prefix ++ */
    CheckedInt& operator++()
    {
        *this = *this + 1;
        return *this;
    }

    /** postfix ++ */
    CheckedInt operator++(int)
    {
        CheckedInt tmp = *this;
        *this = *this + 1;
        return tmp;
    }

    /** prefix -- */
    CheckedInt& operator--()
    {
        *this = *this - 1;
        return *this;
    }

    /** postfix -- */
    CheckedInt operator--(int)
    {
        CheckedInt tmp = *this;
        *this = *this - 1;
        return tmp;
    }

private:
    /** operator!= is disabled. Indeed, (a!=b) should be the same as !(a==b) but that
      * would mean that if a or b is invalid, (a!=b) is always true, which is very tricky.
      */
    template<typename U>
    PRBool operator !=(U other) const { return !(*this == other); }
};

#define CHECKEDINT_BASIC_BINARY_OPERATOR(NAME, OP)               \
template<typename T>                                          \
inline CheckedInt<T> operator OP(const CheckedInt<T> &lhs, const CheckedInt<T> &rhs) \
{                                                                     \
    T x = lhs.mValue;                                                \
    T y = rhs.mValue;                                                \
    T result = x OP y;                                                \
    T is_op_valid                                                     \
        = CheckedInt_internal::is_##NAME##_valid(x, y, result);       \
    /* give the compiler a good chance to perform RVO */              \
    return CheckedInt<T>(result,                                      \
                         lhs.mIsValid & rhs.mIsValid & is_op_valid);  \
}

CHECKEDINT_BASIC_BINARY_OPERATOR(add, +)
CHECKEDINT_BASIC_BINARY_OPERATOR(sub, -)
CHECKEDINT_BASIC_BINARY_OPERATOR(mul, *)

// division can't be implemented by CHECKEDINT_BASIC_BINARY_OPERATOR
// because if rhs == 0, we are not allowed to even try to compute the quotient.
template<typename T>
inline CheckedInt<T> operator /(const CheckedInt<T> &lhs, const CheckedInt<T> &rhs)
{
    T x = lhs.mValue;
    T y = rhs.mValue;
    T is_op_valid = CheckedInt_internal::is_div_valid(x, y);
    T result = is_op_valid ? (x / y) : 0;
    /* give the compiler a good chance to perform RVO */
    return CheckedInt<T>(result,
                         lhs.mIsValid & rhs.mIsValid & is_op_valid);
}

// implement cast_to_CheckedInt<T>(x), making sure that
//  - it allows x to be either a CheckedInt<T> or any integer type that can be casted to T
//  - if x is already a CheckedInt<T>, we just return a reference to it, instead of copying it (optimization)

template<typename T, typename U>
struct cast_to_CheckedInt_impl
{
    typedef CheckedInt<T> return_type;
    static CheckedInt<T> run(U u) { return u; }
};

template<typename T>
struct cast_to_CheckedInt_impl<T, CheckedInt<T> >
{
    typedef const CheckedInt<T>& return_type;
    static const CheckedInt<T>& run(const CheckedInt<T>& u) { return u; }
};

template<typename T, typename U>
inline typename cast_to_CheckedInt_impl<T, U>::return_type
cast_to_CheckedInt(U u)
{
    return cast_to_CheckedInt_impl<T, U>::run(u);
}

#define CHECKEDINT_CONVENIENCE_BINARY_OPERATORS(OP, COMPOUND_OP) \
template<typename T>                                          \
template<typename U>                                          \
CheckedInt<T>& CheckedInt<T>::operator COMPOUND_OP(U rhs)    \
{                                                             \
    *this = *this OP cast_to_CheckedInt<T>(rhs);                 \
    return *this;                                             \
}                                                             \
template<typename T, typename U>                              \
inline CheckedInt<T> operator OP(const CheckedInt<T> &lhs, U rhs) \
{                                                             \
    return lhs OP cast_to_CheckedInt<T>(rhs);                    \
}                                                             \
template<typename T, typename U>                              \
inline CheckedInt<T> operator OP(U lhs, const CheckedInt<T> &rhs) \
{                                                             \
    return cast_to_CheckedInt<T>(lhs) OP rhs;                    \
}

CHECKEDINT_CONVENIENCE_BINARY_OPERATORS(+, +=)
CHECKEDINT_CONVENIENCE_BINARY_OPERATORS(*, *=)
CHECKEDINT_CONVENIENCE_BINARY_OPERATORS(-, -=)
CHECKEDINT_CONVENIENCE_BINARY_OPERATORS(/, /=)

template<typename T, typename U>
inline PRBool operator ==(const CheckedInt<T> &lhs, U rhs)
{
    return lhs == cast_to_CheckedInt<T>(rhs);
}

template<typename T, typename U>
inline PRBool operator ==(U  lhs, const CheckedInt<T> &rhs)
{
    return cast_to_CheckedInt<T>(lhs) == rhs;
}

// convenience typedefs.
// the use of a macro here helps make sure that we don't let a typo slip into some of these.
#define CHECKEDINT_MAKE_TYPEDEF(Type) \
typedef CheckedInt<PR##Type> Checked##Type;

CHECKEDINT_MAKE_TYPEDEF(Int8)
CHECKEDINT_MAKE_TYPEDEF(Uint8)
CHECKEDINT_MAKE_TYPEDEF(Int16)
CHECKEDINT_MAKE_TYPEDEF(Uint16)
CHECKEDINT_MAKE_TYPEDEF(Int32)
CHECKEDINT_MAKE_TYPEDEF(Uint32)
CHECKEDINT_MAKE_TYPEDEF(Int64)
CHECKEDINT_MAKE_TYPEDEF(Uint64)

} // end namespace mozilla

#endif /* mozilla_CheckedInt_h */

#include <iostream>
using namespace std;

#include "nsString.h"
#include "nsStdStringWrapper.h"
#include "nsLiteralString.h"


#if 0
static
void
Call_ToLowerCase( const nsAReadableCString& aSource, nsAWritableCString& aResult )
  {
    aSource.ToLowerCase(aResult);
  }

static
void
Call_ToUpperCase( const nsAReadableCString& aSource, nsAWritableCString& aResult )
  {
    aSource.ToUpperCase(aResult);
  }
#endif

static
ostream&
print_string( const nsAReadableString& s, ostream& os = std::cout )
  {
    struct PRUnichar_to_char
      {
        char operator()( PRUnichar c ) { return char(c); }
      };

    transform(s.Begin(), s.End(), ostream_iterator<char>(os), PRUnichar_to_char());
    return os;
  }


template <class CharT>
basic_nsLiteralString<CharT>
make_a_hello_string( CharT* )
  {
  }

NS_SPECIALIZE_TEMPLATE
basic_nsLiteralString<char>
make_a_hello_string( char* )
  {
    return basic_nsLiteralString<char>("Hello");
  }

NS_SPECIALIZE_TEMPLATE
basic_nsLiteralString<PRUnichar>
make_a_hello_string( PRUnichar* )
  {
    static PRUnichar constant_unicode[] = { 'H', 'e', 'l', 'l', 'o', PRUnichar() };
    return basic_nsLiteralString<PRUnichar>(constant_unicode);
  }

static
void
CallCMid( nsAWritableCString& aResult, const nsAReadableCString& aSource, PRUint32 aStartPos, PRUint32 aLengthToCopy )
  {
    aSource.Mid(aResult, aStartPos, aLengthToCopy);
  }



template <class CharT>
int
readable_hello_tests( const basic_nsAReadableString<CharT>& str, ostream& os = std::cout )
  {
    int tests_failed = 0;

    if ( str.Length() != 5 )
      {
        os << "Length() FAILED" << endl;
        ++tests_failed;
      }

    if ( str.First() != CharT('H') )
      {
        cout << "|First()| FAILED" << endl;
        ++tests_failed;
      }

    if ( str.Last() != CharT('o') )
      {
        cout << "|Last()| FAILED" << endl;
        ++tests_failed;
      }

    if ( str[3] != CharT('l') )
      {
        cout << "|operator[]()| FAILED" << endl;
        ++tests_failed;
      }

    if ( str.CountChar( CharT('l') ) != 2 )
      {
        cout << "|CountChar()| FAILED" << endl;
        ++tests_failed;
      }

    basic_nsAReadableString<CharT>::ConstIterator iter = str.Begin();
    if ( *iter != CharT('H') )
      {
        cout << "iterator FAILED: didn't start out pointing at the right thign or couldn't be dereferenced." << endl;
        ++tests_failed;
      }

    ++iter;

    if ( *iter != CharT('e') )
      {
        cout << "iterator FAILED: couldn't be incremented, or else couldn't be dereferenced." << endl;
        ++tests_failed;
      }

    iter = str.End();
    --iter;
    if ( *iter != CharT('o') )
      {
        cout << "iterator FAILED: couldn't be set to End(), or else couldn't be decremented, or else couldn't be dereferenced." << endl;
        ++tests_failed;
      }

    basic_nsAReadableString<CharT>::ConstIterator iter1 = str.Begin(3);
    if ( *iter1 != CharT('l') )
      {
        cout << "iterator FAILED: couldn't be set to Begin(n), or else couldn't be dereferenced." << endl;
        ++tests_failed;
      }

    basic_nsAReadableString<CharT>::ConstIterator iter2 = str.End(2);
    if ( *iter2 != CharT('l') )
      {
        cout << "iterator FAILED: couldn't be set to End(n), or else couldn't be dereferenced." << endl;
        ++tests_failed;
      }

    if ( iter1 != iter2 )
      {
        cout << "iterator comparison with != FAILED" << endl;
        ++tests_failed;
      }

    if ( !(iter1 == iter2) )
      {
        cout << "iterator comparison with == FAILED" << endl;
        ++tests_failed;
      }

    typedef CharT* CharT_ptr;

    if ( str != make_a_hello_string( CharT_ptr() ) )
      {
        cout << "comparison with hello string FAILED" << endl;
        ++tests_failed;
      }

    return tests_failed;
  }

static
inline
bool
compare_equals( const nsAReadableCString& lhs, const nsAReadableCString& rhs )
  {
    return bool(lhs == rhs);
  }

static
inline
bool
compare_equals( const nsLiteralCString& lhs, const nsAReadableCString& rhs )
  {
    return bool(lhs == rhs);
  }

static
inline
bool
compare_equals( const nsAReadableCString& lhs, const nsLiteralCString& rhs )
  {
    return bool(lhs == rhs);
  }


template <class CharT>
int
test_goofy_iterators( const basic_nsAReadableString<CharT>& aString )
  {
    typedef typename basic_nsAReadableString<CharT>::ConstIterator ConstIterator;

    int tests_failed = 0;

    ConstIterator iter1 = aString.Begin();
    ConstIterator iter2 = aString.Begin();
    ++iter2; ++iter2;

    ConstIterator iter3 = aString.End();
    --iter3;
    ++iter1; ++iter1;
    if ( iter1 != iter2 )
      {
        cout << "goofy iterator test FAILED" << endl;
        ++tests_failed;
      }

    return tests_failed;
  }



typedef void* void_ptr;

int
main()
  {	
    int tests_failed = 0;
  	cout << "String unit tests.  Compiled " __DATE__ " " __TIME__ << endl;


      //
      // |nsLiteralString|
      //


    PRUnichar constant_unicode[] = { 'H', 'e', 'l', 'l', 'o', PRUnichar() };
    nsLiteralString aLiteralString(constant_unicode);
    tests_failed += readable_hello_tests(aLiteralString);

    cout << "\"";
    print_string(aLiteralString) << "\"" << endl;

    {
      const PRUnichar* buffer = aLiteralString.GetUnicode();
      if ( !buffer )
        {
          cout << "|nsLiteralString::GetUnicode()| FAILED: should have returned non-|0|" << endl;
          ++tests_failed;
        }

      const char* cbuffer = aLiteralString.GetBuffer();
      if ( cbuffer )
        {
          cout << "|nsLiteralString::GetBuffer()| FAILED: should have returned |0|" << endl;
          ++tests_failed;
        }
    }

    {
      nsCString aCString("Hello");
      tests_failed += readable_hello_tests(aCString);
    }

    {
      nsStdCString aCString("Hello");
      tests_failed += readable_hello_tests(aCString);
    }

    {
      nsLiteralCString aLiteralCString("Hello");
      tests_failed += readable_hello_tests(aLiteralCString);
    }

    {
      nsLiteralCString str1("Hello");
      nsStdCString str2("Hello");

      nsLiteralCString str3("Hello there");

      if ( str1 != str2 )
        {
          cout << "string comparison using != failed" << endl;
          ++tests_failed;
        }

      if ( !(str3 > str2) )
        {
          cout << "string comparison using > failed" << endl;
          ++tests_failed;
        }

      if ( !(str2 < str3) )
        {
          cout << "string comparison using < failed" << endl;
          ++tests_failed;
        }

      if ( str1 != "Hello" )
        {
          cout << "string comparison using == failed" << endl;
          ++tests_failed;
        }
    }

    nsLiteralCString part1("He"), part2("llo");
    tests_failed += readable_hello_tests(part1+part2);

    nsLiteralCString part2a("l"), part2b("lo");
    tests_failed += readable_hello_tests(part1+part2a+part2b);

    cout << "The summed string is \"" << part1 + part2a + part2b << "\"" << endl;

    nsStdCString extracted_middle("XXXXXXXXXX");
    CallCMid(extracted_middle, part1+part2a+part2b, 1, 3);
    
    cout << "The result of mid was \"" << extracted_middle << "\"" << endl;
    
    nsLiteralCString middle_answer("ell");
    if ( middle_answer != extracted_middle )
      {
        cout << "mid FAILED on nsConcatString" << endl;
        ++tests_failed;
      }

      //
      // |nsLiteralCString|
      //


    nsLiteralCString aLiteralCString("Goodbye");
    cout << "\"" << aLiteralCString << "\"" << endl;
    if ( aLiteralCString.Length() == 7 )
      cout << "|nsLiteralCString::Length()| OK" << endl;
    else
      cout << "|nsLiteralCString::Length()| FAILED" << endl;
    cout << aLiteralCString << endl;

    const char* cbuffer = aLiteralCString.GetBuffer();
    cout << "GetBuffer()-->" << showbase << void_ptr(cbuffer) << endl;
    cout << "GetUnicode()-->" << void_ptr( aLiteralCString.GetUnicode() ) << endl;
    cout << "The length of the string \"" << aLiteralCString << "\" is " << aLiteralCString.Length() << endl;


      //
      // |nsStdStringWrapper|, i.e., |nsStdCString|
      //


    nsStdCString aCString("Hello");
    if ( aCString.Length() == 5 )
      cout << "|nsStdCString::Length()| OK" << endl;
    else
      cout << "|nsStdCString::Length()| FAILED" << endl;

    {
      nsStdCString extracted_middle;
      CallCMid(extracted_middle, part1+part2a+part2b, 1, 3);

     cout << "The result of mid was \"" << extracted_middle << "\"" << endl;

      nsLiteralCString middle_answer("ell");
      if ( middle_answer != extracted_middle )
        {
          cout << "mid FAILED on nsStdCString" << endl;
          ++tests_failed;
        }
    }

    cbuffer = aCString.GetBuffer();
    cout << "GetBuffer()-->" << showbase << void_ptr(cbuffer) << endl;
    cout << "GetUnicode()-->" << void_ptr( aCString.GetUnicode() ) << endl;

    // cout << "The length of the string \"" << static_cast<nsAReadableCString>(aCString) << "\" is " << aCString.Length() << endl;


    nsLiteralCString source("This is a string long enough to be interesting.");
    cout << source << endl;

    nsStdCString leftString;
    source.Left(leftString, 9);
    // cout << static_cast<nsAReadableCString>(leftString) << endl;



    tests_failed += test_goofy_iterators(part1+part2a+part2b);


    
    
    cout << "End of string unit tests. ";
    if ( !tests_failed )
      cout << "All tests OK." << endl;
    else
      cout << "One or more tests FAILED." << endl;

  	return 0;
  }

#include <iostream>
using namespace std;

#include "nsString.h"
#include "nsStdStringWrapper.h"

// #define NS_USE_WCHAR_T


static
ostream&
print_string( const nsAReadableString& s )
  {
    struct PRUnichar_to_char
      {
        char operator()( PRUnichar c ) { return char(c); }
      };

    transform(s.BeginReading(), s.EndReading(), ostream_iterator<char>(cout), PRUnichar_to_char());
    return cout;
  }


template <class CharT>
basic_nsLiteralString<CharT>
literal_hello( CharT* )
  {
  }

NS_SPECIALIZE_TEMPLATE
basic_nsLiteralString<char>
literal_hello( char* )
  {
    return basic_nsLiteralString<char>("Hello");
  }

NS_SPECIALIZE_TEMPLATE
basic_nsLiteralString<PRUnichar>
literal_hello( PRUnichar* )
  {
#ifdef NS_USE_WCHAR_T
    return basic_nsLiteralString<PRUnichar>(L"Hello");
#else
    static PRUnichar constant_unicode[] = { 'H', 'e', 'l', 'l', 'o', PRUnichar() };
    return basic_nsLiteralString<PRUnichar>(constant_unicode);
#endif
  }

static
void
CallCMid( nsAWritableCString& aResult, const nsAReadableCString& aSource, PRUint32 aStartPos, PRUint32 aLengthToCopy )
  {
    aSource.Mid(aResult, aStartPos, aLengthToCopy);
  }



template <class CharT>
int
test_multifragment_iterators( const basic_nsAReadableString<CharT>& aString )
    /*
      ...this tests a problem that was present in |nsPromiseConcatenation| where,
      because it originally stored some iteration state in the object itself, rather than
      in the fragment, the iterators could get confused if used out of sequence.

      This test should be run on any multi-fragment implementation to verify that it
      does not have the same bug.  Make sure the first fragment is only one character long.
    */
  {
    typedef typename basic_nsAReadableString<CharT>::ConstIterator ConstIterator;

    int tests_failed = 0;

    ConstIterator iter1 = aString.BeginReading();
    ConstIterator iter2 = aString.BeginReading();
    ++iter2; ++iter2;

    ConstIterator iter3 = aString.EndReading();
    --iter3;
    ++iter1; ++iter1;
    if ( iter1 != iter2 )
      {
        cout << "FAILED in |test_multifragment_iterators|" << endl;
        ++tests_failed;
      }

    return tests_failed;
  }

template <class CharT>
int
test_deprecated_GetBufferGetUnicode( const basic_nsAReadableString<CharT>& aReadable )
  {
    int tests_failed = 0;

    if ( aReadable.GetBuffer() || aReadable.GetUnicode() )
      {
        cout << "FAILED |test_deprecated_GetBufferGetUnicode()|: non-zero result." << endl;
        ++tests_failed;
      }

    return tests_failed;
  }

NS_SPECIALIZE_TEMPLATE
int
test_deprecated_GetBufferGetUnicode( const basic_nsAReadableString<char>& aReadable )
  {
    int tests_failed = 0;

    if ( !aReadable.GetBuffer() )
      {
        cout << "FAILED |test_deprecated_GetBufferGetUnicode()|: |GetBuffer()| returned 0." << endl;
        ++tests_failed;
      }

    if ( aReadable.GetUnicode() )
      {
        cout << "FAILED |test_deprecated_GetBufferGetUnicode()|: |GetUnicode()| returned a non-zero result." << endl;
        ++tests_failed;
      }

    return tests_failed;
  }

NS_SPECIALIZE_TEMPLATE
int
test_deprecated_GetBufferGetUnicode( const basic_nsAReadableString<PRUnichar>& aReadable )
  {
    int tests_failed = 0;

    if ( aReadable.GetBuffer() )
      {
        cout << "FAILED |test_deprecated_GetBufferGetUnicode()|: |GetBuffer()| returned a non-zero result." << endl;
        ++tests_failed;
      }

    if ( !aReadable.GetUnicode() )
      {
        cout << "FAILED |test_deprecated_GetBufferGetUnicode()|: |GetUnicode()| returned 0." << endl;
        ++tests_failed;
      }

    return tests_failed;
  }

template <class CharT>
int
test_readable_hello( const basic_nsAReadableString<CharT>& aReadable )
  {
    int tests_failed = 0;

    if ( aReadable.Length() != 5 )
      {
        cout << "FAILED |test_readable_hello|: |Length()| --> " << aReadable.Length() << endl;
        ++tests_failed;
      }

    if ( aReadable.First() != CharT('H') )
      {
        cout << "FAILED |test_readable_hello|: |First()| --> '" << aReadable.First() << "'" << endl;
        ++tests_failed;
      }

    if ( aReadable.Last() != CharT('o') )
      {
        cout << "FAILED |test_readable_hello|: |Last()| --> '" << aReadable.Last() << "'" << endl;
        ++tests_failed;
      }

    if ( aReadable[3] != CharT('l') )
      {
        cout << "FAILED |test_readable_hello|: |operator[]| --> '" << aReadable[3] << "'" << endl;
        ++tests_failed;
      }

    if ( aReadable.CountChar( CharT('l') ) != 2 )
      {
        cout << "FAILED |test_readable_hello|: |CountChar('l')| --> " << aReadable.CountChar(CharT('l')) << endl;
        ++tests_failed;
      }

    basic_nsAReadableString<CharT>::ConstIterator iter = aReadable.BeginReading();
    if ( *iter != CharT('H') )
      {
        cout << "FAILED |test_readable_hello|: didn't start out pointing to the right thing, or else couldn't be dereferenced. --> '" << *iter << "'" << endl;
        ++tests_failed;
      }

    ++iter;

    if ( *iter != CharT('e') )
      {
        cout << "FAILED |test_readable_hello|: iterator couldn't be incremented, or else couldn't be dereferenced. --> '" << *iter << "'" << endl;
        ++tests_failed;
      }

    iter = aReadable.EndReading();
    --iter;
    if ( *iter != CharT('o') )
      {
        cout << "FAILED |test_readable_hello|: iterator couldn't be set to |EndReading()|, or else couldn't be decremented, or else couldn't be dereferenced. --> '" << *iter << "'" << endl;
        ++tests_failed;
      }

    basic_nsAReadableString<CharT>::ConstIterator iter1 = aReadable.BeginReading(3);
    if ( *iter1 != CharT('l') )
      {
        cout << "FAILED |test_readable_hello|: iterator couldn't be set to |BeginReading(n)|, or else couldn't be dereferenced. --> '" << *iter1 << "'" << endl;
        ++tests_failed;
      }

    basic_nsAReadableString<CharT>::ConstIterator iter2 = aReadable.EndReading(2);
    if ( *iter2 != CharT('l') )
      {
        cout << "FAILED |test_readable_hello|: iterator couldn't be set to |EndReading(n)|, or else couldn't be dereferenced. --> '" << *iter2 << "'" << endl;
        ++tests_failed;
      }

    if ( iter1 != iter2 )
      {
        cout << "FAILED |test_readable_hello|: iterator comparison with !=." << endl;
        ++tests_failed;
      }

    if ( !(iter1 == iter2) )
      {
        cout << "FAILED |test_readable_hello|: iterator comparison with ==." << endl;
        ++tests_failed;
      }

    typedef CharT* CharT_ptr;
    if ( aReadable != literal_hello(CharT_ptr()) )
      {
        cout << "FAILED |test_readable_hello|: comparison with \"Hello\"" << endl;
        ++tests_failed;
      }

    tests_failed += test_multifragment_iterators(aReadable);
    tests_failed += test_deprecated_GetBufferGetUnicode(aReadable);

    return tests_failed;
  }





template <class CharT>
int
test_writable( basic_nsAWritableString<CharT>& aWritable )
  {
    int tests_failed = 0;
    // ...


    {
      typedef CharT* CharT_ptr;
      aWritable = literal_hello(CharT_ptr());

      if ( aWritable != literal_hello(CharT_ptr()) )
        {
          cout << "FAILED assignment and/or comparison in |test_writable|." << endl;
          ++tests_failed;
        }

      tests_failed += test_readable_hello(aWritable);
    }

    return tests_failed;
  }



typedef void* void_ptr;

int
main()
  {	
    int tests_failed = 0;
  	cout << "String unit tests.  Compiled " __DATE__ " " __TIME__ << endl;


    {
#ifdef NS_USE_WCHAR_T
      nsLiteralString s0(L"Hello");
#else
      PRUnichar b0[] = { 'H', 'e', 'l', 'l', 'o', PRUnichar() };
      nsLiteralString s0(b0);
#endif
      tests_failed += test_readable_hello(s0);

      nsLiteralCString s1("Hello");
      tests_failed += test_readable_hello(s1);
    }

    {
#ifdef NS_USE_WCHAR_T
      nsString s3(L"Hello");
#else
      PRUnichar b3[] = { 'H', 'e', 'l', 'l', 'o', PRUnichar() };
      nsString s3(b3);
#endif
      tests_failed += test_readable_hello(s3);
      tests_failed += test_writable(s3);

      nsCString s4("Hello");
      tests_failed += test_readable_hello(s4);
      tests_failed += test_writable(s4);
    }

    {
#ifdef NS_USE_WCHAR_T
      nsStdString s5(L"Hello");
#else
      PRUnichar b5[] = { 'H', 'e', 'l', 'l', 'o', PRUnichar() };
      nsStdString s5(b5);
#endif
      tests_failed += test_readable_hello(s5);
      tests_failed += test_writable(s5);

      nsStdCString s6("Hello");
      tests_failed += test_readable_hello(s6);
      tests_failed += test_writable(s6);
    }

    {
#ifdef NS_USE_WCHAR_T
      nsLiteralString s7(L"He");
      nsString        s8(L"l");
      nsStdString     s9(L"lo");
#else
      PRUnichar b7[] = { 'H', 'e', PRUnichar() };
      PRUnichar b8[] = { 'l', PRUnichar() };
      PRUnichar b9[] = { 'l', 'o', PRUnichar() };

      nsLiteralString s7(b7);
      nsString        s8(b8);
      nsStdString     s9(b9);
#endif

      tests_failed += test_readable_hello(s7+s8+s9);

      nsString s13( s7 + s8 + s9 );
      tests_failed += test_readable_hello(s13);

      nsStdString s14( s7 + s8 + s9 );
      tests_failed += test_readable_hello(s14);

      // nsSharedString s15( s7 + s8 + s9 );
      // tests_failed += test_readable_hello(s15);

      nsCString         s10("He");
      nsLiteralCString  s11("l");
      nsStdCString      s12("lo");
      
      tests_failed += test_readable_hello(s10+s11+s12);

      
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

#if 0
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
      // |nsStdStringWrapper|, i.e., |nsStdCString|
      //

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



    nsStdCString leftString;
    source.Left(leftString, 9);
    // cout << static_cast<nsAReadableCString>(leftString) << endl;



    tests_failed += test_multifragment_iterators(part1+part2a+part2b);
#endif

    
    
    cout << "End of string unit tests." << endl;
    if ( !tests_failed )
      cout << "OK, all tests passed." << endl;
    else
      cout << "FAILED one or more tests." << endl;

  	return 0;
  }

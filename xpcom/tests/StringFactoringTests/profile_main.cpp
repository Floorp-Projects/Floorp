// profile_main.cpp

#include "nscore.h"
#include <iostream.h>
#include <string>
#include <iomanip>

#include "nsInt64.h"

#ifdef XP_MAC
#include <Timer.h>
#include "Profiler.h"
#else
#include "prtime.h"
#endif

#ifndef TEST_STD_STRING
#include "nsString.h"
#else
#include "nsStdStringWrapper.h"
typedef nsStdCString nsCString;
#endif

static const int kTestSucceeded = 0;
static const int kTestFailed = 1;

static const size_t N = 100000;


template <class T>
inline
PRUint32
TotalLength( const T& s )
  {
    return s.Length();
  }

NS_SPECIALIZE_TEMPLATE
inline
PRUint32
TotalLength( const string& s )
  {
    return s.length();
  }

template <class T>
inline
PRUint32
Find( const T& text, const T& pattern )
  {
    return text.Find(pattern);
  }

NS_SPECIALIZE_TEMPLATE
inline
PRUint32
Find( const string& text, const string& pattern )
  {
    return text.find(pattern);
  }

inline
nsInt64
GetTime()
  {
#ifdef XP_MAC
    UnsignedWide time;
    Microseconds(&time);
    return nsInt64( *reinterpret_cast<PRInt64*>(&time) );
#else
    return nsInt64( PR_Now() );
#endif
  }

class TestTimer
  {
    public:
      TestTimer() : mStartTime(GetTime()) { }

     ~TestTimer()
        {
          nsInt64 stopTime = GetTime();
	        nsInt64 totalTime = stopTime - mStartTime;
#ifdef HAVE_LONG_LONG
          cout << setw(10) << NS_STATIC_CAST(PRInt64, totalTime) << " µs : ";
#else
          cout << setw(10) << NS_STATIC_CAST(PRInt32, totalTime) << "µs : ";
#endif
        }

    private:
      nsInt64 mStartTime;
  };

inline
int
foo( const nsCString& )
  {
    return 1;
  }

static
int
test_construction()
  {
    cout << endl;

    {
      nsCString someCString;
      int total = 0;
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          total += foo( someCString );
        }
    }
    cout << "null loop time for constructor" << endl;


    {
      int total = 0;
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          total += foo( nsCString() );
        }
    }
    cout << "nsCString()" << endl;


    {
      int total = 0;
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          total += foo( nsCString("This is a reasonable length string with some text in it and it is good.") );
        }
    }
    cout << "nsCString(\"abc\")" << endl;

    return kTestSucceeded;
  }

static
int
test_concat()
  {
    cout << endl;

                //---------|---------|---------|---------|---------|---------|---------|
    nsCString s1("This is a reasonable length string with some text in it and it is good.");
    nsCString s2("This is another string that I will use in the concatenation test.");
    nsCString s3("This is yet a third string that I will use in the concatenation test.");

    PRUint32 len = TotalLength( s1 + s2 + s3 + s1 + s2 + s3 );
    if ( len != (71 + 65 + 69 + 71 + 65 + 69) )
      {
        cout << "|test_concat()| FAILED" << endl;
        return kTestFailed;
      }


    {
      nsCString anEmptyCString;
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          len += TotalLength( anEmptyCString );
        }
    }
    cout << "null loop time for concat" << endl;


    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        len += TotalLength( s1 + s2 + s3 + s1 + s2 + s3 );
    }
    cout << "TotalLength( s1 + s2 + s3 + s1 + s2 + s3 )" << endl;


    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        len += TotalLength( s1 + s2 );
    }
    cout << "TotalLength( s1 + s2 )" << endl;

    return kTestSucceeded;
  }

static
int
test_concat_and_assign()
  {
                //---------|---------|---------|---------|---------|---------|---------|
    nsCString s1("This is a reasonable length string with some text in it and it is good.");
    nsCString s2("This is another string that I will use in the concatenation test.");
    nsCString s3("This is yet a third string that I will use in the concatenation test.");

    nsCString s4( s1 + s2 + s3 + s1 + s2 + s3 );
    if ( TotalLength(s4) != (71 + 65 + 69 + 71 + 65 + 69) )
      {
        cout << "|test_concat()| FAILED" << endl;
        return kTestFailed;
      }


    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        s4 = s1 + s2 + s3 + s1 + s2 + s3;
    }
    cout << "s4 = s1 + s2 + s3 + s1 + s2 + s3" << endl;

    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        s4 = s1 + s2;
    }
    cout << "s4 = s1 + s2" << endl;

    return kTestSucceeded;
  }

static
int
test_compare()
  {
    nsCString s1("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxThis is a reasonable length string with some text in it and it is good.");
    nsCString s2("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxThis is a reasonable length string with some text in it and it is bad.");

    int count = 0;
    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        if ( s1 > s2 )
          ++count;
    }
    cout << "s1 > s2" << endl;

    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        if ( s1 == s1 )
          ++count;
    }
    cout << "s1 == s1" << endl;

    return kTestSucceeded;
  }

static
int
test_countchar()
  {
    nsCString s1("This is a reasonable length string with some text in it and it is good.");

    if ( s1.CountChar('e') != 5 )
      {
        cout << "|test_countchar()| FAILED: found a  count of " << s1.CountChar('e') << endl;
        return kTestFailed;
      }

    PRUint32 total = 0;
    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        total += s1.CountChar('e');
    }
    cout << "s1.CountChar('e')" << endl;

    return kTestSucceeded;
  }

static
int
test_append_string()
  {
    nsCString s1("This is a reasonable length string with some text in it and it is good.");
    nsCString s2("This is another string that I will use in the concatenation test.");

    PRUint32 len = 0;
    
    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          nsCString s3;
          s3.Append(s1);
          s3.Append(s2);
          len += TotalLength(s3);
        }
    }
    cout << "s3.Append(s1); s3.Append(s2)" << endl;

    return kTestSucceeded;
  }

static
int
test_repeated_append_string()
  {
    nsCString s1("This is a reasonable length string with some text in it and it is good.");
    nsCString s2("This is another string that I will use in the concatenation test.");

    PRUint32 len = 0;
    
    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          nsCString s3;
          for ( int j=0; j<100; ++j )
            {
              s3.Append(s1);
              s3.Append(s2);
              len += TotalLength(s3);
            }
        }
    }
    cout << "repeated s3.Append(s1); s3.Append(s2)" << endl;

    return kTestSucceeded;
  }

static
int
test_append_char()
  {
    cout << endl;

    PRUint32 len = 0;

    nsCString s1("hello");
    PRUint32 oldLength = s1.Length();
    
    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          s1.SetLength(oldLength);
        }
    }
    cout << "null loop time for append char" << endl;

    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          s1.Append('e');
          s1.SetLength(oldLength);
        }
    }
    cout << "s1.Append('e')" << endl;

    return kTestSucceeded;
  }

static
int
test_repeated_append_char()
  {
    PRUint32 len = 0;
    
    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          nsCString s1;
          for ( int j=0; j<1000; ++j )
            {
              s1.Append('e');
              len += TotalLength(s1);
            }
        }
    }
    cout << "repeated s1.Append('e')" << endl;

    return kTestSucceeded;
  }

static
int
test_insert_string()
  {
    nsCString s1("This is a reasonable length string with some text in it and it is good.");

    PRUint32 len = 0;
    
    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        {
          nsCString s2("This is another string that I will use in the concatenation test.");
          s2.Insert(s1, 3);
          len += TotalLength(s2);
        }
    }
    cout << "s2.Insert(s1, 3)" << endl;

    return kTestSucceeded;
  }

#ifndef TEST_STD_STRING
static
int
test_find_string()
  {
    nsCString text("aaaaaaaaaab");
    nsCString pattern("aab");

    PRUint32 position = 0;
    {
      TestTimer timer;
      for ( int i=0; i<N; ++i )
        position = Find(text, pattern);
    }
    cout << "text.Find(pattern)" << endl;

    return kTestSucceeded;
  }
#endif

class Profiler
  {
    public:
      Profiler()
        {
#if 0
          ProfilerInit(collectDetailed, bestTimeBase, 100, 32);
#endif
        }

      void
      Dump( const char* output_name )
        {
	}

      void
      Dump( const unsigned char* output_name )
        {
#if 0
          ProfilerDump(output_name);
#endif
        }

     ~Profiler()
        {
#if 0
          ProfilerDump(mOutputName);
          ProfilerTerm();
#endif
        }
  };

int
main()
  {

    cout << "String performance profiling.  Compiled " __DATE__ " " __TIME__ << endl;
#ifdef TEST_STD_STRING
    cout << "Testing std::string." << endl;
#else
    cout << "Testing factored nsString." << endl;
#endif

    int tests_failed = 0;

    Profiler profiler;

    tests_failed += test_construction();
    tests_failed += test_concat();
    tests_failed += test_concat_and_assign();
    tests_failed += test_compare();
    tests_failed += test_countchar();
    tests_failed += test_append_string();
    tests_failed += test_repeated_append_string();
    tests_failed += test_append_char();
    tests_failed += test_repeated_append_char();
    tests_failed += test_insert_string();
#ifndef TEST_STD_STRING
    tests_failed += test_find_string();
#endif

#ifdef TEST_STD_STRING
    profiler.Dump("\pStandard String.prof");
#else
    profiler.Dump("\pFactored String.prof");
#endif

    if ( tests_failed )
      cout << "One or more tests FAILED.  Measurements may be invalid." << endl;

    cout << "End of string performance profiling." << endl;
    return 0;
  }

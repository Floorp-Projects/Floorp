
#ifndef _STRINGTEST
#define _STRINGTEST

#include "nsString.h"
#include <string>
#include <time.h>

using namespace std;

#define USE_WIDE  1
#ifdef  USE_WIDE
  #define stringtype  nsString
  #define astringtype nsAutoString
  #define chartype    PRUnichar
  #define stlstringtype wstring
#else
  #define stringtype  nsCString
  #define astringtype nsCAutoString
  #define chartype    char
  #define stlstringtype string
#endif


#include "windows.h"
#include <stdio.h>

const double gTicks = 1.0e-7;

/**************************************************************************
  
  We use this stopwatch class for timing purposes. I've placed this inline
  to simplify the project creation process. 

  This is effectively the same stopwatch as the one used by the gecko team
  for performance measurement.

 **************************************************************************/
class CStopwatch {

private:
   enum EState { kUndefined, kStopped, kRunning };

   double     fStartRealTime;   //wall clock start time
   double     fStopRealTime;    //wall clock stop time
   double     fStartCpuTime;    //cpu start time
   double     fStopCpuTime;     //cpu stop time
   double     fTotalCpuTime;    //total cpu time
   double     fTotalRealTime;   //total real time
   EState     fState;           //stopwatch state

public:
  CStopwatch() {
   // Create a stopwatch and start it.
   fState         = kUndefined;
   fTotalCpuTime  = 0;
   fTotalRealTime = 0;
   Start();
  }
  
  void Start(bool reset = true) {

       // Start the stopwatch. If reset is kTRUE reset the stopwatch before
   // starting it. Use kFALSE to continue timing after a Stop() without
   // resetting the stopwatch.

   if (reset) {
      fTotalCpuTime  = 0;
      fTotalRealTime = 0;
   }
   if (fState != kRunning) {
      fStartRealTime = GetRealTime();
      fStartCpuTime  = GetCPUTime();
   }
   fState = kRunning;
  }

  void Stop() {
   // Stop the stopwatch.
   fStopRealTime = GetRealTime();
   fStopCpuTime  = GetCPUTime();
   if (fState == kRunning) {
      fTotalCpuTime  += fStopCpuTime  - fStartCpuTime;
      fTotalRealTime += fStopRealTime - fStartRealTime;
   }
   fState = kStopped;

  }

  void Continue() {
   // Resume a stopped stopwatch. The stopwatch continues counting from the last
   // Start() onwards (this is like the laptimer function).

  if (fState == kUndefined) {
    printf("%s\n","stopwatch not started");
    exit(1);
  }

  if (fState == kStopped) {
    fTotalCpuTime  -= fStopCpuTime  - fStartCpuTime;
    fTotalRealTime -= fStopRealTime - fStartRealTime;
  }

  fState = kRunning;

  }

  void Reset() { ResetCpuTime(); ResetRealTime(); }
  void ResetCpuTime(double time = 0) { Stop();  fTotalCpuTime = time; }
  void ResetRealTime(double time = 0) { Stop(); fTotalRealTime = time; }

  double     CpuTime() {
   // Return the cputime passed between the start and stop events. If the
   // stopwatch was still running stop it first.

  if (fState == kUndefined) {
    printf("%s\n","stopwatch not started");
    exit(1);
  }

   if (fState == kRunning)
      Stop();

   return fTotalCpuTime;  
  }

  void Print() {
    // Print the real and cpu time passed between the start and stop events.

    double  realt = RealTime();

    int  hours = int(realt / 3600);
    realt -= hours * 3600;
    int  min   = int(realt / 60);
    realt -= min * 60;
    int  sec   = int(realt);
    printf("Real time %d:%d:%d, CP time %.3f", hours, min, sec, CpuTime());

  }

  double RealTime()
  {
     // Return the realtime passed between the start and stop events. If the
     // stopwatch was still running stop it first.

    if (fState == kUndefined) {
      printf("%s\n","stopwatch not started");
      exit(1);
    }
   
    if (fState == kRunning)
      Stop();

    return fTotalRealTime;
  }

  static double GetRealTime(){ 
    union {
      FILETIME ftFileTime;
      __int64  ftInt64;
    } ftRealTime; // time the process has spent in kernel mode
    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st,&ftRealTime.ftFileTime);
    return (double)ftRealTime.ftInt64 * gTicks;
  }
  
  static double GetCPUTime() {
    OSVERSIONINFO OsVersionInfo;

  //*-*         Value                      Platform
  //*-*  ----------------------------------------------------
  //*-*  VER_PLATFORM_WIN32s          Win32s on Windows 3.1
  //*-*  VER_PLATFORM_WIN32_WINDOWS       Win32 on Windows 95
  //*-*  VER_PLATFORM_WIN32_NT            Windows NT
  //*-*
    OsVersionInfo.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
    GetVersionEx(&OsVersionInfo);
    if (OsVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
      DWORD       ret;
      FILETIME    ftCreate,       // when the process was created
                  ftExit;         // when the process exited

      union     {FILETIME ftFileTime;
                 __int64  ftInt64;
                } ftKernel; // time the process has spent in kernel mode

      union     {FILETIME ftFileTime;
                 __int64  ftInt64;
                } ftUser;   // time the process has spent in user mode

      HANDLE hProcess = GetCurrentProcess();
      ret = GetProcessTimes (hProcess, &ftCreate, &ftExit,
                                       &ftKernel.ftFileTime,
                                       &ftUser.ftFileTime);
      if (ret != TRUE){
        ret = GetLastError ();
        printf("%s 0x%lx\n"," Error on GetProcessTimes", (int)ret);
      }

      /*
       * Process times are returned in a 64-bit structure, as the number of
       * 100 nanosecond ticks since 1 January 1601.  User mode and kernel mode
       * times for this process are in separate 64-bit structures.
       * To convert to floating point seconds, we will:
       *
       *          Convert sum of high 32-bit quantities to 64-bit int
       */

        return (double) (ftKernel.ftInt64 + ftUser.ftInt64) * gTicks;
    }
    else
        return GetRealTime();

  }

};



static const char* kConstructorError = "constructor error";
static const char* kComparisonError  = "Comparision error!";
static const char* kEqualsError = "Equals error!";

static char* kNumbers[]={"0123456789","0\01\02\03\04\05\06\07\08\09\0\0\0"};
static char* kLetters[]={"abcdefghij","a\0b\0c\0d\0e\0f\0g\0h\0i\0j\0\0\0"};
static char* kAAA[]={"AAA","A\0A\0A\0\0\0"};
static char* kBBB[]={"BBB","B\0B\0B\0\0\0"};
static char* kHello[]={"hello","h\0e\0l\0l\0o\0\0\0"};
static char* kWSHello[]={"  hello  "," \0 \0h\0e\0l\0l\0o\0 \0 \0\0\0"};


void getNumbers(nsStr& aString,PRBool aMultiByte){
  aString.mLength=aString.mCapacity=10;
  aString.mStr=( char*)kNumbers[aMultiByte];
}

void getAAA(nsStr& aString,PRBool aMultiByte){
  aString.mLength=aString.mCapacity=3;
  aString.mStr=( char*)kAAA[aMultiByte];
}

void getBBB(nsStr& aString,PRBool aMultiByte){
  aString.mLength=aString.mCapacity=3;
  aString.mStr=( char*)kBBB[aMultiByte];
}

void getHello(nsStr& aString,PRBool aMultiByte){
  aString.mLength=aString.mCapacity=5;
  aString.mStr=( char*)kHello[aMultiByte];
}

void getWSHello(nsStr& aString,PRBool aMultiByte){
  aString.mLength=aString.mCapacity=9;
  aString.mStr=(char*)kWSHello[aMultiByte];
}


/********************************************************
  
    This class's only purpose in life is to test the 
    netscape string library. We exercise the string
    API's here, and do a bit of performance testing
    against the standard c++ library string (from STL).

 ********************************************************/
class CStringTester {
public:
  CStringTester() {
    TestConstructors();
    TestAutoStrings();
    TestAppend();
    TestAssignAndAdd();
    TestInsert();
    TestDelete();
    TestTruncate();
    TestLogical();
    TestLexomorphic();
    TestCreators();
    TestNumerics();
    TestNSStr();
    TestCharAccessors();
    TestSearching();
    TestSubsumables();
    TestRandomOps();
    TestReplace();
    TestStringPerformance();
    TestWideStringPerformance();
    TestRegressions();
  }
protected:
    int TestConstructors();
    int TestLogical();
    int TestAutoStrings();
    int TestAppend();
    int TestAssignAndAdd();
    int TestInsert();
    int TestDelete();
    int TestTruncate();
    int TestLexomorphic();
    int TestCreators();
    int TestNumerics();
    int TestCharAccessors();
    int TestSearching();
    int TestNSStr();
    int TestSubsumables();
    int TestRandomOps();
    int TestReplace();
    int TestStringPerformance();
    int TestWideStringPerformance();
    int TestRegressions();
};



/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestSearching(){
  int result=0;



  PRUnichar pbuf[10]={'e','f','g',0};
  PRUnichar pbuf2[10]={'a','b','c',0};


  //Since there is so much ground to cover with searching, we use a typedef to 
  //allow you to vary the string type being searched...


  stringtype theDest("abcdefghijkabc");
  nsString  s1("ijk");
  nsCString c1("ijk");

  PRInt32 pos=theDest.Find(pbuf);
  NS_ASSERTION(pos==4,"Error: Find routine");

  pos=theDest.Find(pbuf2,PR_FALSE,-1);
  NS_ASSERTION(pos==0,"Error: Find routine");

  pos=theDest.Find(pbuf2,PR_FALSE,pos+1);
  NS_ASSERTION(pos==11,"Error: Find routine");

  pos=theDest.FindChar('a');
  NS_ASSERTION(pos==0,"Error: Find routine");

  pos=theDest.FindChar('a',PR_FALSE,pos+1);
  NS_ASSERTION(pos==11,"Error: Find routine");

  pos=theDest.Find("efg");
  NS_ASSERTION(pos==4,"Error: Find routine");

  pos=theDest.Find("EFG",PR_TRUE);
  NS_ASSERTION(pos==4,"Error: Find routine");

  pos=theDest.FindChar('d');
  NS_ASSERTION(pos==3,"Error: Find char routine");

  pos=theDest.Find(s1);
  NS_ASSERTION(pos==8,"Error: Find char routine");

  pos=theDest.FindCharInSet("12k");
  NS_ASSERTION(pos==10,"Error: Findcharinset routine");

  pos=theDest.FindCharInSet(pbuf);
  NS_ASSERTION(pos==4,"Error: Findcharinset routine");

  pos=theDest.FindCharInSet(s1);
  NS_ASSERTION(pos==8,"Error: Findcharinset routine");

  pos=theDest.Find("efg",PR_FALSE,2);
  NS_ASSERTION(pos==4,"Error: Find routine");

  pos=theDest.RFindCharInSet("12k");
  NS_ASSERTION(pos==10,"Error: RFindcharinset routine");

  pos=theDest.RFindCharInSet("xyz");
  NS_ASSERTION(pos==-1,"Error: RFindcharinset routine");

  pos=theDest.RFindCharInSet(pbuf);
  NS_ASSERTION(pos==6,"Error: RFindcharinset routine");

  pos=theDest.RFindCharInSet(s1);
  NS_ASSERTION(pos==10,"Error: RFindcharinset routine");

  pos=theDest.RFind("efg");
  NS_ASSERTION(pos==4,"Error: RFind routine");

  pos=theDest.RFind("xxx");
  NS_ASSERTION(pos==-1,"Error: RFind routine"); //this should fail

  pos=theDest.RFind("");
  NS_ASSERTION(pos==-1,"Error: RFind routine"); //this too should fail.

  pos=theDest.RFindChar('a',PR_FALSE,4);
  NS_ASSERTION(pos==0,"Error: RFind routine");


    //now try searching with FindChar using offset and count...
  {
    stringtype s1("hello there rick");

    PRInt32 pos=s1.FindChar('r');  //this will search from the begining, and for the length of the string.
    NS_ASSERTION(pos==9,"Error: FindChar() with offset and count");

    pos=s1.FindChar('r',PR_FALSE,0,5);  //this will search from the front using count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: FindChar() with offset and count");

    pos=s1.FindChar('r',PR_FALSE,0,10);  //this will search from the front using count. THIS WILL SUCCEED!
    NS_ASSERTION(pos==9,"Error: FindChar() with offset and count");

    pos=s1.FindChar('i',PR_FALSE,5,5);  //this will search from the middle using count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: FindChar() with offset and count");

    pos=s1.FindChar('i',PR_FALSE,5,10);  //this will search from the middle using count. THIS WILL SUCCEED!
    NS_ASSERTION(pos==13,"Error: FindChar() with offset and count");

    pos=s1.FindChar('k',PR_FALSE,10,2);  //this will search from near the end using count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: FindChar() with offset and count");

    pos=s1.FindChar('k',PR_FALSE,10,7);  //this will search from near the end using count. THIS WILL SUCCEED!
    NS_ASSERTION(pos==15,"Error: FindChar() with offset and count");

    //now let's try a few with bad data...

    pos=s1.FindChar('k',PR_FALSE,100,2);  //this will search from a bad offset. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: FindChar() with offset and count");

    pos=s1.FindChar('k',PR_FALSE,10,0);  //this will search for a bad count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: FindChar() with offset and count");

    pos=s1.FindChar('k',PR_FALSE,10,20);  //this will search for a bad count. THIS WILL SUCCEED!
    NS_ASSERTION(pos==15,"Error: FindChar() with offset and count");

    pos=s1.FindChar('k',PR_FALSE,10,4);  //this will search for a bad count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: FindChar() with offset and count");

    pos=10;
  }

    //now try searching with RFindChar using offset and count...
  {
    stringtype s1("hello there rick");

    PRInt32 pos=s1.RFindChar('o');  //this will search from the end, and for the length of the string.
    NS_ASSERTION(pos==4,"Error: RFindChar() with offset and count");

    pos=s1.RFindChar(' ',PR_FALSE,-1,4);  //this will search from the end, and for the length of the string.
    NS_ASSERTION(pos==-1,"Error: RFindChar() with offset and count"); //THIS WILL FAIL

    pos=s1.RFindChar(' ',PR_FALSE,12,1);  //this will search from the middle, and for the length of 1.
    NS_ASSERTION(pos==-1,"Error: RFindChar() with offset and count"); //THIS WILL FAIL

    pos=s1.RFindChar(' ',PR_FALSE,12,2);  //this will search from the middle, and for the length of 2.
    NS_ASSERTION(pos==11,"Error: RFindChar() with offset and count"); //THIS WILL SUCCEED

    pos=s1.RFindChar('o',PR_FALSE,-1,5);  //this will search from the end using count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: RFindChar() with offset and count");

    pos=s1.RFindChar('o',PR_FALSE,-1,12);  //this will search from the front using count. THIS WILL SUCCEED!
    NS_ASSERTION(pos==4,"Error: RFindChar() with offset and count");

    pos=s1.RFindChar('l',PR_FALSE,8,2);  //this will search from the middle using count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: RFindChar() with offset and count");

    pos=s1.RFindChar('l',PR_FALSE,8,7);  //this will search from the middle using count. THIS WILL SUCCEED!
    NS_ASSERTION(pos==3,"Error: RFindChar() with offset and count");

//***

    pos=s1.RFindChar('h',PR_FALSE,3,2);  //this will search from near the end using count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: RFindChar() with offset and count");

    pos=s1.RFindChar('h',PR_FALSE,3,7);  //this will search from near the end using count. THIS WILL SUCCEED!
    NS_ASSERTION(pos==0,"Error: RFindChar() with offset and count");

    //now let's try a few with bad data...

    pos=s1.RFindChar('k',PR_FALSE,100,2);  //this will search from a bad offset. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: RFindChar() with offset and count");

    pos=s1.RFindChar('k',PR_FALSE,10,0);  //this will search for a bad count. THIS WILL FAIL!
    NS_ASSERTION(pos==-1,"Error: RFindChar() with offset and count");

    pos=10;
  }

    //now try searching with Find() using offset and count...
  {
    stringtype s1("hello there rick");

    PRInt32 pos= s1.Find("there",PR_FALSE,0,4); //first search from front using offset
    NS_ASSERTION(pos==-1,"Error: Find() with offset and count");  //THIS WILL FAIL

    pos= s1.Find("there",PR_FALSE,0,8); //first search from front using count
    NS_ASSERTION(pos==6,"Error: Find) with offset and count");  //THIS WILL SUCCEED

    pos= s1.Find("there",PR_FALSE,4,1); //first search from front using count
    NS_ASSERTION(pos==-1,"Error: Find() with offset and count");  //THIS WILL FAIL

    pos= s1.Find("there",PR_FALSE,5,2); //first search from front using count
    NS_ASSERTION(pos==6,"Error: Find() with offset and count");  //THIS WILL SUCCEED

    pos= s1.Find("there",PR_FALSE,6,1); //first search from front using count
    NS_ASSERTION(pos==6,"Error: Find() with offset and count");  //THIS WILL SUCCEED

    pos= s1.Find("there",PR_FALSE,6,0); //first search from front using a bogus count
    NS_ASSERTION(pos==-1,"Error: Find() with offset and count");  //THIS WILL FAIL
    
    pos= s1.Find("k",PR_FALSE,15,1); //first search from end using a count
    NS_ASSERTION(pos==15,"Error: Find() with offset and count");  //THIS WILL SUCCEED

    pos= s1.Find("k",PR_FALSE,15,10); //first search from end using a LARGE count
    NS_ASSERTION(pos==15,"Error: Find() with offset and count");  //THIS WILL SUCCEED

    pos= s1.Find("k",PR_FALSE,25,10); //first search from bogus offset using a LARGE count
    NS_ASSERTION(pos==-1,"Error: Find() with offset and count");  //THIS WILL FAIL

    pos=10;
  }

        //now try substringsearching with RFind() using offset and count...
  {
    nsString s1("abcdefghijklmnopqrstuvwxyz");

    PRInt32 pos= s1.RFind("ghi"); //first search from end using count
    NS_ASSERTION(pos==6,"Error: RFind() with offset and count");  //THIS WILL SUCCEED!

    pos= s1.RFind("nop",PR_FALSE,-1,4); //first search from end using count
    NS_ASSERTION(pos==-1,"Error: RFind() with offset and count");  //THIS WILL FAIL

    pos= s1.RFind("nop",PR_FALSE,-1,15); //first search from end using count
    NS_ASSERTION(pos==13,"Error: RFind() with offset and count");  //THIS WILL SUCCEED

    pos= s1.RFind("nop",PR_FALSE,16,3); //first search from middle using count
    NS_ASSERTION(pos==-1,"Error: RFind() with offset and count");  //THIS WILL FAIL

    pos= s1.RFind("nop",PR_FALSE,16,7); //first search from middle using count
    NS_ASSERTION(pos==13,"Error: RFind() with offset and count");  //THIS WILL SUCCEED

    pos= s1.RFind("nop",PR_FALSE,0,1); //first search from front using count
    NS_ASSERTION(pos==-1,"Error: RFind() with offset and count");  //THIS WILL FAIL

    pos= s1.RFind("abc",PR_FALSE,0,1); //first search from middle using count
    NS_ASSERTION(pos==0,"Error: RFind() with offset and count");  //THIS WILL SUCCEED

    pos= s1.RFind("foo",PR_FALSE,10,100); //first search from front using bogus count
    NS_ASSERTION(pos==-1,"Error: RFind() with offset and count");  //THIS WILL FAIL

    pos= s1.RFind("ghi",PR_FALSE,30,1); //first search from middle using bogus offset
    NS_ASSERTION(pos==-1,"Error: RFind() with offset and count");  //THIS WILL FAIL

    pos=10;
  }

    //Various UNICODE tests...
  {
      //do some searching against chinese unicode chars...

    PRUnichar chinese[] = {0x4e41,0x4e42, 0x4e43, 0x0000}; // 3 chinese  unicode

    nsString  T2(chinese);
    nsString  T2copy(chinese); 

    pos = T2.FindCharInSet("A");
    NS_ASSERTION(kNotFound==pos,"Error in FindCharInSet");

    pos=T2.RFindCharInSet("A",2);
    NS_ASSERTION(kNotFound==pos,"Error in RFindCharInSet");

    pos=T2.Find("A", PR_FALSE, 0, 1);
    NS_ASSERTION(kNotFound==pos,"Error in Find");

    pos=T2.RFind("A", PR_FALSE, 2, 1);
    NS_ASSERTION(kNotFound==pos,"Error in RFind");

    T2.ReplaceChar("A",' ');
    NS_ASSERTION(T2==T2copy,"Error in ReplaceChar");

      //Here's the 3rd FTang unicode test...

    static char test4[]="ABCDEF";
    static char test4b[]=" BCDEF";
    static PRUnichar test5[]={0x4e41, 0x0000};
    static PRUnichar test6[]={0x0041, 0x0000};
 
    static nsCAutoString T4(test4);
    static nsCAutoString T4copy(test4);
    static nsCAutoString T4copyb(test4b);
    static nsAutoString T5(test5);
    static nsAutoString T6(test6);
 
    pos = T4.FindCharInSet(T5.GetUnicode());
    if(kNotFound != pos)
       printf("nsCString::FindCharInSet(const PRUnichar*) error- found when it should not\n");

    if(0 != T4.FindCharInSet(T6.GetUnicode()))
       printf("nsCString::FindCharInSet(const PRUnichar*) error- not found when it should\n");


    if(kNotFound != T4.RFindCharInSet(T5.GetUnicode(),2))
       printf("nsCString::RFindCharInSet(const PRUnichar*) error- found when it should not\n");

    if(0 != T4.RFindCharInSet(T6.GetUnicode(),2))
       printf("nsCString::RFindCharInSet(const PRUnichar*) error- not found when it should\n");

    if(kNotFound != T4.Find(T5.GetUnicode(), PR_FALSE, 0, 1))
       printf("nsCString::Find(const PRUnichar*) error- found when it should not\n");

    if(0 != T4.Find(T6.GetUnicode(), PR_FALSE, 0, 1))
       printf("nsCString::Find(const PRUnichar*) error- not found when it should\n");

    #if 0 // nsCString::Rfind(const PRUnichar* ...) is not available somehow
    if(kNotFound != T4.RFind(T5.GetUnicode(), PR_FALSE, 2, 1))
       printf("nsCString::RFind(const PRUnichar*) error- found when it should not\n");
    else
       printf("nsCString::RFind(const PRUnichar*) ok\n");
    if(0 != T4.RFind(T6.GetUnicode(), PR_FALSE, 2, 1))
       printf("nsCString::RFind(const PRUnichar*) error- not found when it should\n");
    else
       printf("nsCString::RFind(const PRUnichar*) ok\n");
    #endif

    T4.ReplaceChar(PRUnichar(0x4E41),PRUnichar(' '));
    if(T4 != T4copy)
       printf("nsCString::ReplaceChar(PRUnichar, PRUnichar) error- replace when it should not\n");

    T4 = test4;
    T4.ReplaceChar(PRUnichar(0x0041),PRUnichar(' '));
    if(T4 != T4copyb)
       printf("nsCString::ReplaceChar(PRUnichar, PRUnichar) error- not replace when it should\n");
  } 

    return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestConstructors(){
  int result=0;
  PRUnichar  pbuf[10]={'f','o','o',0};
  PRUnichar*  buf=pbuf;


  nsString  s1("hello world");
  nsCString c1("what's up");

  stringtype temp0; 
  stringtype temp1(s1);
  NS_ASSERTION(temp1==s1,"Constructor error");

  stringtype temp2(c1);
  NS_ASSERTION(temp2==c1,"Constructor error");
  
  stringtype temp3("hello world");
  NS_ASSERTION(temp3=="hello world","Constructor error");
  
  stringtype temp4(pbuf);
  NS_ASSERTION(temp4==pbuf,"Constructor error");
  
  stringtype temp5('a');
  NS_ASSERTION(temp5=="a","Constructor error");
  
  stringtype temp6(PRUnichar('a'));
  NS_ASSERTION(temp5=="a","Constructor error");

    //now just for fun, let's construct 2byte from 1byte and back...
  {
    nsCString temp0("hello");
    nsString  temp1(temp0);
    nsCString temp2(temp1);
  }

  return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestLogical(){
  int result=0;
  stringtype temp8("aaaa");
  stringtype temp8a("AAAA");
  stringtype  temp9("bbbb");

  const char* aaaa="aaaa";
  const char* bbbb="bbbb";


    //First, test a known error that got checked into the tree...
  {
    nsString mURL("file:///c|/test/foo.txt");
    PRBool result=mURL.Equals("file:/",PR_FALSE);
    NS_ASSERTION(!result,kEqualsError);
    result=mURL.Equals("file:/",PR_FALSE,5);
    NS_ASSERTION(result,kEqualsError);
    result=mURL.Equals("file:/",PR_FALSE,-1);
    NS_ASSERTION(!result,kEqualsError);

    nsString s1("rick");
    result=s1.Equals("rick",PR_FALSE,6);
    result++;
  }


  {
    //this little piece of code tests to see whether the PL_strcmp series works
    //correctly even when we have strings whose buffers contain nulls.

    char *buf1 = "what's up \0\0 doc?";
    char *buf2 = "what's up \0\0 dog?";

    nsString s1(buf2,17);

    PRInt32 result=s1.Compare(buf1,PR_TRUE,17);
    result=s1.FindChar('?');
    result++;  
  }

    //First test the string compare routines...

  NS_ASSERTION(0>temp8.Compare(bbbb),kComparisonError);
  NS_ASSERTION(0>temp8.Compare(temp9),kComparisonError);
  NS_ASSERTION(0<temp9.Compare(temp8),kComparisonError);
  NS_ASSERTION(0==temp8.Compare(temp8a,PR_TRUE),kComparisonError);
  NS_ASSERTION(0==temp8.Compare(aaaa),kComparisonError);

    //Now test the boolean operators...
  NS_ASSERTION(temp8==temp8,kComparisonError);
  NS_ASSERTION(temp8==aaaa,kComparisonError);

  NS_ASSERTION(temp8!=temp9,kComparisonError);
  NS_ASSERTION(temp8!=bbbb,kComparisonError);

  NS_ASSERTION(((temp8<temp9) && (temp9>=temp8)),kComparisonError);

  NS_ASSERTION(((temp9>temp8) && (temp8<=temp9)),kComparisonError);
  NS_ASSERTION(temp9>aaaa,kComparisonError);

  NS_ASSERTION(temp8<=temp8,kComparisonError);
  NS_ASSERTION(temp8<=temp9,kComparisonError);
  NS_ASSERTION(temp8<=bbbb,kComparisonError);

  NS_ASSERTION(((temp9>=temp8) && (temp8<temp9)),kComparisonError);
  NS_ASSERTION(temp9>=temp8,kComparisonError);
  NS_ASSERTION(temp9>=aaaa,kComparisonError);

  NS_ASSERTION(temp8.Equals(temp8),kEqualsError);
  NS_ASSERTION(temp8.Equals(aaaa),kEqualsError);
  
  stringtype temp10(temp8);
  temp10.ToUpperCase();
  NS_ASSERTION(temp8.Equals(temp10,PR_TRUE),kEqualsError);
  NS_ASSERTION(temp8.Equals("AAAA",PR_TRUE),kEqualsError);


    //now test the new string Equals APIs..
  {
    nsCString s1("hello there");
    NS_ASSERTION(s1.Equals("hello there"),kEqualsError);
    NS_ASSERTION(s1.Equals("hello rick",PR_FALSE,5),kEqualsError);
    NS_ASSERTION(!s1.Equals("hello rick",PR_FALSE-1),kEqualsError);

    nsCString s2("");
    NS_ASSERTION(s2.Equals(""),kEqualsError);

    nsCString s3("view-source:");
    NS_ASSERTION(s3.Equals("VIEW-SOURCE:",PR_TRUE,12),kEqualsError);
  }

    //now test the count argument...
  {
    nsString s1("rickgessner");
    nsString s2("rickricardo");
    PRInt32 result=s1.Compare(s2);  //assume no case conversion, and full-length comparison...
    result=s1.Compare(s2,PR_FALSE,4);
    result=s1.Compare(s2,PR_FALSE,5);

    PRBool  b=s1.Equals(s2);
    b=s1.Equals("rick",PR_FALSE,4);  
    b=s1.Equals("rickz",PR_FALSE,5);  

    result=10;

    nsString s3("view");

#define kString "view-source"

    b=s3.Equals(kString);
    result=10;
  }

  return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestAssignAndAdd(){
  int result=0;

  static const char* s1="hello";
  static const char* s2="world";
  static const PRUnichar pbuf[] = {' ','w','o','r','l','d',0};

  nsString ns1("I'm an nsString");
  nsCString nc1("I'm an nsCString");

  nsAutoString as1("nsAutoString source");
  nsCAutoString ac1("nsCAutoString source");


  {
      //****  Test assignments to nsString

    stringtype theDest;
    theDest.Assign(theDest);  //assign nsString to itself
    theDest.Assign(ns1);  //assign an nsString to an nsString
    NS_ASSERTION(theDest==ns1,"Assignment error");
    theDest.Assign(nc1);  //assign an nsCString to an nsString
    NS_ASSERTION(theDest==nc1,"Assignment error");
    theDest.Assign(as1); //assign an nsAutoString to an nsString
    NS_ASSERTION(theDest==as1,"Assignment error");
    theDest.Assign(ac1); //assign an nsCAutoString to an nsString
    NS_ASSERTION(theDest==ac1,"Assignment error");
    theDest.Assign("simple char*");  //assign a char* to an nsString
    NS_ASSERTION(theDest=="simple char*","Assignment error");
    theDest.Assign(pbuf);  //assign a PRUnichar* to an nsString
    NS_ASSERTION(theDest==pbuf,"Assignment error");
    theDest.Assign('!');  //assign a char to an nsString
    NS_ASSERTION(theDest=="!","Assignment error");
    theDest.Assign(PRUnichar('$'));  //assign a char to an nsString
    NS_ASSERTION(theDest=="$","Assignment error");

    theDest=ns1;
    NS_ASSERTION(theDest==ns1,"Assignment error");
    theDest=nc1;
    NS_ASSERTION(theDest==nc1,"Assignment error");
    theDest='a';
    NS_ASSERTION(theDest=="a","Assignment error");
    theDest=PRUnichar('a');
    NS_ASSERTION(theDest=="a","Assignment error");
    theDest=s1;
    NS_ASSERTION(theDest==s1,"Assignment error");
    theDest=pbuf;
    NS_ASSERTION(theDest==pbuf,"Assignment error");

  } 

    //test operator+()...
  {
    nsString s1("hello");
    nsString s2(" world");
    nsCString c1(" world");
    
    stringtype theDest;
    
    theDest=s1+s2;
    NS_ASSERTION(theDest=="hello world","Assignment error");

    theDest=s1+c1;
    NS_ASSERTION(theDest=="hello world","Assignment error");

    theDest=s1+" world";
    NS_ASSERTION(theDest=="hello world","Assignment error");

    theDest=s1+pbuf;
    NS_ASSERTION(theDest=="hello world","Assignment error");

    theDest=s1+'!';
    NS_ASSERTION(theDest=="hello!","Assignment error");

    theDest=s1+PRUnichar('!');
    NS_ASSERTION(theDest=="hello!","Assignment error");
    
  }

  return result;
}


/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestAppend(){
  int result=0;


  static const char* s1="hello";
  static const char* s2="world";
  static const PRUnichar pbuf[] = {' ','w','o','r','l','d',0};
  static const PRUnichar pbuf1[] = {'a','b','c','d','l','d'};


  stringtype theDest;
  theDest.Append((float)100.100);
  NS_ASSERTION(theDest=="100.1","Append(float) error");

  theDest.Truncate();
  theDest.Append(12345);
  NS_ASSERTION(theDest=="12345","Append(int) error");

  theDest.Truncate();
  theDest.Append(pbuf1,1);
  NS_ASSERTION(theDest=="a","Append(PRUnichar*,count) error");

  theDest.Truncate();
  theDest.Append('a');
  NS_ASSERTION(theDest=="a","Append(char) error");


  static const PRUnichar pbuf2[] = {'w','h','a','t','s',' ','u','p',' ','d','o','c','?',0};

  theDest.Truncate();
  theDest.Append(pbuf,20); //try appending more chars than actual length of pbuf
  NS_ASSERTION(theDest!=pbuf,"Append(PRUnichar*) error");
  //(NOTE: if you tell me to append X chars, I'll assume you really have X chars, and the length
  //       get's set accordingly. This test really is correct; it just seems odd.

  theDest.Truncate(0);
  theDest.Append(pbuf,5); //try appending fewer chars than actual length of pbuf
  NS_ASSERTION(theDest==" worl","Append(PRUnichar*) error");

  theDest.Truncate(0);
  theDest.Append(pbuf);    //try appending all of pbuf
  NS_ASSERTION(theDest==pbuf,"Append(PRUnichar*) error");

  char* ss=0;
  theDest.Truncate();
  theDest.Append(ss);       //try appending NULL
  NS_ASSERTION(theDest=="","Append(nullstr) error");

  theDest.Append(pbuf,0);   //try appending nothing
  NS_ASSERTION(theDest=="","Append(nullstr) error");


  {
    //test improvement to unichar appends...
    stringtype s1("hello");
    char c='!';
    s1+=c;
    NS_ASSERTION(s1=="hello!","operator+=() error");

    c=0xfa;
    s1+=c;
    s1.Append(c);

    PRUnichar theChar='f';
    s1+=theChar;

    char theChar2='g';
    s1+=theChar2;

    long theLong= 1234;
    s1+=theLong;

  }

  {
      //this just proves we can append nulls in our buffers...
    stringtype c("hello");
    stringtype s(" there");
    c.Append(s);
    char buf[]={'a','b',0,'d','e'};
    s.Append(buf,5);
  }

  {
    nsString foo;
    char c='a';
    for(int i=0;i<10;i++){
      foo+=c;
    }
    foo=(foo+'x');
  }


  stringtype temp2("there");

    theDest.Append(temp2);
    theDest.Append(" xxx ");
    theDest.Append(pbuf);
    theDest.Append('4');
    theDest.Append(PRUnichar('Z'));

    stringtype a(s1);
    stringtype b(s2);

    theDest.Truncate();
    temp2.Truncate();
    theDest=a+b;
    temp2=a+"world!";
    temp2=a+pbuf;
    stringtype temp3;
    temp3=temp2+'!';

  return result;
}


/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestCharAccessors(){
  int result=0;

  //first test the 2 byte version...


  {
    stringtype temp1("hello there rick");
    stringtype temp2;

    temp1.Left(temp2,10);
    temp1.Mid(temp2,6,5);
    temp1.Right(temp2,4);

      //Now test the character accessor methods...
    stringtype theString("hello");
    PRUint32 len=theString.Length();
    PRUnichar theChar;
    for(PRUint32 i=0;i<len;i++) {
      theChar=theString.CharAt(i);
    }
    theChar=theString.First();
    theChar=theString.Last();
    theChar=theString[3];
    theString.SetCharAt('X',3);
  }

    //now test the 1 byte version
  {
    stringtype temp1("hello there rick");
    stringtype temp2;
    temp1.Left(temp2,10);
    temp1.Mid(temp2,6,5);
    temp1.Right(temp2,4);

      //Now test the character accessor methods...
    stringtype theString("hello");
    PRUint32 len=theString.Length();
    PRUnichar ch;
    for(PRUint32 i=0;i<len;i++) {
      ch=theString.CharAt(i);
    }
    ch=theString.First();
    ch=theString.Last();
    ch=theString[3];
    theString.SetCharAt('X',3);
  }
  return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestCreators(){
  int result=0;

  {
    nsString  theString5("");
    char* str0=theString5.ToNewCString();
    Recycle(str0);

    theString5+="hello rick";
    nsString* theString6=theString5.ToNewString();
    delete theString6;

    nsString  theString1("hello again");
    PRUnichar* thePBuf=theString1.ToNewUnicode();
    if(thePBuf)
      Recycle(thePBuf);

    char* str=theString5.ToNewCString();
    if(str)
      Recycle(str);

    char buffer[6];
    theString5.ToCString(buffer,sizeof(buffer));
    
  }

  {
    nsCString  theString5("hello rick");
    nsCString* theString6=theString5.ToNewString();
    delete theString6;

    PRUnichar* thePBuf=theString5.ToNewUnicode();
    if(thePBuf)
      Recycle(thePBuf);

    char* str=theString5.ToNewCString();
    if(str)
      Recycle(str);

    char buffer[100];
    theString5.ToCString(buffer,sizeof(buffer)-1);
    nsCString  theOther=theString5.GetBuffer();
  }
  return result;
}


/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestInsert(){
  int result=0;

  //NOTE: You need to run this test program twice for this method to work. 
  //      Once with USE_WIDE defined, and once without (to test nsCString)

  {
    static const PRUnichar pbuf[] = {' ','w','o','r','l','d',0};

    nsString temp1("hello rick");
    temp1.Insert("there ",6); //char* insertion
    temp1.Insert(pbuf,3); //prunichar* insertion
    temp1.Insert("?",10); //char insertion
    temp1.Insert('*',10); //char insertion
    temp1.Insert("xxx",100,100); //this should append.
    stringtype temp2("abcdefghijklmnopqrstuvwxyz");
    temp2.Insert(temp1,10);
    temp2.Cut(20,5);
    temp2.Cut(100,100); //this should fail.

  }

  {
    static const PRUnichar pbuf[] = {' ','w','o','r','l','d',0};

    nsCString temp1("hello rick");
    temp1.Insert("there ",6); //char* insertion
    temp1.Insert(pbuf,3); //prunichar* insertion
    temp1.Insert("?",10); //char insertion
    temp1.Insert('*',10); //char insertion
    temp1.Insert("xxx",100,100); //this should append.
    stringtype temp2("abcdefghijklmnopqrstuvwxyz");
    temp2.Insert(temp1,10);
    temp2.Cut(20,5);
    temp2.Cut(100,100); //this should fail.

  }

      //test insert using mixed sized strings...
  {
    int theType0=eOneByte;
    int theType1=eOneByte;

      //This code will perform an append test for all combintations of char size...
    for(theType0=eOneByte;theType0<=eTwoByte;theType0++) {
      for(theType1=eOneByte;theType1<=eTwoByte;theType1++) {
        nsStr s0,s1;
        nsStr::Initialize(s0,eCharSize(theType0));        
        nsStr::Initialize(s1,eCharSize(theType1));
        getHello(s1,theType1);

        nsStr::Insert(s0,0,s1,0,5);  //insert some at front of empty string
        nsStr::Truncate(s0,0);
        nsStr::Insert(s0,3,s1,0,5);  //insert ALL in middle of empty string
        nsStr::Truncate(s0,0);
        nsStr::Insert(s0,4,s1,0,5);  //insert ALL at end of empty string
        nsStr::Truncate(s0,0);
        nsStr::Insert(s0,0,s1,0,5);  //init string to hello

        getAAA(s1,theType1);

        nsStr::Insert(s0,0,s1,0,5);  //insert some at front, but lie about length...
        nsStr::Insert(s0,7,s1,0,5);  //insert ALL in middle 
        nsStr::Insert(s0,14,s1,0,5);  //insert ALL at end 

        getBBB(s1,theType1);

        nsStr::Insert(s0,0,s1,2,3);  //insert SOME at front
        nsStr::Insert(s0,9,s1,1,2);  //insert SOME in middle
        nsStr::Insert(s0,20,s1,4,1);  //insert SOME at end 
        
        nsStr::Destroy(s0);
        nsStr::Destroy(s1);
        result=0;
      }
    }
  }

  return result;
}


/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestNSStr(){
  int result=0;
  
  //NOTE: You need to run this test program twice for this method to work. 
  //      Once with USE_WIDE defined, and once without (to test nsCString)

  int theType0=eOneByte;

      //This code will perform an append test for all combintations of char size...
  for(theType0=eOneByte;theType0<=eTwoByte;theType0++) {  
    {
      nsStr s0;
      nsStr s1;
      nsStr::Initialize(s0,eCharSize(theType0));
      nsStr::Initialize(s1,eCharSize(theType0));
      getNumbers(s1,theType0);
      nsStr::Assign(s0,s1,0,s1.mLength);

      nsStr::Delete(s0,100,1000); //should do nothing...
      nsStr::Delete(s0,0,1); //should delete 1st char...
      nsStr::Delete(s0,s0.mLength-1,1); //should delete last char...
      nsStr::Delete(s0,3,2); //should delete mid chars...
      nsStr::Delete(s0,3,4); //should delete mid chars...

      nsStr::Destroy(s0);
      nsStr::Destroy(s1);

      result=0;
    }
  }
  return result;
}
 
/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestDelete(){
  int result=0;
  
  //NOTE: You need to run this test program twice for this method to work. 
  //      Once with USE_WIDE defined, and once without (to test nsCString)

    //let's try some whacky string deleting calls
  {
    const PRUnichar pbuf[] = {'w','h','a','t','s',' ','u','p',' ','d','o','c','?',0};

    nsString s1(pbuf);
    s1.Cut(3,20); //try deleting more chars than actual length of pbuf
    s1=pbuf;
    s1.Cut(3,-10);
    s1=pbuf;
    s1.Cut(3,2);
  }

  {
    nsCAutoString s;
    for(int i=0;i<518;i++) {
      s.Append("0123456789");
    }
    s+="abc";
    s.Cut(0,5);
  }

  {
    astringtype ab("ab");
    stringtype abcde("cde");
    stringtype cut("abcdef");
    cut.Cut(7,10); //this is out of bounds, so ignore...
    //cut.DebugDump(cout);
    cut.Cut(5,2); //cut last chars
    //cut.DebugDump(cout);
    cut.Cut(1,1); //cut first char
    //cut.DebugDump(cout);
    cut.Cut(2,1); //cut one from the middle
    //cut.DebugDump(cout);
    cut="Hello there Rick";
    //cut.DebugDump(cout);
}

  return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestTruncate(){
  int result=0;
    //test delete against a twobyte string...

  int theType0=eOneByte;

  {
    nsCString s0;
    nsCString s1(kNumbers[0]);
    s0=s1;
    s0.Truncate(100); //should fail
    s0.Truncate(5); //should work
    s0.Truncate(0); //should work
  }

  {
    nsString s0;
    nsString s1(kNumbers[0]);
    s0=s1;
    s0.Truncate(100); //should fail
    s0.Truncate(5); //should work
    s0.Truncate(0); //should work
  }
  return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestNumerics(){
  int result=0;

  //try a few numeric conversion routines...
  {
    nsCString str1("-12345");
    nsString str2("hello");
    nsString str3;
    nsString str4("0");
    nsString str5("&#183;");
    nsString str6("&xi;");
    nsString str7("#xe3;");
    nsString str8("#FF;");
    nsCString str9("-#xa?a;"); //should return -10 (decimal)
    nsCString str10("&#191;");
    nsCString str11("&#vvv;");
    nsCString str12("-vvv;");
    nsCString str13("-f");

    PRInt32 err;
    PRInt32 theInt=str1.ToInteger(&err);
    theInt=str2.ToInteger(&err);
    theInt=str3.ToInteger(&err);
    theInt=str4.ToInteger(&err);
    theInt=str5.ToInteger(&err);
    theInt=str6.ToInteger(&err);
    theInt=str7.ToInteger(&err,16);
    theInt=str8.ToInteger(&err,-1);
    theInt=str9.ToInteger(&err,kAutoDetect);
    theInt=str10.ToInteger(&err);
    theInt=str11.ToInteger(&err);
    theInt=str12.ToInteger(&err);
    theInt=str13.ToInteger(&err);

    CStopwatch theSW2;
    printf("ToInteger()\n");
    for(int i=0;i<1000000;i++){
      theInt=str1.ToInteger(&err,16);
    }
    theSW2.Print();

    str1="100.100";
    float theFloat=str1.ToFloat(&err);
  }
  //try a few numeric conversion routines...
  {
    nsCString str1("10000");
    nsCString str2("hello");
    nsCString str3;
    PRInt32 err;
    PRInt32 theInt=str1.ToInteger(&err);
    theInt=str2.ToInteger(&err);
    theInt=str3.ToInteger(&err);

    str1="100.100";
    float theFloat=str1.ToFloat(&err);
  }
  return 0;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestLexomorphic(){
  int result=0;
    //test delete against a twobyte string...

  //NOTE: You need to run this test program twice for this method to work. 
  //      Once with USE_WIDE defined, and once without (to test nsCString)


  {
    nsString theTag("FOO");
    nsString theLower;
    theTag.ToLowerCase(theLower);
    int x=5;
  }
  
  int theType0=eOneByte;

  PRUnichar pbuf[] = {'h','e','l','l','o','\n','\n','\n','\n',250,'\n','\n','\n','\n','\n','\n','r','i','c','k',0};

    //and hey, why not do a few lexo-morphic tests...
  nsAutoString s0(pbuf);
  s0.ToUpperCase();
  s0.ToLowerCase();
  s0.StripChars("l");
  s0.StripChars("\n");
  s0.StripChar(0);

  {
    nsAutoString s1(pbuf);
    s1.CompressSet("\n ",' ');
  }

  s0.Trim(" ",PR_TRUE,PR_TRUE);
  s0.CompressWhitespace();
  s0.ReplaceChar('r','b');
  s0=pbuf;
  s0.ToUpperCase();
  s0.ToLowerCase();
  s0.Append("\n\n\n \r \r \r \t \t \t");
  s0.StripWhitespace();

  nsCAutoString s1("\n\r hello \n\n\n\r\r\r\t rick \t\t ");
  s1.ToUpperCase();
  s1.ToLowerCase();
  s1.StripChars("o");
  s1.Trim(" ",PR_TRUE,PR_TRUE);
  s1.CompressWhitespace();;
  s1.ReplaceChar('r','b');
  s1.ToUpperCase();
  s1.ToLowerCase();
  s1.Append("\n\n\n \r \r \r \t \t \t");
  s1.StripWhitespace();

  {
    nsCString temp("aaaa");
    for(int i=0;i<100;i++) {
      temp+="0123456789.";
    }
    temp.StripChars("a2468");
    i=5;

    temp="   hello rick    ";
    temp.StripChars("\n\r\t\b ");
  }

  return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestAutoStrings(){
  int result=0;

  PRUnichar pbuf[] = {'h','e','l','l','o',0};

  //this test makes sure that autostrings who assume ownership of a buffer, 
  //don't also try to copy that buffer onto itself... (was a bug)
  {
    nsAutoString  temp0; 
    nsAutoString  temp1("hello rick");
    nsAutoString  temp3(pbuf);
    nsAutoString  temp4(CBufDescriptor((char*)pbuf,PR_TRUE,5,5));
    nsAutoString  temp5(temp3);
    nsString      s(pbuf);
    nsAutoString  temp6(s);
    char buffer[500];

    nsAutoString  temp7(CBufDescriptor((PRUnichar*)buffer,PR_TRUE,sizeof(buffer)-10/2,0));
    temp7="hello, my name is inigo montoya.";

    nsAutoString as;
    for(int i=0;i<30;i++){
      as+='a';
    }
    bool b=true; //this line doesn't do anything, it just gives a convenient breakpoint.
  }

  {
    nsCAutoString  temp0; 
    nsCAutoString  temp1("hello rick");

    //nsCAutoString  temp2("hello rick",PR_TRUE,5);
    nsCAutoString  temp3(pbuf);

    {
      const char* buf="hello rick";
      int len=strlen(buf);
      nsAutoString   temp4(CBufDescriptor(buf,PR_TRUE,len+1,len));
    }

    nsCAutoString  temp5(temp3);
    nsString       s(pbuf);
    nsCAutoString  temp6(s);  //create one from a nsString
    char buffer[500];


    nsCAutoString as;
    for(int i=0;i<30;i++){
      as+='a';
    }
    bool b=true;

    nsCAutoString s3(CBufDescriptor(buffer,PR_TRUE,sizeof(buffer)-1,0));
    s3="hello, my name is inigo montoya.";

      //make an autostring that copies an nsString...
    nsString s0("eat icecream");
    nsCAutoString s4(s0); 
    nsCAutoString s5(s0.GetUnicode());

    nsString aaa("hi there rick");

  }

  const char* st="hello again";
  nsString s; //this also forces nsString to run it's selftest.
  s=st;
  nsAutoString astr;
  astr=st;

  return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestSubsumables(){
  int result=0;
  char* buf="hello rick";
  nsSubsumeStr s1(buf,false);
  nsString ns1(s1);

//  nsSubsumeCStr s2(ns1);
//  nsCString c1(s2);
  return result;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestRandomOps(){
  int result=0;

  return 0;

  char* str[]={"abc ","xyz ","123 ","789 ","*** ","... "};

  string    theSTLString;
  nsString theString;

  enum  ops {eAppend,eDelete,eInsert};
  
   srand( (unsigned)time( NULL ) );   

   int err[] = {1,7,9,6,0,4,1,1,1,1};

  for(int theOp=0;theOp<100000;theOp++){
    
    int r=rand();
    char buf[100];
    sprintf(buf,"%i",r);
    int len=strlen(buf);
    int index=buf[len-1]-'0';

    //debug... index=err[theOp];

    printf("%i\n",index);
    switch(index) {
      case 0:
      case 1:
      case 2:
        theSTLString.append(str[index]);
        theString.Append(str[index]);
        break;
      case 3:
      case 4:
      case 5:
        if (theString.mLength>2) {
          int pos=theString.mLength/2;
          theSTLString.insert(pos,str[index],4);
          theString.Insert(str[index],pos);
        }
        break;
      case 6:
      case 7:
      case 8:
      case 9:
        if(theString.mLength>10) {
          int len=theString.mLength/2;
          int pos=theString.mLength/4;
          theSTLString.erase(pos,len);
          theString.Cut(pos,len);
        }
        break;

    } //for
    if(!theString.Equals(theSTLString.c_str())) {
      printf("oops!\n");
      exit(1);
    }

    if(theString.mLength>300) {
      theString.Truncate();
      theSTLString.erase();
    }
  }

  return result;
}


/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestReplace(){
  int result=0;

  const char* find="..";
  const char* rep= "+";

  nsCString s("hello..there..rick..gessner.");
  s.ReplaceSubstring(find,rep);
  s.ReplaceSubstring(rep,find);

  return result;
}

/**
 * This method tests the performance of various methods.
 * 
 * 
 * @return
 */
int CStringTester::TestWideStringPerformance() {

  cout << endl << endl << "Widestring Performance Tests..." << endl;

  char* libname[] = {"STL","nsString",0};

    //**************************************************
    //Test append("abcde") against STL::wstring...
    //**************************************************
  {

    PRUnichar pbuf[10]={'a','b','c','d','e',0};
    
    CStopwatch theSW1;
    theSW1.Start();
    for(int i=0;i<1000;i++){
      nsString s;
      for(int j=0;j<200;j++){
        s.Append("abcde");
      }
    }
    theSW1.Stop();

    wchar_t wbuf[10] = {'a','b','c','d','e',0};

    CStopwatch theSW2;
    theSW2.Start();
    for(i=0;i<1000;i++){
      wstring s;
      for(int j=0;j<200;j++){
        s.append(wbuf);
      }
    }
    theSW2.Stop();
  
    cout << "  Append(\"abcde\"); nsString: " << theSW1.CpuTime() << " STL::wstring: " << theSW2.CpuTime() << endl;
    int x=0;
  }

    //**************************************************
    //Test append(char) against STL::wstring
    //**************************************************
  {

    CStopwatch theSW1;
    theSW1.Start();
    for(int i=0;i<500;i++){
      nsString s;
      for(int j=0;j<200;j++){
        s.Append('a');
      }
    }
    theSW1.Stop();


    CStopwatch theSW2;
    theSW2.Start();
    for(i=0;i<500;i++){
      wstring s;
      wchar_t theChar('a');
      for(int j=0;j<200;j++){
        s.append('a',1);
      }
    }
    theSW2.Stop();
  
    cout << "  Append('a'); nsString: " << theSW1.CpuTime() << " STL::wstring: " << theSW2.CpuTime() << endl;
    int x=0;
  }

    //**************************************************
    //Test insert("123") against STL::wstring
    //**************************************************
  {

    PRUnichar pbuf1[10]={'a','b','c','d','e','f',0};
    PRUnichar pbuf2[10]={'1','2','3',0};

    CStopwatch theSW1;
    theSW1.Start();
    for(int i=0;i<1000;i++){
      nsString s("abcdef");
      int inspos=3;
      for(int j=0;j<100;j++){
        s.Insert(pbuf2,inspos);
        inspos+=3;
      }
    }
    theSW1.Stop();


    CStopwatch theSW2;
    theSW2.Start();

    wchar_t wbuf1[10] = {'a','b','c','d','e','f',0};
    wchar_t wbuf2[10] = {'1','2','3',0};

    for(i=0;i<1000;i++){
      wstring s(wbuf1);
      int inspos=3;
      for(int j=0;j<100;j++){
        s.insert(inspos,wbuf2);
      }
    }
    theSW2.Stop();
  
    cout << "  Insert(\"123\"); nsString: " << theSW1.CpuTime() << " STL::wstring: " << theSW2.CpuTime() << endl;
    int x=0;
  }

    //**************************************************
    //Let's test substring searching performance...
    //**************************************************
  {
    PRUnichar pbuf1[] = {'a','a','a','a','a','a','a','a','a','a','b',0};
    PRUnichar pbuf2[]   = {'a','a','b',0};

    nsString s(pbuf1);
    nsString target(pbuf2);

    CStopwatch theSW1;
    theSW1.Start();
    for(int i=-1;i<1000000;i++) {
      PRInt32 result=s.Find(target,PR_FALSE,i);
    }
    theSW1.Stop();

    wchar_t wbuf1[] = {'a','a','a','a','a','a','a','a','a','a','b',0};
    wchar_t wbuf2[] = {'a','a','b',0};
    wstring ws(wbuf1);
    wstring wtarget(wbuf2);

    CStopwatch theSW2;
    theSW2.Start();
    for(i=-1;i<1000000;i++) {
      PRInt32 result=ws.find(wtarget,0);
    }
    theSW2.Stop();

    cout << "  Find(\"aab\"); nsString: " << theSW1.CpuTime() << " STL::wstring: " << theSW2.CpuTime() << endl;
  }

    //**************************************************
    //Now let's test comparisons...
    //**************************************************
  {
    nsString  s("aaaaaaaaaaaaaaaaaaab");
    PRUnichar target[]={'a','a','a','a','a','a','a','a','a','a','a','a','a','b',0};
    size_t theLen=(sizeof(target)-1)/2;

    CStopwatch theSW1;
    theSW1.Start();
    int result=0;
    for(int i=-1;i<1000000;i++) {
      result=s.Compare(target,PR_FALSE,theLen);
      result++;
    }
    theSW1.Stop();

    wchar_t buf[]={'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','b',0};
    wstring ws(buf);
    wchar_t wtarget[]={'a','a','a','a','a','a','a','a','a','a','a','a','a','b',0};

    CStopwatch theSW2;
    theSW2.Start();
    for(i=-1;i<1000000;i++) {
      result=ws.compare(0,theLen,wtarget);
      result++;
    }
    theSW2.Stop();

    cout << "  Compare(\"aaaaaaaaaaaaaab\"); nsString: " << theSW1.CpuTime() << " STL:wstring: " << theSW2.CpuTime() << endl;

  }

    //**************************************************
    //Now lets test string deletions...
    //**************************************************
  {

    int strcount=6000;
    int outerIter=200;
    int innerIter=1000;

    PRUnichar pbuf[] = {'1','2','3','4','5','6','7','8','9','0',0};
    nsString source1;  //build up our target string...
    for(int i=0;i<strcount;i++) {
      source1.Append(pbuf);
    }

    CStopwatch theSW1;
    theSW1.Start();

    for(i=0;i<outerIter;i++) {
    
      theSW1.Stop();  //don't time the building of our test string...
      nsString s1(source1);
      theSW1.Start();

      for(int j=0;j<1000;j++){
        s1.Cut(20,50);
      }
    }
    theSW1.Stop();


    wchar_t wbuf[] = {'1','2','3','4','5','6','7','8','9','0',0};
    wstring source2;  //build up our target string...
    for(i=0;i<strcount;i++) {
      source2.append(wbuf);
    }

    CStopwatch theSW2;
    theSW2.Start();

    for(i=0;i<outerIter;i++) {
    
      theSW2.Stop();  //don't time the building of our test string...
      wstring s2(source2);
      theSW2.Start();

      for(int j=0;j<1000;j++){
        s2.erase(20,50);
      }
    }
    theSW2.Stop();

    cout << "  Delete(...); nsString: " << theSW1.CpuTime() << " STL:wstring: " << theSW2.CpuTime() << endl;

  }

    //**************************************************
    //Now let's test the findChar routine...
    //**************************************************
  {

    nsString s1;
    for(int i=0;i<100;i++) {
      s1.Append("1234567890",10);
    }
    s1+="xyz";

    CStopwatch theSW1;
    theSW1.Start();

    for(i=0;i<100000;i++) {
      int f=s1.FindChar('z',PR_FALSE,0);
    }
    theSW1.Stop();


    wchar_t wbuf[] = {'1','2','3','4','5','6','7','8','9','0',0};
    wstring s2;
    for( i=0;i<100;i++) {
      s2.append(wbuf);
    }
    wchar_t wbuf2[] = {'x','y','z',0};
    s2.append(wbuf2);

    CStopwatch theSW2;
    theSW2.Start();

    for(i=0;i<100000;i++) {
      int f=s2.find_first_of('z',0);
    }
    theSW2.Stop();

    cout << "  FindChar(...); nsString: " << theSW1.CpuTime() << " STL:wstring: " << theSW2.CpuTime() << endl;

  }
  return 0;
}

/**
 * This method tests the performance of various methods.
 * 
 * 
 * @return
 */
int CStringTester::TestStringPerformance() {

  cout << endl << endl << endl << "CString Performance Tests..." << endl;

  char* libname[] = {"STL","nsString",0};

    //**************************************************
    //Test append("abcde") against STL...
    //**************************************************
  {
    
    CStopwatch theSW1;
    theSW1.Start();
    for(int i=0;i<1000;i++){
      nsCString s;
      for(int j=0;j<200;j++){
        s.Append("abcde");
      }
    }
    theSW1.Stop();

    CStopwatch theSW2;
    theSW2.Start();
    for(i=0;i<1000;i++){
      string s;
      for(int j=0;j<200;j++){
        s.append("abcde");
      }
    }
    theSW2.Stop();
  
    cout << "  Append(\"abcde\"); nsCString: " << theSW1.CpuTime() << " STL::string: " << theSW2.CpuTime() << endl;
    int x=0;
  }

    //**************************************************
    //Test append(char) against STL
    //**************************************************
  {

    CStopwatch theSW1;
    theSW1.Start();
    for(int i=0;i<500;i++){
      nsCString s;
      for(int j=0;j<200;j++){
        s.Append('a');
      }
    }
    theSW1.Stop();


    CStopwatch theSW2;
    theSW2.Start();
    for(i=0;i<500;i++){
      string s;
      wchar_t theChar('a');
      for(int j=0;j<200;j++){
        s.append('a',1);
      }
    }
    theSW2.Stop();
  
    cout << "  Append('a'); nsCString: " << theSW1.CpuTime() << " STL::string: " << theSW2.CpuTime() << endl;
    int x=0;
  }

    //**************************************************
    //Test insert("123") against STL
    //**************************************************
  {

    char pbuf1[10]={'a','b','c','d','e','f',0};
    char pbuf2[10]={'1','2','3',0};

    CStopwatch theSW1;
    theSW1.Start();
    for(int i=0;i<1000;i++){
      nsCString s("abcdef");
      int inspos=3;
      for(int j=0;j<100;j++){
        s.Insert(pbuf2,inspos);
        inspos+=3;
      }
    }
    theSW1.Stop();


    CStopwatch theSW2;
    theSW2.Start();
    for(i=0;i<1000;i++){
      string s(pbuf1);
      int inspos=3;
      for(int j=0;j<100;j++){
        s.insert(inspos,pbuf2);
      }
    }
    theSW2.Stop();
  
    cout << "  Insert(\"123\"); nsCString: " << theSW1.CpuTime() << " STL::string: " << theSW2.CpuTime() << endl;
    int x=0;
  }

    //**************************************************
    //Let's test substring searching performance...
    //**************************************************
  {
    char pbuf1[] = {'a','a','a','a','a','a','a','a','a','a','b',0};
    char pbuf2[]   = {'a','a','b',0};

    nsCString s(pbuf1);

    CStopwatch theSW1;
    theSW1.Start();
    for(int i=-1;i<1000000;i++) {
      PRInt32 result=s.Find(pbuf2,PR_FALSE,i);
    }
    theSW1.Stop();

    string ws(pbuf1);

    CStopwatch theSW2;
    theSW2.Start();
    for(i=-1;i<1000000;i++) {
      PRInt32 result=ws.find(pbuf2,0,3);
    }
    theSW2.Stop();

    cout << "  Find(\"aab\"); nsCString: " << theSW1.CpuTime() << " STL::string: " << theSW2.CpuTime() << endl;
  }


    //**************************************************
    //Now let's test comparisons...
    //**************************************************
  {
    nsCString  s("aaaaaaaaaaaaaaaaaaab");
    char*  target="aaaaaaaaaaaaab";
    size_t theLen=strlen(target);

    CStopwatch theSW1;
    theSW1.Start();
    int result=0;
    for(int i=-1;i<1000000;i++) {
      result=s.Compare(target,PR_FALSE,theLen);
      result++;
    }
    theSW1.Stop();

    string ws("aaaaaaaaaaaaaaaaaaab");

    CStopwatch theSW2;
    theSW2.Start();
    for(i=-1;i<1000000;i++) {
      result=ws.compare(0,theLen,target);
      result++;
    }
    theSW2.Stop();

    cout << "  Compare(\"aaaaaaaaaaaaaab\"); nsCString: " << theSW1.CpuTime() << " STL::string: " << theSW2.CpuTime() << endl;

  }

    //**************************************************
    //Now lets test string deletions...
    //**************************************************
  {

    int strcount=6000;
    int outerIter=200;
    int innerIter=1000;

    char* buffer = "1234567890";
    nsCString source1;  //build up our target string...
    for(int i=0;i<strcount;i++) {
      source1.Append(buffer);
    }

    CStopwatch theSW1;
    theSW1.Start();

    for(i=0;i<outerIter;i++) {
    
      theSW1.Stop();  //don't time the building of our test string...
      nsCString s1(source1);
      theSW1.Start();

      for(int j=0;j<innerIter;j++){
        s1.Cut(20,50);
      }
    }
    theSW1.Stop();


    string source2;  //build up our target string...
    for(i=0;i<strcount;i++) {
      source2.append(buffer);
    }

    CStopwatch theSW2;
    theSW2.Start();

    for(i=0;i<outerIter;i++) {
    
      theSW2.Stop();  //don't time the building of our test string...
      string s2(source2);
      theSW2.Start();

      for(int j=0;j<innerIter;j++){
        s2.erase(20,50);
      }
    }
    theSW2.Stop();

    cout << "  Delete(...); nsCString: " << theSW1.CpuTime() << " STL:string: " << theSW2.CpuTime() << endl;

  }


    //**************************************************
    //Now let's test the findChar routine...
    //**************************************************
  {

    nsCString s1;
    for(int i=0;i<100;i++) {
      s1.Append("1234567890",10);
    }
    s1+="xyz";

    CStopwatch theSW1;
    theSW1.Start();

    for(i=0;i<100000;i++) {
      int f=s1.FindChar('z',PR_FALSE,0);
    }
    theSW1.Stop();


    string s2;
    for( i=0;i<100;i++) {
      s2.append("1234567890");
    }
    s2.append("xyz");

    CStopwatch theSW2;
    theSW2.Start();

    for(i=0;i<100000;i++) {
      int f=s2.find_first_of('z',0);
    }
    theSW2.Stop();

    cout << "  FindChar(...); nsCString: " << theSW1.CpuTime() << " STL:string: " << theSW2.CpuTime() << endl;

  }

  return 0;
}

/**
 * This method is here as a place to put known regressions...
 * 
 * 
 * @return
 */
int CStringTester::TestRegressions() {

  nsString s("FTP: Netscape</a>");
  PRInt32 result=s.Find("</A>",PR_TRUE);

    //this was a known bug...
  {
    nsString s0("");
    PRInt32 pos=s0.RFind("-->");
    nsString s1("debug");
    pos=s1.RFind("b");
    pos=s1.RFind("de",PR_FALSE,1);
    pos=10;
  }

    //this was a known bug...
  {
    nsString s0("RDF:RDF");
    PRInt32 pos=s0.Find(":");
    pos=10;
  }

  return result;
}

#endif


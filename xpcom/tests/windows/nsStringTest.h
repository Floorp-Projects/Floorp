/********************************************************************************************
 *
 * MODULES NOTES:
 *
 *  This file is designed to help test the new nsString classes.
 * 
 * Contributor(s):
 *   Rick Gessner <rickg@netscape.com>
 * 
 * History:
 *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Flesh out the interface to be compatible with old library (rickg)
 *  
 ********************************************************************************************/

#ifndef _STRINGTEST
#define _STRINGTEST


#include "nsString.h"
#include "nsReadableUtils.h"
#include <time.h>

#define USE_STL 

#ifdef USE_STL
#include <string>
using namespace std;
#endif

#define USE_WIDE  1
#ifdef  USE_WIDE
  #define stringtype  nsString
  #define astringtype nsAutoString
  #define chartype    PRUnichar
#else
  #define stringtype  nsCString
  #define astringtype nsCAutoString
  #define chartype    char
#endif


#include <stdio.h>



static const char* kConstructorError = "constructor error";
static const char* kComparisonError  = "Comparision error!";
static const char* kEqualsError = "Equals error!";

static char* kNumbers[]={"0123456789","0\01\02\03\04\05\06\07\08\09\0\0\0"};
static char* kLetters[]={"abcdefghij","a\0b\0c\0d\0e\0f\0g\0h\0i\0j\0\0\0"};
static char* kAAA[]={"AAA","A\0A\0A\0\0\0"};
static char* kBBB[]={"BBB","B\0B\0B\0\0\0"};
static char* kHello[]={"hello","h\0e\0l\0l\0o\0\0\0"};
static char* kWSHello[]={"  hello  "," \0 \0h\0e\0l\0l\0o\0 \0 \0\0\0"};



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
    TestExtractors();
    TestSearching();
    TestSubsumables();
    TestRandomOps();
    TestReplace();
    TestRegressions();
    TestStringPerformance();
    TestWideStringPerformance();
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
    int TestExtractors();
    int TestSearching();
    int TestSubsumables();
    int TestRandomOps();
    int TestReplace();
    int TestStringPerformance();
    int TestWideStringPerformance();
    int TestRegressions();
};


class Stopwatch {
public:
  Stopwatch() {
    start=clock();
  }

  void Stop() {
    stop=clock();
  }

  double Elapsed() {
    return (double)(stop - start) / CLOCKS_PER_SEC;
  }

  void Print(const char* msg= "") {
    printf("%s %f\n",msg,Elapsed());
  }
  
  clock_t start,stop;
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
  NS_ASSERTION(pos==-1,"Error: RFind routine");


    //now try searching with FindChar using offset and count...
  {
    stringtype s1("hello there rick");

    PRInt32 pos=s1.FindChar('r');  //this will search from the beginning, and for the length of the string.
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

    pos=s1.RFindChar('i');  //this will search from the end, and for the length of the string.
    NS_ASSERTION(pos==13,"Error: RFindChar() with offset and count");

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
    
    PRUnichar test5[]={0x4e41, 0x0000};
    PRUnichar test6[]={0x0041, 0x0000};
 
    nsCString T4(test4);
    nsCString T4copy(test4);
    nsCString T4copyb(test4b);
    nsCString T5(test5);
    nsCString T6(test6);
 
    pos = T4.FindCharInSet(T5.get());
    NS_ASSERTION(0==pos,"Error in FindcharInSet"); //This should succeed.

    pos = T4.FindCharInSet(T6.get());
    NS_ASSERTION(kNotFound<pos,"Error in FindcharInSet");  //This should succeed.

    pos = T4.RFindCharInSet(T5.get(),2);
    NS_ASSERTION(0==pos,"Error in RFindCharInSet");  //This should fail.

    pos = T4.RFindCharInSet(T6.get(),2);
    NS_ASSERTION(kNotFound<pos,"Error in RFindCharInSet");  //This should fail.

    pos = T4.Find(T5.get(), PR_FALSE, 0, 1);
    NS_ASSERTION(0==pos,"Error in Find");  //This should succeed.

    pos= T4.Find(T6.get(), PR_FALSE, 0, 1);
    NS_ASSERTION(kNotFound<pos,"Error in Find"); //This should fail.

    pos = T4.RFind(T5.get(), PR_FALSE, 2, 1);
    NS_ASSERTION(kNotFound==pos,"Error in RFind");
    
    pos =T4.RFind(T6.get(), PR_FALSE, 2, 1);
    NS_ASSERTION(kNotFound==pos,"Error in RFind");


/*  
  NOT WORKING IN NEW STRING YET...

    T4.ReplaceChar(PRUnichar(0x4E41),PRUnichar(' '));
    if(T4 != T4copy)
       printf("nsCString::ReplaceChar(PRUnichar, PRUnichar) error- replace when it should not\n");

    T4 = test4;
    T4.ReplaceChar(PRUnichar(0x0041),PRUnichar(' '));
    if(T4 != T4copyb)
       printf("nsCString::ReplaceChar(PRUnichar, PRUnichar) error- not replace when it should\n");

*/

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


  //Test nsCString constructors...
  {
    nsCString temp0; 
    nsCString temp1(s1);
    NS_ASSERTION(temp1==s1,"nsCString Constructor error");

    nsCString temp2(c1);
    NS_ASSERTION(temp2==c1,"nsCString Constructor error");
  
    nsCString temp3("hello world");
    NS_ASSERTION(temp3=="hello world","nsCString Constructor error");
  
    nsCString temp4(pbuf);
    NS_ASSERTION(temp4==pbuf,"nsCString Constructor error");
  
    nsCString temp5('a');
    NS_ASSERTION(temp5=="a","nsCString Constructor error");
  
    nsCString temp6(PRUnichar('a'));
    NS_ASSERTION(temp5=="a","nsCString Constructor error");
  }

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

  {
    nsString foo("__moz_text");
    PRInt32 cmp=foo.Compare("pre",PR_FALSE,-1);
    cmp=cmp;
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
  ToUpperCase(temp10);
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
      //****  Test assignments to nsCString...

    nsCString theDest;
    theDest.Assign(theDest);  //assign nsString to itself
    theDest.Assign(ns1);  //assign an nsString to an nsString
    NS_ASSERTION(theDest==ns1,"Assignment error");

    theDest.Assign(nc1);  //assign an nsCString to an nsString
    NS_ASSERTION(theDest==nc1,"Assignment error");
    
    theDest.Assign(as1); //assign an nsAutoString to an nsString
//    NS_ASSERTION(theDest==as1,"Assignment error");
    
    theDest.Assign(ac1); //assign an nsCAutoString to an nsString
//    NS_ASSERTION(theDest==ac1,"Assignment error");
    
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

  /* NOT WORKING YET...
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
*/
    
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

    c=(char)0xfa;
    s1+=c;
    s1.Append(c);

    PRUnichar theChar='f';
    s1+=theChar;

    char theChar2='g';
    s1+=theChar2;

//    long theLong= 1234;
//    s1+=theLong;

  }

  {
      //this just proves we can append nulls in our buffers...
    stringtype c("hello");
    stringtype s(" there");
    c.Append(s);
    char buf[]={'a','b',0,'d','e'};
    s.Append(buf,5);
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

/* NOT WORKING YET...
    theDest=a+b;
    temp2=a+"world!";
    temp2=a+pbuf;
    stringtype temp3;
    temp3=temp2+'!';
*/
  return result;
}


/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
int CStringTester::TestExtractors(){
  int result=0;

  //first test the 2 byte version...


  {
    nsString temp1("hello there rick");
    nsString temp2;

    temp1.Left(temp2,10);
    NS_ASSERTION(temp2=="hello ther","Left() error");

    temp1.Mid(temp2,6,5);
    NS_ASSERTION(temp2=="there","Mid() error");

    temp1.Right(temp2,4);
    NS_ASSERTION(temp2=="rick","Right() error");

      //Now test the character accessor methods...
    nsString theString("hello");
    PRUint32 len=theString.Length();
    PRUnichar theChar;
    for(PRUint32 i=0;i<len;i++) {
      theChar=theString.CharAt(i);
    }

/* NOT WORKING YET...
    theChar=theString.First();
    theChar=theString.Last();
    theChar=theString[3];
    theString.SetCharAt('X',3);
*/
  }

    //now test the 1 byte version
  {
    nsCString temp1("hello there rick");
    nsCString temp2;
    temp1.Left(temp2,10);
    temp1.Mid(temp2,6,5);
    temp1.Right(temp2,4);

      //Now test the character accessor methods...
    nsCString theString("hello");
    PRUint32 len=theString.Length();
    char ch;
    for(PRUint32 i=0;i<len;i++) {
      ch=theString.CharAt(i);
    }

/* NOT WORKING YET...

    ch=theString.First();
    ch=theString.Last();
    ch=theString[3];
    theString.SetCharAt('X',3);
*/
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

/* NOT WORKING YET
  {
    nsString  theString5("");
    char* str0 = ToNewCString(theString5);
    Recycle(str0);

    theString5+="hello rick";
    nsString* theString6=theString5.ToNewString();
    delete theString6;

    nsString  theString1("hello again");
    PRUnichar* thePBuf=ToNewUnicode(theString1);
    if(thePBuf)
      Recycle(thePBuf);

    char* str = ToNewCString(theString5);
    if(str)
      Recycle(str);

    char buffer[6];
    theString5.ToCString(buffer,sizeof(buffer));
    
  }

  {
    nsCString  theString5("hello rick");
    nsCString* theString6=theString5.ToNewString();
    delete theString6;

    PRUnichar* thePBuf=ToNewUnicode(theString5);
    if(thePBuf)
      Recycle(thePBuf);

    char* str = ToNewCString(theString5);
    if(str)
      Recycle(str);

    char buffer[100];
    theString5.ToCString(buffer,sizeof(buffer)-1);
    nsCString  theOther=theString5.GetBuffer();
  }
  */
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

  {
    static const char pbuf[] = " world";

    nsCString temp1("hello rick");
    temp1.Insert("there ",6); //char* insertion
    NS_ASSERTION(temp1=="hello there rick","Insert error");

    temp1.Insert(pbuf,3); //prunichar* insertion
    NS_ASSERTION(temp1=="hel worldlo there rick","Insert error");

    temp1.Insert("?",10); //char insertion
    NS_ASSERTION(temp1=="hel worldl?o there rick","Insert error");

    temp1.Insert('*',10); //char insertion
    NS_ASSERTION(temp1=="hel worldl*?o there rick","Insert error");

    temp1.Insert("xxx",100,3); //this should append.
    NS_ASSERTION(temp1=="hel worldl*?o there rickxxx","Insert error");

    nsCString temp2("abcdefghijklmnopqrstuvwxyz");
//    temp2.Insert(temp1,10);
    temp2.Cut(20,5);
    NS_ASSERTION(temp2=="abcdefghijklmnopqrstz","Insert error");

    temp2.Cut(100,100); //this should fail.
    NS_ASSERTION(temp2=="abcdefghijklmnopqrstz","Insert error");

  }

  {
    static const PRUnichar pbuf[] = {' ','w','o','r','l','d',0};


    nsString temp1("llo rick");

    temp1.Insert("he",0); //char* insertion
    NS_ASSERTION(temp1=="hello rick","Insert error");

    temp1.Insert("there ",6); //char* insertion
    NS_ASSERTION(temp1=="hello there rick","Insert error");

    temp1.Insert(pbuf,3); //prunichar* insertion
    NS_ASSERTION(temp1=="hel worldlo there rick","Insert error");

    temp1.Insert("?",10); //char insertion
    NS_ASSERTION(temp1=="hel worldl?o there rick","Insert error");

    temp1.Insert('*',10); //char insertion
    NS_ASSERTION(temp1=="hel worldl*?o there rick","Insert error");

    temp1.Insert("xxx",100,3); //this should append.
    NS_ASSERTION(temp1=="hel worldl*?o there rickxxx","Insert error");

    nsString temp2("abcdefghijklmnopqrstuvwxyz");
//    temp2.Insert(temp1,10);
    temp2.Cut(20,5);
    NS_ASSERTION(temp2=="abcdefghijklmnopqrstz","Insert error");

    temp2.Cut(100,100); //this should fail.
    NS_ASSERTION(temp2=="abcdefghijklmnopqrstz","Insert error");

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
    const char* pbuf = "whats up doc?";

    nsCString s1(pbuf);
    s1.Cut(3,20); //try deleting more chars than actual length of pbuf
    NS_ASSERTION(s1=="wha","Cut error");

    s1=pbuf;
    s1.Cut(3,-10);
    NS_ASSERTION(s1==pbuf,"Cut error");

    s1=pbuf;
    s1.Cut(3,2);
    NS_ASSERTION(s1=="wha up doc?","Cut error");

  }

    //let's try some whacky string deleting calls
  {
    const PRUnichar pbuf[] = {'w','h','a','t','s',' ','u','p',' ','d','o','c','?',0};

    nsString s1(pbuf);
    s1.Cut(3,20); //try deleting more chars than actual length of pbuf
    NS_ASSERTION(s1=="wha","Cut error");

    s1=pbuf;
    s1.Cut(3,-10);
    NS_ASSERTION(s1==pbuf,"Cut error");

    s1=pbuf;
    s1.Cut(3,2);
    NS_ASSERTION(s1=="wha up doc?","Cut error");
  }


  {
    nsCString s;
    int i;
    for(i=0;i<518;i++) {
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
    NS_ASSERTION(str1=="-12345","Append(int) error");

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
    NS_ASSERTION(theInt==14,"ToInteger error");

    theInt=str3.ToInteger(&err);
    NS_ASSERTION(theInt==0,"ToInteger error");

    theInt=str4.ToInteger(&err);
    NS_ASSERTION(theInt==0,"ToInteger error");

    theInt=str5.ToInteger(&err);
    NS_ASSERTION(theInt==183,"ToInteger error");

    theInt=str6.ToInteger(&err);
    NS_ASSERTION(theInt==0,"ToInteger error");

    theInt=str7.ToInteger(&err,16);
    NS_ASSERTION(theInt==227,"ToInteger error");

    theInt=str8.ToInteger(&err,kAutoDetect);
    NS_ASSERTION(theInt==255,"ToInteger error");

    theInt=str9.ToInteger(&err,kAutoDetect);
    NS_ASSERTION(theInt==-10,"ToInteger error");

    theInt=str10.ToInteger(&err);
    NS_ASSERTION(theInt==191,"ToInteger error");

    theInt=str11.ToInteger(&err);
    NS_ASSERTION(theInt==0,"ToInteger error");

    theInt=str12.ToInteger(&err);
    NS_ASSERTION(theInt==0,"ToInteger error");

    theInt=str13.ToInteger(&err);
    NS_ASSERTION(theInt==-15,"ToInteger error");


    Stopwatch watch;
    int i;
    for(i=0;i<1000000;i++){
      theInt=str1.ToInteger(&err,16);
    }
    watch.Stop();
    watch.Print("ToInteger() ");

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
    NS_ASSERTION(theInt==10000,"ToInteger error");

    theInt=str2.ToInteger(&err);
    NS_ASSERTION(theInt==14,"ToInteger error");
    
    theInt=str3.ToInteger(&err);
    NS_ASSERTION(theInt==0,"ToInteger error");

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
  
  PRUnichar pbuf[] = {'h','e','l','l','o','\n','\n','\n','\n',250,'\n','\n','\n','\n','\n','\n','r','i','c','k',0};

    //and hey, why not do a few lexo-morphic tests...
  nsString s0(pbuf);
  ToUpperCase(s0);
  ToLowerCase(s0);
  s0.StripChars("l");
  s0.StripChars("\n");
  s0.StripChar(250);
  NS_ASSERTION(s0=="heorick","Stripchars error");

  {
    nsAutoString s1(pbuf);
    s1.CompressSet("\n ",' ');
  }

  {

    char pbuf[] = { 0x1C, 0x04, 0x1D, 0x04, 0x20, 0x00, 0x2D, 0x00, 0x20, 0x00, 0x23, 0x04, 0x20, 0x00, 0x40, 0x04,
                    0x43, 0x04, 0x3B, 0x04, 0x4F, 0x04, 0x20, 0x00, 0x30, 0x04, 0x40, 0x04, 0x3C, 0x04, 0x38, 0x04,
                    0x38, 0x04, 0x20, 0x00, 0x2D, 0x00, 0x20, 0x00, 0x32, 0x04, 0x42, 0x04, 0x3E, 0x04, 0x40, 0x04,
                    0x3E, 0x04, 0x41, 0x04, 0x42, 0x04, 0x35, 0x04, 0x3F, 0x04, 0x35, 0x04, 0x3D, 0x04, 0x3D, 0x04, 
                    0x4B, 0x04, 0x35, 0x04, 0x20, 0x00, 0x3B, 0x04, 0x38, 0x04, 0x46, 0x04, 0x30, 0x04, 0x20, 0x00,
                    0x2D, 0x00, 0x20, 0x00, 0x30, 0x00, 0x39, 0x00, 0x2F, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 
                    0x30, 0x00, 0x00, 0x00};

    nsAutoString temp((PRUnichar*)pbuf);
    nsAutoString temp1(temp);
    temp.CompressWhitespace();
    PRBool equals=temp.Equals(temp1);

  }

  {
    nsString s1("   ");
    s1.Trim("; \t");
    s1="helvetica";
    s1.Trim("; \t");
  }

  s0.Insert("  ",0);
  NS_ASSERTION(s0=="  heorick","Stripchars error");

  s0.Append("  ");
  NS_ASSERTION(s0=="  heorick  ","Stripchars error");
  
  s0.Trim(" ",PR_TRUE,PR_TRUE);
  NS_ASSERTION(s0=="heorick","Stripchars error");

  s0.Append("  abc  123  xyz  ");
  s0.CompressWhitespace();
  NS_ASSERTION(s0=="heorick abc 123 xyz","CompressWS error");

  s0.ReplaceChar('r','b');
  NS_ASSERTION(s0=="heobick abc 123 xyz","ReplaceChar error");

  s0=pbuf;
  ToUpperCase(s0);
  ToLowerCase(s0);
  s0.Append("\n\n\n \r \r \r \t \t \t");
  s0.StripWhitespace();

  nsCAutoString s1("\n\r hello \n\n\n\r\r\r\t rick \t\t ");
  ToUpperCase(s1);
  ToLowerCase(s1);
  s1.StripChars("o");
  s1.Trim(" ",PR_TRUE,PR_TRUE);
  s1.CompressWhitespace();
  NS_ASSERTION(s1=="hell rick","Compress Error");

  s1.ReplaceChar('h','w');
  NS_ASSERTION(s1=="well rick","Compress Error");

  ToUpperCase(s1);
  NS_ASSERTION(s1=="WELL RICK","Compress Error");

  ToLowerCase(s1);
  s1.Append("\n\n\n \r \r \r \t \t \t");
  s1.StripWhitespace();
  NS_ASSERTION(s1=="wellrick","Compress Error");

  {
    nsCString temp("aaaa");
    int i;
    for(i=0;i<100;i++) {
      temp+="0123456789.";
    }
    temp.StripChars("a2468");
    i=5;

    temp="   hello rick    ";
    temp.StripChars("\n\r\t\b ");
    NS_ASSERTION(temp=="hellorick","This isn't good");
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

/*  NOT WORKING...
    nsAutoString  temp4(CBufDescriptor((char*)pbuf,PR_TRUE,5,5));
*/
    nsAutoString  temp5(temp3);
    nsString      s(pbuf);
    nsAutoString  temp6(s);

/*  NOT WORKING...
    char buffer[500];
    nsAutoString  temp7(CBufDescriptor((PRUnichar*)buffer,PR_TRUE,sizeof(buffer)-10/2,0));
    temp7="hello, my name is inigo montoya.";
*/

    nsAutoString as;
    int i;
    for(i=0;i<30;i++){
      as+='a';
    }
    PRBool b=PR_TRUE; //this line doesn't do anything, it just gives a convenient breakpoint.
    NS_ASSERTION(as=="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","error in operator+=()");
  }


  {
    nsCAutoString  temp0; 
    nsCAutoString  temp1("hello rick");


    //nsCAutoString  temp2("hello rick",PR_TRUE,5);
    nsCAutoString  temp3(pbuf);

    {
      const char* buf="hello rick";
      int len=strlen(buf);

//       nsAutoString   temp4(CBufDescriptor(buf,PR_TRUE,len+1,len));  //NOT WORKING
    }

    nsCAutoString  temp5(temp3);
    nsString       s(pbuf);
    nsCAutoString  temp6(s);  //create one from a nsString

    nsCAutoString as;
    int i;
    for(i=0;i<30;i++){
      as+='a';
    }
    PRBool b=PR_TRUE;

#if 0

/* NOT WORKING
    char buffer[500];

    nsCAutoString s3(CBufDescriptor(buffer,PR_TRUE,sizeof(buffer)-1,0));
    s3="hello, my name is inigo montoya.";
  */
      //make an autostring that copies an nsString...
    nsString s0("eat icecream");
    nsCAutoString s4(s0); 
    nsCAutoString s5(s0.get());
    nsString aaa("hi there rick");
#endif
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

#if 0

  char* str[]={"abc ","xyz ","123 ","789 ","*** ","... "};

  string    theSTLString;
  nsString theString;
  nsString thePrevString;

  enum  ops {eNOP,eAppend,eDelete,eInsert};
  char* opStrs[] = {"nop","append","delete","insert"};
  
   srand( (unsigned)time( NULL ) );   

   int err[] = {1,7,9,6,0,4,1,1,1,1};

  ops theOp=eNOP;
  int pos,len,index;

  fstream output("c:/temp/out.file",ios::out);
  output<<"dump!";

  for(int theOpIndex=0;theOpIndex<1000000;theOpIndex++){
    
    int r=rand();
    char buf[100];
    sprintf(buf,"%i",r);
    len=strlen(buf);
    index=buf[len-1]-'0';

    //debug... index=err[theOp];

    switch(index) {
      case 0:
      case 1:
      case 2:
        theSTLString.append(str[index]);
        theString.Append(str[index]);
        theOp=eAppend;
        break;

      case 3:
      case 4:
      case 5:
        theOp=eInsert;
        if (theString.Length()>2) {
          pos=theString.Length()/2;
          theSTLString.insert(pos,str[index],4);
          theString.Insert(str[index],pos);
        }
        break;

      case 6:
      case 7:
      case 8:
      case 9:
        theOp=eDelete;
        if(theString.Length()>10) {
          len=theString.Length()/2;
          pos=theString.Length()/4;
          theSTLString.erase(pos,len);
          theString.Cut(pos,len);
        }
        break;

      default:
        theOp=eNOP;

    } //for

    if(eNOP<theOp) {

      int lendiff=theString.Length()-theSTLString.length();
      PRBool equals=theString.Equals(theSTLString.c_str());
      if(((lendiff)) || (!equals)) {
        printf("Error in %s at:%i len:%i (%s)!\n",opStrs[theOp],pos,len,str[index]);
        output.close();
        theString.Equals(theSTLString.c_str());

        if(theOp==eInsert) {
          thePrevString.Insert(str[index],pos);
        }
        else if(theOp==eAppend) {
          thePrevString.Append(str[index]);
        }
        return 0;
      }

#if FOO
      output<< opStrs[theOp];
      if((eInsert==theOp) || (eAppend==theOp)){
        output<< "(" << str[index] << ", " << pos << ") ";
      }

      thePrevString.ToCString(buffer,1000,0);
      output << " Old: [" << buffer << "]";
      theString.ToCString(buffer,1000,0);
      output << " New: [" << buffer << "]";
      output << " STL: [" << theSTLString.c_str() << "]" << endl;

      if(theString.mStringValue.mLength>300) {
        theString.Truncate();
        theSTLString.erase();
      }
#endif
      thePrevString=theString;
    }
  }
#endif
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

  const char* s1="hello..there..rick..gessner.";
  const char* s2="hello+there+rick+gessner.";

  nsCString s(s1);
  s.ReplaceSubstring(find,rep);
  NS_ASSERTION(s==s2,"ReplaceSubstring error");

  s.ReplaceSubstring(rep,find);
  NS_ASSERTION(s==s1,"ReplaceSubstring error");

  return result;
}


/**
 * This method tests the performance of various methods.
 * 
 * 
 * @return
 */
int CStringTester::TestWideStringPerformance() {

  printf("Widestring performance tests...\n");

  char* libname[] = {"STL","nsString",0};


    //**************************************************
    //Test Construction  against STL::wstring...
    //**************************************************
  {
    nsString theConst;
    for(int z=0;z<10;z++){
      theConst.Append("0123456789");
    }
    
    Stopwatch watch1;
    int i;
    for(i=0;i<1000000;i++){
      nsString s(theConst);
    }
    watch1.Stop();

    wchar_t wbuf[] = {'a','b','c','d','e','f','g','h','i','j',0};
    wstring theConst2;
    for(int w=0;w<10;w++){
      theConst2.append(wbuf);
    }

    Stopwatch watch2;
#ifdef USE_STL
    for(i=0;i<1000000;i++){
      wstring s(theConst2);
    }
#endif
    watch2.Stop();

    printf("Construct(abcde)    NSString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
  }


    //**************************************************
    //Test append("abcde") against STL::wstring...
    //**************************************************
  {

    PRUnichar pbuf[10]={'a','b','c','d','e',0};
    
    Stopwatch watch1;
    int i;
    for(i=0;i<1000;i++){
      nsString s;
      for(int j=0;j<200;j++){
        s.Append("abcde");
      }
    }
    watch1.Stop();

    wchar_t wbuf[10] = {'a','b','c','d','e',0};

    Stopwatch watch2;
#ifdef USE_STL
    for(i=0;i<1000;i++){
      wstring s;
      for(int j=0;j<200;j++){
        s.append(wbuf);
      }
    }
#endif
    watch2.Stop();

    printf("Append(abcde)       NSString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
  }

    //**************************************************
    //Test append(char) against STL::wstring
    //**************************************************
  {

    Stopwatch watch1;
    int i;
    for(i=0;i<500;i++){
      nsString s;
      for(int j=0;j<200;j++){
        s.Append('a');
      }
    }
    watch1.Stop();


    Stopwatch watch2;
#ifdef USE_STL
    for(i=0;i<500;i++){
      wstring s;
      wchar_t theChar('a');
      for(int j=0;j<200;j++){
        s.append('a',1);
      }
    }
#endif
    watch2.Stop();
    
    printf("Append('a')         NSString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
    int x=0;
  }

    //**************************************************
    //Test insert("123") against STL::wstring
    //**************************************************
  {

    PRUnichar pbuf1[10]={'a','b','c','d','e','f',0};
    PRUnichar pbuf2[10]={'1','2','3',0};

    Stopwatch watch1;
    int i;
    for(i=0;i<1000;i++){
      nsString s("abcdef");
      int inspos=3;
      for(int j=0;j<100;j++){
        s.Insert(pbuf2,inspos);
        inspos+=3;
      }
    }
    watch1.Stop();


    wchar_t wbuf1[10] = {'a','b','c','d','e','f',0};
    wchar_t wbuf2[10] = {'1','2','3',0};

    Stopwatch watch2;
#ifdef USE_STL
    for(i=0;i<1000;i++){
      wstring s(wbuf1);
      int inspos=3;
      for(int j=0;j<100;j++){
        s.insert(inspos,wbuf2);
        inspos+=3;
      }
    }  
#endif
    watch2.Stop();

    printf("Insert(123)         NSString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
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

    Stopwatch watch1;
    int i;
    for(i=-1;i<200000;i++) {
      PRInt32 result=s.Find(target,PR_FALSE);
    }
    watch1.Stop();

    Stopwatch watch2;
#ifdef USE_STL
    wchar_t wbuf1[] = {'a','a','a','a','a','a','a','a','a','a','b',0};
    wchar_t wbuf2[] = {'a','a','b',0};
    wstring ws(wbuf1);
    wstring wtarget(wbuf2);

    for(i=-1;i<200000;i++) {
      PRInt32 result=ws.find(wtarget);
    }
#endif
    watch2.Stop();

    printf("Find(aab)           NSString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
  }

    //**************************************************
    //Now let's test comparisons...
    //**************************************************
  {
    nsString  s("aaaaaaaaaaaaaaaaaaab");
    PRUnichar target[]={'a','a','a','a','a','a','a','a','a','a','a','a','a','b',0};
    size_t theLen=(sizeof(target)-1)/2;

    Stopwatch watch1;
    int result=0;
    int i;
    for(i=-1;i<1000000;i++) {
      result=s.Compare(target,PR_FALSE,theLen);
      result++;
    }
    watch1.Stop();

    Stopwatch watch2;
#ifdef USE_STL
    wchar_t buf[]={'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','b',0};
    wstring ws(buf);
    wchar_t wtarget[]={'a','a','a','a','a','a','a','a','a','a','a','a','a','b',0};

    for(i=-1;i<1000000;i++) {
      result=ws.compare(0,theLen,wtarget);
      result++;
    }
#endif
    watch2.Stop();

    printf("Compare(aaaaaaaab)  NSString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());

  }

    //**************************************************
    //Now lets test string deletions...
    //**************************************************
  {

    int strcount=6000;
    int outerIter=100;
    int innerIter=100;

    PRUnichar pbuf[] = {'1','2','3','4','5','6','7','8','9','0',0};
    nsString source1;  //build up our target string...
    int i;
    for(i=0;i<strcount;i++) {
      source1.Append(pbuf);
    }

    Stopwatch watch1;

    for(i=0;i<outerIter;i++) {    
      nsString s1(source1);
      for(int j=0;j<100;j++){
        s1.Cut(20,50);
      }
    }
    watch1.Stop();
    printf("Cut(...)            NSString: %f ",watch1.Elapsed());

#ifdef USE_STL

    wchar_t wbuf[] = {'1','2','3','4','5','6','7','8','9','0',0};
    wstring source2;  //build up our target string...

    for(i=0;i<strcount;i++) {
      source2.append(wbuf);
    }

    Stopwatch watch2;

    for(i=0;i<outerIter;i++) {
      wstring s2(source2);
      for(int j=0;j<100;j++){
        s2.erase(20,50);
      }
    }
    watch2.Stop();

    printf(" STL: %f",watch2.Elapsed());
#endif
    printf("\n");

  }

    //**************************************************
    //Now let's test the findChar routine...
    //**************************************************
  {

    nsString s1;
    int i;
    for(i=0;i<100;i++) {
      s1.Append("1234567890",10);
    }
    s1+="xyz";

    Stopwatch watch1;
    for(i=0;i<100000;i++) {
      int f=s1.FindChar('z',PR_FALSE,0);
    }
    watch1.Stop();
    printf("FindChar('z')       NSString: %f",watch1.Elapsed());

#ifdef USE_STL
    wchar_t wbuf[] = {'1','2','3','4','5','6','7','8','9','0',0};
    wstring s2;
    for( i=0;i<100;i++) {
      s2.append(wbuf);
    }
    wchar_t wbuf2[] = {'x','y','z',0};
    s2.append(wbuf2);

    Stopwatch watch2;

    for(i=0;i<100000;i++) {
      int f=s2.find_first_of('z',0);
    }
    watch2.Stop();
    printf("  STL: %f",watch2.Elapsed());
#endif
    printf("\n");

  }
  return 0;
}


/************************************************************************************************
 * 
 *  This method tests the performance of various methods.
 * 
 ************************************************************************************************/
int CStringTester::TestStringPerformance() {
  printf("c-String performance tests...\n");

  char* libname[] = {"STL","nsString",0};

    //**************************************************
    //Test Construction  against STL::wstring...
    //**************************************************
  {
    nsCString theConst;
    for(int z=0;z<10;z++){
      theConst.Append("0123456789");
    }
    
    Stopwatch watch1;
    int i;
    for(i=0;i<1000000;i++){
      nsCString s(theConst);
    }
    watch1.Stop();

    char wbuf[] = {'a','b','c','d','e','f','g','h','i','j',0};
    string theConst2;
    for(int w=0;w<10;w++){
      theConst2.append(wbuf);
    }

    Stopwatch watch2;
#ifdef USE_STL
    for(i=0;i<1000000;i++){
      string s(theConst2);
    }
#endif
    watch2.Stop();

    printf("Construct(abcde)    NSCString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
  }

    //**************************************************
    //Test append("abcde") against STL...
    //**************************************************

  {
    
    Stopwatch watch1;
    int i;
    for(i=0;i<1000;i++){
      nsCString s;
      for(int j=0;j<200;j++){
        s.Append("abcde",5);
      }
    }
    watch1.Stop();

    Stopwatch watch2;
#ifdef USE_STL
    for(i=0;i<1000;i++){
      string s;
      for(int j=0;j<200;j++){
        s.append("abcde",5);
      }
    }
#endif
    watch2.Stop();
  
    printf("Append(abcde)       NSCString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
    int x=0;
  }

    //**************************************************
    //Test append(char) against STL
    //**************************************************
  {

    Stopwatch watch1;
    int i;
    for(i=0;i<500;i++){
      nsCString s;
      for(int j=0;j<200;j++){
        s.Append('a');
      }
    }
    watch1.Stop();

    Stopwatch watch2;
#ifdef USE_STL
    for(i=0;i<500;i++){
      string s;
      wchar_t theChar('a');
      for(int j=0;j<200;j++){
        s.append('a',1);
      }
    }
#endif
    watch2.Stop();
  
    printf("Append('a')         NSCString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
    int x=0;
  }

    //**************************************************
    //Test insert("123") against STL
    //**************************************************
  {

    char pbuf1[10]={'a','b','c','d','e','f',0};
    char pbuf2[10]={'1','2','3',0};

    Stopwatch watch1;
    int i;
    for(i=0;i<1000;i++){
      nsCString s("abcdef");
      int inspos=3;
      for(int j=0;j<100;j++){
        s.Insert(pbuf2,inspos);
        inspos+=3;
      }
    }
    watch1.Stop();

    Stopwatch watch2;
#ifdef USE_STL
    for(i=0;i<1000;i++){
      string s(pbuf1);
      int inspos=3;
      for(int j=0;j<100;j++){
        s.insert(inspos,pbuf2);
        inspos+=3;
      }
    }
#endif
    watch2.Stop();
  
    printf("insert(123)         NSCString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
    int x=0;
  }

    //**************************************************
    //Let's test substring searching performance...
    //**************************************************
  {
    char *pbuf1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aab";
               
    char *pbuf2 = "aab";

    nsCString s(pbuf1);

    Stopwatch watch1;
    int i;
    for(i=-1;i<20000;i++) {
      PRInt32 result=s.Find(pbuf2,PR_FALSE);
    }
    watch1.Stop();

    Stopwatch watch2;
#ifdef USE_STL
    string ws(pbuf1);
    for(i=-1;i<20000;i++) {
      PRInt32 result=ws.find(pbuf2);
    }
#endif
    watch2.Stop();

    printf("Find(aab)           NSCString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
  }

    //**************************************************
    //Now let's test comparisons...
    //**************************************************
  {
    char*  target="aaaaaaaaaaaaab";
    size_t theLen=strlen(target);

    Stopwatch watch1;
    nsCString  s("aaaaaaaaaaaaaaaaaaab");
    int result=0;
    int i;
    for(i=-1;i<1000000;i++) {
      result=s.Compare(target,PR_FALSE,theLen);
      result++;
    }
    watch1.Stop();

    Stopwatch watch2;
#ifdef USE_STL
    string ws("aaaaaaaaaaaaaaaaaaab");
    for(i=-1;i<1000000;i++) {
      result=ws.compare(0,theLen,target);
      result++;
    }
#endif
    watch2.Stop();

    printf("Compare(aaaaaaaab)  NSCString: %f  STL: %f\n",watch1.Elapsed(),watch2.Elapsed());

  }

    //**************************************************
    //Now lets test string deletions...
    //**************************************************
  {

    int strcount=6000;
    int outerIter=100;
    int innerIter=100;

    char* buffer = "1234567890";
    nsCString source1;  //build up our target string...
    int i;
    for(i=0;i<strcount;i++) {
      source1.Append(buffer);
    }

    Stopwatch watch1;
    for(i=0;i<outerIter;i++) {    
      nsCString s1(source1);
      for(int j=0;j<innerIter;j++){
        s1.Cut(20,50);
      }
    }
    watch1.Stop();
    printf("Cut(...)            NSCString: %f ",watch1.Elapsed());

#ifdef USE_STL
    string source2;  //build up our target string...
    for(i=0;i<strcount;i++) {
      source2.append(buffer);
    }

    Stopwatch watch2;
    for(i=0;i<outerIter;i++) {
      string s2(source2);
      for(int j=0;j<innerIter;j++){
        s2.erase(20,50);
      }
    }
    watch2.Stop();

    printf(" STL: %f",watch2.Elapsed());
#endif
    printf("\n");
  }


    //**************************************************
    //Now let's test the findChar routine...
    //**************************************************
  {

    nsCString s1;
    int i;
    for(i=0;i<100;i++) {
      s1.Append("1234567890",10);
    }
    s1+="xyz";

    Stopwatch watch1;
    for(i=0;i<100000;i++) {
      int f=s1.FindChar('z',PR_FALSE,0);
    }
    watch1.Stop();
    printf("FindChar('z')       NSCString: %f ",watch1.Elapsed());

#ifdef USE_STL
    string s2;
    for( i=0;i<100;i++) {
      s2.append("1234567890");
    }
    s2.append("xyz");

    Stopwatch watch2;
    for(i=0;i<100000;i++) {
      int f=s2.find_first_of('z',0);
    }
    watch2.Stop();
    printf(" STL: %f\n",watch1.Elapsed(),watch2.Elapsed());
#endif
    printf("\n");

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


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

#ifndef _STRINGTEST2
#define _STRINGTEST2


#include "nsString.h"
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
const double gTicks = 1.0e-7;





/********************************************************
  
    This class's only purpose in life is to test the 
    netscape string library. We exercise the string
    API's here, and do a bit of performance testing
    against the standard c++ library string (from STL).

 ********************************************************/
class CStringTester {

public:

	int TestI18nString();
    
};




//test 1: unicode char is stripped correctly using StripChars()
void nsStringTest1(){
	PRUnichar test[]={0x0041,0x0051,0x0052,0x0000};
	nsString T(test);
	PRUnichar result[]={0x0051,0x0052,0x0000};
	nsString R(result);
	T.StripChars("ABC");
	NS_ASSERTION(T.Equals(R), "Test 1: Unicode comparison error");
}
//test 2: unicode char is not matched and stripped when high-order byte is not 0x00
void nsStringTest2(){
	PRUnichar test[]={0x4e41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[]={0x4e41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.StripChars("ABC");
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
	//test 3: unicode char is not matched and stripped when high-order byte is 0xFF
void nsStringTest3(){

	PRUnichar test[] =  {0xFF41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0xFF41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(test);
	T.StripChars("ABC");
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest4(){
	//test 4: unicode char is matched and stripped correctly using StripChar()
	PRUnichar test[] =  {0x0041,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	PRUnichar result[] = {0x0051,0x0052,0x0000};
	nsAutoString R(result);
	T.StripChar('A');
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest5(){
	//test 5: unicode char is not matched and stripped when high-order byte is not 0x00

	PRUnichar test[]={0x4e41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[]={0x4e41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.StripChar('A');
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest6(){
	//test 6: unicode char is not matched and stripped when high-order byte is 0xFF

	PRUnichar test[] =  {0xFF41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0xFF41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.StripChar('A');
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest7(){
	//test 7: unicode char is matched and replaced correctly using ReplaceChar()
	
	PRUnichar test[] =  {0x0041,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	PRUnichar result[] = {0x0050,0x0051,0x0052,0x0000};
	nsAutoString R(result);
	T.ReplaceChar('A',0x50);
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest8(){
	//test 8: unicode char is not matched or replaced when high-order byte != 0x00
	
	PRUnichar test[] =  {0x4e41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0x4e41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.ReplaceChar('A',0x50);
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest9(){
	//test 9: unicode char is not matched or replaced when high-order byte matches search char
	
	PRUnichar test[] =  {0x4150,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0x4150,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.ReplaceChar('A',0x50);
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest10(){
	//test 10: unicode char is not matched or replaced when high-order byte == 0xFF
	
	PRUnichar test[] =  {0xFFc1,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0xFFc1,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.ReplaceChar('A',0x50);
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}

void nsStringTest11(){
	//test 11: unicode char is correctly replaced when parameter 1 is a string
	
	PRUnichar test[] =  {0x0041,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	PRUnichar result[] = {0x0050,0x0051,0x0052,0x0000};
	nsAutoString R(result);
	T.ReplaceChar("ABC",0x50);
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest12(){
	//test 12: unicode char is not replaced when high-order byte != 0x00
	
	PRUnichar test[] =  {0x4e41,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0x4e41,0x0051,0x0052,0x0000};
	nsAutoString R(result);
 	T.ReplaceChar("ABC",0x50);
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");

}
void nsStringTest13(){
	//test 13: unicode char is not replaced when high-order byte matches char in search string
	PRUnichar test[] =  {0x4150,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0x4150,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.ReplaceChar("ABC",'T');
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");

}
void nsStringTest14(){
	PRUnichar test[] =  {0xFFc2,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0xFFc2,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.ReplaceChar("ABC",'T');
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest15(){
	PRUnichar test[] =  {0xFFc2,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0xFFc2,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
 	PRUnichar s = 0xc2; //compiler error on this line
	T.ReplaceChar(s,'T');
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}

void nsStringTest16(){
	/* TESTS for ReplaceSubstring()*/
	
	PRUnichar test[] =  {0x0041,0x0042,0x0043,0x0044,0x0045,0x0000};
	nsAutoString T(test);
    PRUnichar result[] = {0x0044,0x0045,0x0046,0x0044,0x0045};
	nsAutoString R(result);
	T.ReplaceSubstring("ABC","DEF");
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");

}
void nsStringTest17(){

	PRUnichar test[] =  {0x0041,0x4e42,0x0043,0x0044,0x0045,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0x0041,0x4e42,0x0043,0x0044,0x0045,0x0000};
	nsAutoString R(result);
	T.ReplaceSubstring("ABC","DEF");
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest18(){
	 /*TESTS for Trim()*/

 	PRUnichar test[] =  {0x0041,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
    PRUnichar result[] = {0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R (result);
	T.Trim("ABC");
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest19(){
	PRUnichar test[] =  {0x4e41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0x4e41,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString R(result);
	T.Trim("ABC");
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
}
void nsStringTest22(){
	PRUnichar test[] =  {0x4e51,0x4e52,0x4e53,0x4e41,0x0000};
	nsAutoString T(test);
	PRUnichar result[] =  {0x4e51,0x4e52,0x4e53,0x4e41,0x0000};
	nsAutoString R(result);
	T.Trim("ABC");
	NS_ASSERTION(T.Equals(R), "Unicode comparison error");

}
//void nsStringTest23(){
//	PRUnichar test[] =  {0x4e51,0x4e52,0x4e53,0x4e22,0x0000};
//	nsAutoString T(test);
// 	PRUnichar s(0x4e22);
//	PRUnichar result[] =  {0x4e51,0x4e52,0x4e53,0x0000};
//	nsAutoString R(result);
//	T.Trim(s,PR_TRUE,PR_TRUE,PR_FALSE);
//	NS_ASSERTION(T.Equals(R), "Unicode comparison error");
//}
void nsStringTest24(){
	/*TESTS for Find()*/
	PRUnichar test[] =  {0x0041,0x0042,0x0043,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.Find("ABC") == 0, "Unicode comparison error");

}
void nsStringTest25(){
	PRUnichar test[] =  {0x4e41,0x4e42,0x4e43,0x4e53,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.Find("ABC") == -1, "Unicode comparison error");

}
void nsStringTest26(){
	PRUnichar test[] =  {0xFFc1,0x0051,0x0052,0x0053,0x0000};
	nsAutoString T(test);
 	PRUnichar str[] = {0xc1,0x51,0x52};
	nsAutoString S(str);
	NS_ASSERTION(T.Find(S) == -1, "Unicode comparison error");
}
void nsStringTest27(){
    /*TESTS for FindChar()*/
  
	PRUnichar test[] =  {0x0041,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.FindChar('A') == 0, "Unicode comparison error");
}
void nsStringTest28(){
	PRUnichar test[] =  {0x4e41,0x4e42,0x4e43,0x4e53,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.FindChar('A') == -1, "Unicode comparison error");
}
void nsStringTest29(){
	PRUnichar test[] =  {0xFFc1,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.FindChar(0xc1) == -1, "Unicode comparison error");

}
void nsStringTest30(){
	PRUnichar test[] =  {0x4132,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.FindChar('A1') == -1, "Unicode comparison error");
}
	/*TESTS for FindCharInSet()*/
void nsStringTest31(){
	PRUnichar test[] =  {0x0041,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.FindCharInSet("ABC") == 0, "Unicode comparison error");
}
void nsStringTest32(){
	PRUnichar test[] =  {0x4e41,0x4e42,0x4e43,0x4e53,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.FindCharInSet("ABC") == -1, "Unicode comparison error");
	
}
void nsStringTest33(){
	PRUnichar test[] =  {0xFFc1,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
 	PRUnichar str[] = {0xc1,0x51,0x52};
	nsAutoString s(str);
	NS_ASSERTION(T.FindCharInSet(s) == -1, "Unicode comparison error");
}
void nsStringTest34(){
	PRUnichar test[] =  {0x4132,0x5132,0x5232,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.FindCharInSet("ABC") == -1, "Unicode comparison error");
}
	/*TESTS for RFind()*/
void nsStringTest35(){

	PRUnichar test[] =  {0x0051,0x0052,0x0041,0x0042,0x0043,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.RFind("ABC") == 2, "Unicode comparison error");
}
void nsStringTest36(){
	PRUnichar test[] =  {0x4e41,0x4e42,0x4e43,0x4e53,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.RFind("ABC") == -1, "Unicode comparison error");
}
void nsStringTest37(){
	PRUnichar test[] =  {0xFFc1,0xFFc2,0xFFc3,0x4e53,0x0000};
	nsAutoString T(test);
 	PRUnichar str[] = {0xc1,0xc2,0xc3};
	nsAutoString s(str);	
	NS_ASSERTION(T.RFind(s) == -1, "Unicode comparison error");
}
	/*TESTS for RFindCharInSet*/
void nsStringTest38(){

 	PRUnichar test[] =  {0x0041,0x0042,0x0043,0x0000};
	nsAutoString T(test);
	int res = T.RFindCharInSet("ABC");
	NS_ASSERTION(res==0, "Unicode comparison error");
}
void nsStringTest39(){
 	PRUnichar test[] =  {0x4e41,0x4e42,0x4e43,0x4e53,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.RFindCharInSet("ABC") == -1, "Unicode comparison error");
}
void nsStringTest40(){
 	PRUnichar test[] =  {0xFFc1,0x4e51,0x4e52,0x4e53,0x0000};
	nsAutoString T(test);
 	PRUnichar str[] = {0xc1,0xc2,0xc3};
	nsAutoString s(str);	
	NS_ASSERTION(T.RFindCharInSet(s) == -1, "Unicode comparison error");
}
void nsStringTest41(){
 	PRUnichar test[] =  {0x4132,0x0051,0x0052,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.RFindCharInSet("ABC") == -1, "Unicode comparison error");
}
	/*  TESTS for Compare() */
void nsStringTest42(){
 	PRUnichar test[] =  {0x0041,0x0042,0x0043,0x0000};
	nsAutoString T(test);
	NS_ASSERTION(T.Compare("ABC") == 0, "Unicode comparison error");
}				   
void nsStringTest43(){
 	PRUnichar test[] =  {0xc341,0xc342,0xc343};
	nsAutoString T(test);
	NS_ASSERTION(T.Compare("ABC") == 1, "Unicode comparison error");
}


int CStringTester::TestI18nString(){
	nsStringTest1();
	nsStringTest2();
	nsStringTest3();
	nsStringTest4();
	nsStringTest5();
	nsStringTest6();
	nsStringTest7();
	nsStringTest8();
	nsStringTest9();
	nsStringTest10();
	nsStringTest11();
	nsStringTest12();
	nsStringTest13();
	nsStringTest14();
	nsStringTest15();
	nsStringTest16();
	nsStringTest17();
	nsStringTest18();
	nsStringTest19();
	//nsStringTest20();
	//nsStringTest21();
	nsStringTest22();
	//nsStringTest23();
	nsStringTest24();
   	nsStringTest25();
	nsStringTest26();
	nsStringTest27();
	nsStringTest28();
	nsStringTest29();
	nsStringTest30();
	nsStringTest31();
	nsStringTest32();
	nsStringTest33();
	nsStringTest34();
	nsStringTest35();
	nsStringTest36();
	nsStringTest37();
	nsStringTest38();
	nsStringTest39();
	nsStringTest40();
	nsStringTest41();
	nsStringTest42();
	nsStringTest43();

	return 0;
}

#endif


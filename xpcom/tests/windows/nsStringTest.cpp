
#include <iostream.h>
#include "nsStr.h"
#include "nsStringTest2.h"
#include "nsString.h"
#include "nsReadableUtils.h"

int main(){

  nsString temp("\t\t\n\r\n\r25,* \t   \n\n \t");
  temp.CompressWhitespace();

  nsString temp1("\t\t\n\r\n\r25,* \t   \n\n \t");
  temp1.CompressSet("\t\n\r ",' ',false,false);

  nsString temp3("");
  char* s = ToNewCString(temp3);
  delete s;

  char* f = ToNewCString(nsAutoString(""));
  char* f1 = ToNewCString(nsCAutoString(""));
  delete f;
  delete f1;
  

  CStringTester gStringTester;

  gStringTester.TestI18nString();

  return 0;
}



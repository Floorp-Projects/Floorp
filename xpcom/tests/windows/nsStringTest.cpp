
#include <iostream.h>
#include "nsStr.h"
#include "nsStringTest2.h"
#include "nsString.h"

int main(){

  nsString temp("\t\t\n\r\n\r25,* \t   \n\n \t");
  temp.CompressWhitespace();

  nsString temp1("\t\t\n\r\n\r25,* \t   \n\n \t");
  temp1.CompressSet("\t\n\r ",' ',false,false);

  nsString temp3("");
  char* s=temp3.ToNewCString();
  delete s;

  char* f=nsAutoString("").ToNewCString();
  char* f1=nsCAutoString("").ToNewCString();
  delete f;
  delete f1;
  

  CStringTester gStringTester;

  gStringTester.TestI18nString();

  return 0;
}



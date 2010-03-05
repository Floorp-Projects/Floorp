#include <stdio.h>
 
int main()
{
   char text[20];
   fgets(text, sizeof text, stdin);
   return 0;
}


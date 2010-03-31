#include <prthread.h>
 
int main()
{
   while(1) PR_Sleep(PR_SecondsToInterval(10));
   return 0;
}


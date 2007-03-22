#include "nsXPCOM.h"
#include "nsVoidArray.h"
#include "nsString.h"
class nsIDocShell;

//XXXbz is this even used?  There is no DebugRobot() with this
//signature in the tree!

extern "C" NS_EXPORT int DebugRobot(
   nsVoidArray * workList,
   nsIDocShell * docShell,
   int iMaxLoads,
   char * verify_dir,
   void (*yieldProc )(const char *)
   );

int main(int argc, char **argv)
{
  nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
  if (NS_FAILED(rv)) {
    printf("NS_InitXPCOM2 failed\n");
    return 1;
  }

  nsVoidArray * gWorkList = new nsVoidArray();
  if(gWorkList) {
    int i;
    for (i = 1; i < argc; ++i) {
      nsString *tempString = new nsString;
      tempString->AssignWithConversion(argv[i]);
      gWorkList->AppendElement(tempString);
    }
  }

  return DebugRobot(gWorkList, nsnull, 50, nsnull, nsnull);
}


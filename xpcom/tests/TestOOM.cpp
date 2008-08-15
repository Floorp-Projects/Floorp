/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is a simple OOM Test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "nsXPCOM.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsIMemory.h"

int main(int argc, char **argv)
{
  nsCOMPtr<nsIMemory> mem;
  nsresult rv = NS_GetMemoryManager(getter_AddRefs(mem));

  if (!mem || NS_FAILED(rv))
  {
    printf("Could not get the memory manager\n");
    return -1;
  }

  // allocation note.  don't use nsIMemory to allocate,
  // because we want to test the isLowMemory predicate
  // without regard for the nsIMemory impelmentation (the
  // implementation might count bytes handed out.  however,
  // the predicate should work with out having to rely on
  // that.
  void *big_alloc = malloc(1024 * 1024 * 16);

  const int highpower = 500000;
  
  char* buffers[highpower];
  for (int i=0; i<highpower; i++)
    buffers[i] = nsnull;
  
  for (int i=0; i<highpower; i++)
  {
    PRBool lowMem = PR_FALSE;
    size_t s = 4096; //pow(2,i);
    buffers[i] = (char*) malloc(s);
    
    // You have to touch the buffer
    if (!buffers[i])
      printf("Could not allocate a buffer of size %ld\n", s);
    else
    {
      for (int j=0; j<s; j++)
        buffers[i][j] = 'a';
    }
   
    PRIntervalTime start = PR_IntervalNow();
    mem->IsLowMemory(&lowMem);
    PRIntervalTime cost = PR_IntervalNow() - start;
    
    
    printf("Total Allocated: %ld. \tLow Memory now?  %s\t Took (%d)\n",
           s*i,
	   lowMem ? "Yes" : "No",
	   PR_IntervalToMilliseconds(cost));
  
    if (lowMem)
      break;
  }

  for(int i=0; i<highpower; i++)
  {
    if (buffers[i])
      free(buffers[i]);
  }
  return 0;
}

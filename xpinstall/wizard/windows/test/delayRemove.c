#include <windows.h>
#include <stdio.h>

#define MAX_BUF 1024
#define  LEN_DELAYED_DELETE_PREFIX lstrlen("\\??\\")

void PrintSpecial(char *string, int stringBufSize);

void PrintSpecial(char *string, int stringBufSize)
{
  int i;
  int nullCount = 0;

  printf("\nlength: %d\n", stringBufSize);
  for(i = 0; i < stringBufSize; i++)
  {
    if(string[i] == '\0')
    {
      printf("{null}");
      nullCount++;
    }
    else
      printf("%c", string[i]);
  }
}

void RemoveDelayedDeleteFileEntries(const char *aPathToMatch)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD dwType = REG_NONE;
  DWORD oldMaxValueLen = 0;
  DWORD newMaxValueLen = 0;
  DWORD lenToEnd = 0;
  char  *multiStr = NULL;
  const char key[] = "SYSTEM\\CurrentControlSet\\Control\\Session Manager";
  const char name[] = "PendingFileRenameOperations";
  char  *pathToMatch;
  char  *lcName;
  char  *pName;
  char  *pRename;
  int   nameLen, renameLen;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ|KEY_WRITE, &hkResult) != ERROR_SUCCESS)
    return;

  dwErr = RegQueryValueEx(hkResult, name, 0, &dwType, NULL, &oldMaxValueLen);
  if (dwErr != ERROR_SUCCESS || oldMaxValueLen == 0 || dwType != REG_MULTI_SZ)
  {
    /* no value, no data, or wrong type */
    return;
  }

  multiStr = calloc(oldMaxValueLen, sizeof(BYTE));
  if (!multiStr)
    return;

  pathToMatch = strdup(aPathToMatch);
  if (!pathToMatch)
  {
      free(multiStr);
      return;
  }

  if (RegQueryValueEx(hkResult, name, 0, NULL, multiStr, &oldMaxValueLen) == ERROR_SUCCESS)
  {
       PrintSpecial(multiStr, oldMaxValueLen);
      // The registry value consists of name/newname pairs of null-terminated
      // strings, with a final extra null termination. We're only interested
      // in files to be deleted, which are indicated by a null newname.
      _mbslwr(pathToMatch);
      lenToEnd = newMaxValueLen = oldMaxValueLen;
      pName = multiStr;
      while(*pName && lenToEnd > 0)
      {
          // find the locations and lengths of the current pair. Count the
          // nulls,  we need to know how much data to skip or move
          nameLen = strlen(pName) + 1;
          pRename = pName + nameLen;
          renameLen = strlen(pRename) + 1;

          // How much remains beyond the current pair
          lenToEnd -= (nameLen + renameLen);

          if (*pRename == '\0')
          {
              // No new name, it's a delete. Is it the one we want?
              lcName = strdup(pName);
              if (lcName)
              {
                _mbslwr(lcName);
                if (strstr(lcName, pathToMatch))
                {
                  // It's a match--
                  // delete this pair by moving the remainder on top
                  memmove(pName, pRename + renameLen, lenToEnd);

                  // update the total length to reflect the missing pair
                  newMaxValueLen -= (nameLen + renameLen);

                  // next pair is in place, continue w/out moving pName
                  continue;
                }
              }
          }
          // on to the next pair
          pName = pRename + renameLen;
      }

      if (newMaxValueLen != oldMaxValueLen)
      {
          PrintSpecial(multiStr, newMaxValueLen);
          // We've deleted something, save the changed data
          RegSetValueEx(hkResult, name, 0, REG_MULTI_SZ, multiStr, newMaxValueLen);
          RegFlushKey(hkResult);
      }
  }

  RegCloseKey(hkResult);
  free(multiStr);
}

#if 0
void RemoveDelayedDeleteFileEntries(const char *aPathToMatch)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD oldMaxValueLen = 0;
  DWORD newMaxValueLen = 0;
  int   lenToEnd = 0;
  char  buf[MAX_BUF];
  char  *pathToMatch = NULL;
  char  *multiStr = NULL;
  char  key[] = "SYSTEM\\CurrentControlSet\\Control\\Session Manager";
  char  name[] = "PendingFileRenameOperations";
  char  *op;
  char  *np;
  char  *beginPath;
  BOOL  done = FALSE;

  pathToMatch = strdup(aPathToMatch);
  if(!pathToMatch)
    return;

  if((dwErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ|KEY_WRITE, &hkResult)) == ERROR_SUCCESS)
  {
    dwErr = RegQueryValueEx(hkResult, name, 0, NULL, NULL, &oldMaxValueLen);
    printf("maxValueLen0: %d\n", oldMaxValueLen);

    multiStr = calloc(oldMaxValueLen, sizeof(BYTE));
    dwErr = RegQueryValueEx(hkResult, name, 0, NULL, multiStr, &oldMaxValueLen);
    printf("maxValueLen1: %d\n", oldMaxValueLen);

    if(dwErr != ERROR_SUCCESS)
      printf("RegQueryValueEx() failed: %d\n", dwErr);


    PrintSpecial(multiStr, oldMaxValueLen);

    lenToEnd = newMaxValueLen = oldMaxValueLen;
    op = multiStr;
    if(op && (oldMaxValueLen > 4))
    {
      _mbslwr(pathToMatch);
      while(!done)
      {
        beginPath = op + LEN_DELAYED_DELETE_PREFIX; // skip the first 4 chars ('\\??\\') to get to the first char of path;
        np = beginPath + lstrlen(beginPath) + 1;
        lstrcpy(buf, beginPath);
        _mbslwr(buf);
        lenToEnd = lenToEnd - lstrlen(op) + 2 - LEN_DELAYED_DELETE_PREFIX; // reset the lenToEnd
        if(strstr(buf, pathToMatch))
        {
          if(*np == '\0') // delete found
          {
            newMaxValueLen = newMaxValueLen - lstrlen(op) + 2 - LEN_DELAYED_DELETE_PREFIX; // reset the newMaxValueLen
            memmove(op, np + 1, lenToEnd);
          }
          else // rename found.  do nothing for renaming
          {
            printf("len of np: %d\n", lstrlen(np));
            lenToEnd = lenToEnd - lstrlen(np); // reset the lenToEnd
            op = np + lstrlen(np) + 1;
          }
        }
        else if(*np != '\0')
        {
          lenToEnd = lenToEnd - lstrlen(np); // reset the lenToEnd
          op = np + lstrlen(np) + 1;
        }
        else
          op = np + 1;

        if(*op == '\0') // terminating NULL byte indicating end of MULTISZ
          done = TRUE;
      }
    }

    PrintSpecial(multiStr, newMaxValueLen);

    dwErr = RegSetValueEx(hkResult,
                          name,
                          0,
                          REG_MULTI_SZ,
                          multiStr,
                          newMaxValueLen);
    if(dwErr != ERROR_SUCCESS)
      printf("RegQueryValueEx() failed: %d\n", dwErr);

    if(multiStr)
      free(multiStr);

    RegFlushKey(hkResult);
    RegCloseKey(hkResult);
  }

  if(pathToMatch)
    free(pathToMatch);
}
#endif

int main(argc, argv)
{
    char buf[MAX_BUF];
    DWORD bufSize = sizeof(buf);

//    MoveFileEx("c:\\windows\\system32\\mapistubs.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileEx("c:\\program files\\aol communicator\\phidlemon.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
//    MoveFileEx("c:\\windows\\system32\\mapistubs.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileEx("c:\\prOGRAM fILES\\aol COMMUNicator\\phidlemon.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
//    MoveFileEx("c:\\windows\\system32\\mapistubs.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileEx("srcfiletoremane.txt", "dstfilerenamed.txt", MOVEFILE_DELAY_UNTIL_REBOOT);

//    MoveFileEx("c:\\windows\\system32\\mapistubs.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileEx("c:\\program files\\aol communicator\\phidlemon.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
//    MoveFileEx("c:\\windows\\system32\\mapistubs.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileEx("c:\\program files\\aol communicator\\phidlemon.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
//    MoveFileEx("c:\\windows\\system32\\mapistubs.dll", NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileEx("srcfiletorename.txt", "dstfilerenamed.txt", MOVEFILE_DELAY_UNTIL_REBOOT);

    RemoveDelayedDeleteFileEntries("C:\\Program Files\\AOL Communicator\\");
}

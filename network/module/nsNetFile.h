#include "prio.h"
#include "nsINetFile.h"
#include "plhash.h"


#ifndef ns_net_file_h_
#define ns_net_file_h_

/* This is a singleton implementation. */

class nsNetFile : public nsINetFile {
public:
    NS_DECL_ISUPPORTS

    nsNetFile();

    // Convert a generic file names into a platform file path
    NS_IMETHOD GetFilePath(const char *aName, char **aRes);
    NS_IMETHOD GetTemporaryFilePath(const char *aName, char **aRes);
    NS_IMETHOD GetUniqueFilePath(const char *aName, char **aRes);
    NS_IMETHOD GetCacheFileName(char *aDirTok, char **aRes);

    // Open a file
    NS_IMETHOD OpenFile(const char *aPath, nsFileMode aMode,
                        nsFile** aRes);
    
    // Close a file
    NS_IMETHOD CloseFile(nsFile* aFile);

    // Read a file
    NS_IMETHOD FileRead(nsFile *aFile, char **aBuf, 
                        PRInt32 *aBuflen,
                        PRInt32 *aBytesRead);

    // Read a line from a file
    NS_IMETHOD FileReadLine(nsFile *aFile, char **aBuf,
                        PRInt32 *aBuflen,
                        PRInt32 *aBytesRead);

    // Write a file
    NS_IMETHOD FileWrite(nsFile *aFile, const char *aBuf, 
                         PRInt32 *aLen,
                         PRInt32 *aBytesWritten);

    // Sync a file with disk
    NS_IMETHOD FileSync(nsFile *aFile);

    // Remove a file
    NS_IMETHOD FileRemove(const char *aPath);

    // Rename a file
    NS_IMETHOD FileRename(const char *aPathOld, const char *aPathNew);

    /*
     * Directory Methods
     */

    // Open a directory
    NS_IMETHOD OpenDir(const char *aPath, nsDir** aRes);

    // Close a directory
    NS_IMETHOD CloseDir(nsDir *aDir);

    // Create a directory
    NS_IMETHOD CreateDir(const char *aPath, PRBool aRecurse);

    // Set the internal directories
    NS_IMETHOD SetDirectory(const char *aToken, const char *aDir);

    // Associate a filename with a token, and optionally a dir token.
    NS_IMETHOD SetFileAssoc(const char *aToken, const char *aFile, const char *aDirToken);

protected:
    virtual ~nsNetFile();

private:
    void GenerateGlobalRandomBytes(void *aDest, size_t aLen);
    PRIntn convertToPRFlag(nsFileMode aMode);
    char * URLSyntaxToLocal(const char * aPath);
    char mTokDel;            // token delimiter
    char mDirDel;            // directory delimiter
    PLHashTable *mHTDirs;      // directory registry
    PLHashTable *mHTFiles;     // file registry
};

// The one and only singleton pointer to an nsNetFile.
static nsNetFile *gNetFile = nsnull;

//
// Class to manage static initialization
//
struct nsNetFileInit {
  nsNetFileInit() {
    gNetFile = nsnull;
    (void) NS_InitINetFile();
  }

  ~nsNetFileInit() {
    NS_ShutdownINetFile();
    gNetFile = nsnull;
  }
};

#endif // ns_net_file_h_
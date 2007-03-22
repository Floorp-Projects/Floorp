//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Effective-TLD Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pamela Greene <pamg.bugs@gmail.com> (original author)
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

// This service reads a file of rules describing TLD-like domain names.  For a
// complete description of the expected file format and parsing rules, see
// http://wiki.mozilla.org/Gecko:Effective_TLD_Service

#include "nsEffectiveTLDService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDataHashtable.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsFileStreams.h"
#include "nsIFile.h"
#include "nsIIDNService.h"
#include "nsNetUtil.h"
#include "nsString.h"

// The file name of the list of TLD-like names.  A file with this name in the
// system "res" directory will always be used.  In addition, if a file with
// the same name is present in the user's profile directory, its contents will
// also be used, as though those rules were appended to the system file.
#define EFF_TLD_FILENAME NS_LITERAL_CSTRING("effective_tld_names.dat")

// Define this as nonzero to eanble logging of the tree state after the file has
// been loaded.
#define EFF_TLD_SERVICE_DEBUG 0

/*
  The list of subdomain rules is stored as a wide tree of SubdomainNodes,
  primarily to facilitate multiple levels of wildcards.  Each node represents
  one level of a particular rule in the list, and stores meta-information about
  the rule it represents as well as a list (hash) of all the subdomains beneath
  it.

  stopOK: If true, this node marks the end of a rule.
  exception: If true, this node marks the end of an exception rule.

  For example, if the effective-TLD list contains

     foo.com
     *.bar.com
     !baz.bar.com

  then the subdomain tree will look like this (conceptually; the actual order
  of the nodes in the hashes is not guaranteed):

  +--------------+
  | com          |
  | exception: 0 |        +--------------+
  | stopOK: 0    |        | foo          |
  | children ----|------> | exception: 0 |
  +--------------+        | stopOK: 1    |
                          | children     |
                          +--------------+
                          | bar          |
                          | exception: 0 |        +--------------+
                          | stopOK: 0    |        | *            |
                          | children ----|------> | exception: 0 |
                          +--------------+        | stopOK: 1    |
                                                  | children     |
                                                  +--------------+
                                                  | baz          |
                                                  | exception: 1 |
                                                  | stopOK: 1    |
                                                  | children     |
                                                  +--------------+

*/
struct SubdomainNode;
typedef nsDataHashtable<nsCStringHashKey, SubdomainNode *> SubdomainNodeHash;
struct SubdomainNode {
  PRBool exception;
  PRBool stopOK;
  SubdomainNodeHash children;
};

static void EmptyEffectiveTLDTree();
static nsresult LoadEffectiveTLDFiles();
static nsresult NormalizeHostname(nsCString &aHostname);
static PRInt32 FindEarlierDot(const nsACString &aHostname, PRInt32 aDotLoc);
static SubdomainNode *FindMatchingChildNode(SubdomainNode *parent,
                                            const nsCSubstring &aSubname,
                                            PRBool aCreateNewNode);

nsEffectiveTLDService* nsEffectiveTLDService::sInstance = nsnull;
static SubdomainNode sSubdomainTreeHead;

NS_IMPL_ISUPPORTS1(nsEffectiveTLDService, nsIEffectiveTLDService)

// ----------------------------------------------------------------------

#if defined(EFF_TLD_SERVICE_DEBUG) && EFF_TLD_SERVICE_DEBUG
// LOG_EFF_TLD_NODE
//
// If debugging is enabled, this function is called by LOG_EFF_TLD_TREE to
// recursively print the current state of the effective-TLD tree.
PR_STATIC_CALLBACK(PLDHashOperator)
LOG_EFF_TLD_NODE(const nsACString& aKey, SubdomainNode *aData, void *aLevel)
{
  // Dump this node's information.
  PRInt32 *level = (PRInt32 *) aLevel;
  for (PRInt32 i = 0; i < *level; i++)    printf("-");
  if (aData->exception)                   printf("!");
  if (aData->stopOK)                      printf("^");
  printf("'%s'\n", PromiseFlatCString(aKey).get());

  // Dump its children's information.
  PRInt32 newlevel = (*level) + 1;
  aData->children.EnumerateRead(LOG_EFF_TLD_NODE, &newlevel);

  return PL_DHASH_NEXT;
}
#endif // EFF_TLD_SERVICE_DEBUG

// LOG_EFF_TLD_TREE
//
// If debugging is enabled, prints the current state of the effective-TLD tree.
void LOG_EFF_TLD_TREE(const char *msg = "",
                  SubdomainNode *head = &sSubdomainTreeHead)
{
#if defined(EFF_TLD_SERVICE_DEBUG) && EFF_TLD_SERVICE_DEBUG
  if (msg && msg != "")
    printf("%s\n", msg);

  PRInt32 level = 0;
  head->children.EnumerateRead(LOG_EFF_TLD_NODE, &level);
#endif // EFF_TLD_SERVICE_DEBUG

  return;
}

// ----------------------------------------------------------------------

// nsEffectiveTLDService::Init
//
// Initializes the root subdomain node and loads the effective-TLD file.
// Returns NS_OK upon successful completion, propagated errors if any were
// encountered during the loading of the file and construction of the
// effective-TLD tree.
nsresult
nsEffectiveTLDService::Init()
{
  sSubdomainTreeHead.exception = PR_FALSE;
  sSubdomainTreeHead.stopOK = PR_FALSE;
  nsresult rv = NS_OK;
  if (!sSubdomainTreeHead.children.Init())
    rv = NS_ERROR_OUT_OF_MEMORY;

  if (NS_SUCCEEDED(rv))
    rv = LoadEffectiveTLDFiles();

  // In the event of an error, clear any partially constructed tree.
  if (NS_FAILED(rv))
    EmptyEffectiveTLDTree();

  return rv;
}

// nsEffectiveTLDService::nsEffectiveTLDService
//
nsEffectiveTLDService::nsEffectiveTLDService()
{
  NS_ASSERTION(!sInstance, "Multiple effective-TLD services being created");
  sInstance = this;
}

// nsEffectiveTLDService::~nsEffectiveTLDService
//
nsEffectiveTLDService::~nsEffectiveTLDService()
{
  NS_ASSERTION(sInstance == this, "Multiple effective-TLD services exist");
  EmptyEffectiveTLDTree();
  sInstance = nsnull;
}

// nsEffectiveTLDService::getEffectiveTLDLength
//
// The main external function: finds the length in bytes of the effective TLD
// for the given hostname.  This will fail, generating an error, if the 
// hostname is not UTF-8 or includes characters that are not valid in a URL.
NS_IMETHODIMP
nsEffectiveTLDService::GetEffectiveTLDLength(const nsACString &aHostname,
                                             PRUint32 *effTLDLength)
{
  // Calcluate a default length: either the level-0 TLD, or the whole string
  // length if no dots are present.
  PRInt32 nameLength = aHostname.Length();
  PRInt32 defaultTLDLength;
  PRInt32 dotLoc = FindEarlierDot(aHostname, nameLength - 1);
  if (dotLoc < 0) {
    defaultTLDLength = nameLength;
  }
  else {
    defaultTLDLength = nameLength - dotLoc - 1;
  }
  *effTLDLength = NS_STATIC_CAST(PRUint32, defaultTLDLength);

  // Create a mutable copy of the hostname and normalize it.  This will fail
  // if the hostname includes invalid characters.
  nsCAutoString normHostname(aHostname);
  nsresult rv = NormalizeHostname(normHostname);
  if (NS_FAILED(rv))
    return rv;

  // Walk the domain tree looking for matches at each level.
  SubdomainNode *node = &sSubdomainTreeHead;
  dotLoc = nameLength;
  while (dotLoc > 0) {
    PRInt32 nextDotLoc = FindEarlierDot(normHostname, dotLoc - 1);
    const nsCSubstring &subname = Substring(normHostname, nextDotLoc + 1,
                                            dotLoc - nextDotLoc - 1);
    SubdomainNode *child = FindMatchingChildNode(node, subname, false);
    if (nsnull == child)
      break;
    if (child->exception) {
      // Exception rules use one fewer levels than were matched.
      *effTLDLength = NS_STATIC_CAST(PRUint32, nameLength - dotLoc - 1);
      node = child;
      break;
    }
    // For a normal match, save the location of the last stop, in case we find
    // a non-stop match followed by a non-match further along.
    if (child->stopOK)
      *effTLDLength = NS_STATIC_CAST(PRUint32, nameLength - nextDotLoc - 1);
    node = child;
    dotLoc = nextDotLoc;
  }

  return NS_OK;
}

// ----------------------------------------------------------------------

PR_STATIC_CALLBACK(PLDHashOperator)
DeleteSubdomainNode(const nsACString& aKey, SubdomainNode *aData, void *aUser)
{
  // Neither aData nor its children should ever be null in a properly
  // constructed tree, but this function may also be called to clear a tree
  // that encountered errors during initialization.
  if (nsnull != aData && aData->children.IsInitialized()) {
    // Delete this node's descendants.
    aData->children.EnumerateRead(DeleteSubdomainNode, nsnull);

    // Delete this node's hash of children.
    aData->children.Clear();
  }

  return PL_DHASH_NEXT;
}

// EmptyEffectiveTLDTree
//
// Release the memory allocated by the static effective-TLD tree and reset the
// semaphore to indicate that the tree is not loaded.
void
EmptyEffectiveTLDTree()
{
  if (sSubdomainTreeHead.children.IsInitialized()) {
    sSubdomainTreeHead.children.EnumerateRead(DeleteSubdomainNode, nsnull);
    sSubdomainTreeHead.children.Clear();
  }
  return;
}

// NormalizeHostname
//
// Normalizes characters of hostname. ASCII names are lower-cased, and names
// using other characters are normalized with nsIIDNService::Normalize, which
// follows RFC 3454.
nsresult NormalizeHostname(nsCString &aHostname)
{
  nsresult rv = NS_OK;

  if (IsASCII(aHostname)) {
    ToLowerCase(aHostname);
  }
  else {
    nsCOMPtr<nsIIDNService> idnServ = do_GetService(NS_IDNSERVICE_CONTRACTID);
    if (idnServ)
      rv = idnServ->Normalize(aHostname, aHostname);
  }

  return rv;
}

// ----------------------------------------------------------------------

// FillStringInformation
//
// Fills a start pointer and length for the given string.
void
FillStringInformation(const nsACString &aString, const char **start,
                      PRUint32 *length)
{
  nsACString::const_iterator iter;
  aString.BeginReading(iter);
  *start = iter.get();
  *length = iter.size_forward();
  return;
}

// FindEarlierDot
//
// Finds the byte location of the next dot (.) at or earlier in the string than
// the given aDotLoc.  Returns -1 if no more dots are found.
PRInt32
FindEarlierDot(const nsACString &aHostname, PRInt32 aDotLoc)
{
  if (aDotLoc < 0)
    return -1;

  // Searching for '.' one byte at a time is fine since UTF-8 is a superset of
  // 7-bit ASCII.
  const char *start;
  PRUint32 length;
  FillStringInformation(aHostname, &start, &length);
  PRInt32 endLoc = length - 1;

  if (aDotLoc < endLoc)
    endLoc = aDotLoc;
  for (const char *where = start + endLoc; where >= start; --where) {
    if (*where == '.')
      return (where - start);
  }

  return -1;
}

// FindEndOfName
//
// Finds the byte loaction of the first space or tab in the string, or the
// length of the string (i.e., one past its end) if no spaces or tabs are found.
PRInt32
FindEndOfName(const nsACString &aHostname) {
  // Searching for a space or tab one byte at a time is fine since UTF-8 is a
  // superset of 7-bit ASCII.
  const char *start;
  PRUint32 length;
  FillStringInformation(aHostname, &start, &length);

  for (const char *where = start; where < start + length; ++where) {
    if (*where == ' ' || *where == '\t')
      return (where - start);
  }

  return length;
}

// FindMatchingChildNode
//
// Given a parent node and a candidate subname, searches the parent's children
// for a matching subdomain and returns a pointer to the matching node if one
// was found.  If no exact match was found and aCreateNewNode is true, creates
// a new child node for the given subname and returns that, or nsnull if an
// error was encountered (typically because of memory allocation failure) while
// creating the new child.
//
// If no exact match was found and aCreateNewNode is false, looks for a
// wildcard node (*) instead.  If no wildcard node is found either, returns
// nsnull.
//
// Typically, aCreateNewNode will be true when the subdomain tree is being
// built, and false when it is being searched to determine a hostname's
// effective TLD.
SubdomainNode *
FindMatchingChildNode(SubdomainNode *parent, const nsCSubstring &aSubname,
                      PRBool aCreateNewNode)
{
  // Make a mutable copy of the name in case we need to strip ! from the start.
  nsCAutoString name(aSubname);
  PRBool exception = PR_FALSE;

  // Is this node an exception?
  if (name.Length() > 0 && name.First() == '!') {
    exception = PR_TRUE;
    name.Cut(0, 1);
  }

  // Look for an exact match.
  SubdomainNode *result = nsnull;
  SubdomainNode *match;
  if (parent->children.Get(name, &match)) {
    result = match;
  }

  // If the subname wasn't found, optionally create it.
  else if (aCreateNewNode) {
    SubdomainNode *node = new SubdomainNode;
    if (node) {
      node->exception = exception;
      node->stopOK = PR_FALSE;
      if (node->children.Init() && parent->children.Put(name, node))
        result = node;
    }
  }

  // Otherwise, if we're not making new nodes, look for a wildcard child.
  else if (parent->children.Get(NS_LITERAL_CSTRING("*"), &match)) {
    result = match;
  }

  return result;
}

// AddEffectiveTLDEntry
//
// Adds the given domain name rule to the effective-TLD tree.
//
// CAUTION: As a side effect, the domain name rule will be normalized: in the
// case of ASCII, it will be lower-cased; otherwise, it will be normalized as
// an IDN.
nsresult
AddEffectiveTLDEntry(nsCString &aDomainName) {
  SubdomainNode *node = &sSubdomainTreeHead;

  // Normalize the domain name.
  nsresult rv = NormalizeHostname(aDomainName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 dotLoc = FindEndOfName(aDomainName);
  while (dotLoc > 0) {
    PRInt32 nextDotLoc = FindEarlierDot(aDomainName, dotLoc - 1);
    const nsCSubstring &subname = Substring(aDomainName, nextDotLoc + 1,
                                            dotLoc - nextDotLoc - 1);
    dotLoc = nextDotLoc;
    node = FindMatchingChildNode(node, subname, true);
    if (!node)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // The last node in an entry is by definition a stop-OK node.
  node->stopOK = true;

  return NS_OK;
}

// LocateEffectiveTLDFile
//
// Locates the effective-TLD file.  If aUseProfile is true, uses the file from
// the user's profile directory; otherwise uses the one from the system "res"
// directory.  Places nsnull in foundFile if the desired file was not found.
nsresult
LocateEffectiveTLDFile(nsCOMPtr<nsIFile>& foundFile, PRBool aUseProfile)
{
  foundFile = nsnull;

  nsCOMPtr<nsIFile> effTLDFile = nsnull;
  nsresult rv = NS_OK;
  PRBool exists = PR_FALSE;
  if (aUseProfile) {
    // Look for the file in the user's profile directory.
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(effTLDFile));
    // We allow a nonfatal error so that this component can be tested in an
    // xpcshell with no profile present.
    if (NS_FAILED(rv))
      return rv;
  }
  else {
    // Look for the file in the application "res" directory.
    rv = NS_GetSpecialDirectory(NS_OS_CURRENT_PROCESS_DIR,
                                getter_AddRefs(effTLDFile));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = effTLDFile->AppendNative(NS_LITERAL_CSTRING("res"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = effTLDFile->AppendNative(EFF_TLD_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = effTLDFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists)
    foundFile = effTLDFile;

  return rv;
}

// LoadOneEffectiveTLDFile
//
// Loads the contents of the given effective-TLD file, building the tree as it
// goes.
nsresult
LoadOneEffectiveTLDFile(nsCOMPtr<nsIFile>& effTLDFile)
{
  // Open the file as an input stream.
  nsCOMPtr<nsIInputStream> fileStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream),
                                           effTLDFile,
                                           0x01, // read-only mode
                                           -1,   // all permissions
                                           nsIFileInputStream::CLOSE_ON_EOF);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILineInputStream> lineStream = do_QueryInterface(fileStream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString lineData;
  PRBool moreData = PR_TRUE;
  NS_NAMED_LITERAL_CSTRING(commentMarker, "//");
  while (moreData) {
    rv = lineStream->ReadLine(lineData, &moreData);
    if (NS_SUCCEEDED(rv)) {
      if (! lineData.IsEmpty() &&
          ! StringBeginsWith(lineData, commentMarker)) {
        rv = AddEffectiveTLDEntry(lineData);
      }
    }
    else {
      NS_WARNING("Error reading effective-TLD file; attempting to continue");
    }
  }

  LOG_EFF_TLD_TREE("Effective-TLD tree:");

  return rv;
}

// LoadEffectiveTLDFiles
//
// Loads the contents of the system and user effective-TLD files.
nsresult
LoadEffectiveTLDFiles()
{
  nsCOMPtr<nsIFile> effTLDFile;
  nsresult rv = LocateEffectiveTLDFile(effTLDFile, false);

  // If we didn't find any system effective-TLD file, warn but keep trying.  We
  // can struggle along using the base TLDs.
  if (NS_FAILED(rv) || nsnull == effTLDFile) {
    NS_WARNING("No effective-TLD file found in system res directory");
  }
  else {
    rv = LoadOneEffectiveTLDFile(effTLDFile);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = LocateEffectiveTLDFile(effTLDFile, true);

  // Since the profile copy isn't strictly needed, ignore any errors trying to
  // find or read it, in order to allow testing using xpcshell.
  if (NS_FAILED(rv) || nsnull == effTLDFile)
    return NS_OK;

  return LoadOneEffectiveTLDFile(effTLDFile);
}

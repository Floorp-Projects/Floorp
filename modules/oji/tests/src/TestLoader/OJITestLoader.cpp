/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "OJITestLoader.h"
//#include <io.h>
#include "nsReadableUtils.h"

static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIOJITestLoaderIID, OJITESTLOADER_IID);

NS_IMPL_ISUPPORTS1(OJITestLoader, OJITestLoader)

OJITestLoader::OJITestLoader(void) 
{

  NS_INIT_REFCNT();

  TestResult* res = NULL;
  char** testCase = loadTestList();
  int i = 0;
  TEST_TYPE testType;

  if (!testCase) {
	fprintf(stderr, "ERROR: Can't load test list !\n");
	return;
  }
  if (!(fdResFile = PR_Open(OJI_TEST_RESULTS, PR_CREATE_FILE | PR_WRONLY, PR_IRWXU))) {
    fprintf(stderr, "ERROR: Can't open test results file !\n");
  }
  for(i=0; testCase[i]; i++) {
    res = NULL;
    switch(testType = getTestType(testCase[i])) {
    case(JNI):
      res = runTest(testCase[i], OJI_JNI_TESTS);
      break;
    case(JM):
      res = runTest(testCase[i], OJI_JM_TESTS);
      break;
    case(TM):
      res = runTest(testCase[i], OJI_TM_TESTS);
      break;
    case(LCM):
      res = runTest(testCase[i], OJI_LCM_TESTS);
      break;
    case(JMPTI):
      res = runTest(testCase[i], OJI_JMPTI_TESTS);
      break;
    default:
      fprintf(stderr, "Can't determine test type (%s)\n", testCase[i]);
    }  
    if (res)
      registerRes(res, testCase[i]);
    else
      registerRes(TestResult::FAIL("Test execution failed"), testCase[i]);
  }


}


TestResult* OJITestLoader::runTest(const char* testCase, const char* libName) {

  PRLibrary* lib = PR_LoadLibrary(libName);
  OJI_TESTPROC testProc = NULL; 

  if (lib) {
	testProc = (OJI_TESTPROC)PR_FindSymbol(lib, testCase);
	if (testProc) {
		return testProc();
	} else {
		fprintf(stderr, "WARNING: Can't find %s method in %s library\n", testCase, libName);
	}
  } else {
	fprintf(stderr, "WARNING: Can't load %s library\n", libName);
  }
  return NULL;
}


void OJITestLoader::registerRes(TestResult* res, char* tc){
	char *outBuf = (char*)calloc(1, res->comment.Length() + PL_strlen(tc) + 100);
	
	sprintf(outBuf, "%s: %s (%s)\n", tc, res->status?"PASS":"FAILED", NS_LossyConvertUCS2toASCII(res->comment).get());
	if (fdResFile) {	
		printf("%s", outBuf);	
		if (PR_Write(fdResFile, outBuf, PL_strlen(outBuf)) < PL_strlen(outBuf))
			fprintf(stderr, "WARNING: Can't write entire result message. Possibly not enough free disk space !\n");
	} else {
		printf("%s", outBuf);
	}
	free(outBuf);
}


OJITestLoader::~OJITestLoader() 
{
  if(fdResFile)
    PR_Close(fdResFile);
}


char** OJITestLoader::loadTestList() {
	struct stat st;
	FILE *file = NULL;
	char *content;
	int nRead, nTests, count = 0;
	char *pos, *pos1;
	char **testList;

	if (stat(OJI_TESTS_LIST, &st) < 0) {
		fprintf(stderr, "ERROR: can't get stat from file %s\n", OJI_TESTS_LIST);
		return NULL;
	}
	content = (char*)calloc(1, st.st_size+1);
	if ((file = fopen(OJI_TESTS_LIST, "r")) == NULL) {
		fprintf(stderr, "ERROR: can't open file %s\n", OJI_TESTS_LIST);
		return NULL;
	}
	if ((nRead = fread(content, 1, st.st_size, file)) < st.st_size) {
		fprintf(stderr, "WARNING: can't read entire file in text mode (%d of %d) !\n", nRead, st.st_size);
		//return;
	}
	content[nRead] = 0;
	//printf("File content: %s\n", content);
	fclose(file);

	//allocate maximal possible size
	nTests = countChars(content, '\n') + 2;
	printf("nTests = %d\n", nTests);
	testList = (char**)calloc(sizeof(char*), nTests);
	testList[0] = 0;
	pos = content;
	while((pos1 = PL_strchr(pos, '\n'))) {
		*pos1 = 0;
		if(PL_strlen(pos) > 0 && *pos != '#') {
		  //printf("First char: %c\n", *pos);
			testList[count++] = PL_strdup(pos);
		}
		pos = pos1+1;
		if (!(*pos)) {
			printf("Parser done: %d .. ", count);
			testList[count] = 0;
			printf("ok\n");
			break;
		}
	}
	//If there is no \n after last line
	if (PL_strlen(pos) > 0 && *pos != '#') {
		testList[count++] = PL_strdup(pos);
		testList[count] = 0;
	}
	//free(content);
	return testList;
	
}


int OJITestLoader::countChars(char* buf, char ch) {
	char *pos = buf;
	int count = 0;

	while((pos = PL_strchr(pos, ch))) { 
		pos++;
		count++;
	}
	return count;
} 



TEST_TYPE OJITestLoader::getTestType(char *tc) {
	char *buf = PL_strdup(tc);
	char *pos = PL_strchr(buf, '_');
	if (!pos)
		return (TEST_TYPE)-1;
	*pos = 0;
	for(int i=0; test_types[i]; i++) {
		if (!PL_strcmp(buf, test_types[i])) {
			free(buf);
			return (TEST_TYPE)i;
		}
	}
	free(buf);
	return (TEST_TYPE)-1;
}

NS_METHOD OJITestLoader::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr) {
	OJITestLoader *tl = new OJITestLoader();
	if (!aInstancePtr)
		return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(kISupportsIID)) 
		*aInstancePtr  = (void*)(nsISupports*)tl;
	if (aIID.Equals(kIOJITestLoaderIID))
		*aInstancePtr = (void*)tl;
	if(!(*aInstancePtr))
		return NS_ERROR_NO_INTERFACE;
	return NS_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "mkutils.h"

#include "imaphier.h"
#include "imap4pvt.h"
#include "imap.h"

ImapHierarchyMover::ImapHierarchyMover(
						 const char *destinationNodeName,
					     const char *sourceNodeName,
						 TNavigatorImapConnection &connection)
						 : fIMAPConnection(connection)
{
	fDestinationNodeName = XP_STRDUP(destinationNodeName);
	fSourceNodeName      = XP_STRDUP(sourceNodeName);
	fMailboxHierarchy    = XP_ListNew();
}



ImapHierarchyMover::~ImapHierarchyMover()
{
	// this is a leak, delete the structs
	if (fMailboxHierarchy)
		XP_ListDestroy(fMailboxHierarchy);
	FREEIF(fDestinationNodeName);
	FREEIF(fSourceNodeName);
}



void ImapHierarchyMover::AddToHierarchy(const char *nodeName)
{
	int stringLength = XP_STRLEN(nodeName);
	char *nodeTokens = new char [stringLength + 2];	// end the string in double null so AddToSubTree
	XP_STRCPY(nodeTokens, nodeName);				// will work
	*(nodeTokens + stringLength) = 0;				        
	*(nodeTokens + stringLength+1) = 0;				        
	AddToSubTree(nodeTokens, nodeName, fMailboxHierarchy);
	delete nodeTokens;
}


void ImapHierarchyMover::AddToSubTree(char *nodeTokens, const char *nodeName, XP_List *subTree)
{
	char *positionOfNodeTokens = XP_STRSTR(nodeName, nodeTokens);
	// make sure we find the last one
	char *currentPositionOfNodeTokens = positionOfNodeTokens;
	while (currentPositionOfNodeTokens && strlen(nodeTokens))
	{
		currentPositionOfNodeTokens = XP_STRSTR(currentPositionOfNodeTokens + 1, nodeTokens);
		if (currentPositionOfNodeTokens)
			positionOfNodeTokens = currentPositionOfNodeTokens;
	}
	
	char *placeHolderInTokenString = nil;
	char *currentNodeToken     = XP_STRTOK_R(nodeTokens, "/",&placeHolderInTokenString);
	if (currentNodeToken)
	{
		// find the current node, if it exists
		HierarchyNode *currentNode = NULL;
		int numberOfNodes = XP_ListCount(subTree);
		XP_Bool nodeFound = FALSE;
		while (numberOfNodes && !nodeFound)
		{
			currentNode = (HierarchyNode *) XP_ListGetObjectNum(subTree, numberOfNodes--);
			nodeFound = XP_STRCMP(currentNode->fNodeToken, currentNodeToken) == 0;
		}
		
		if (!nodeFound)
		{
			// create the node
			currentNode = (HierarchyNode *) XP_ALLOC(sizeof(HierarchyNode));
			if (currentNode)
			{
				currentNode->fPresentName = XP_STRDUP(nodeName);
				// less the remaining tokens
				*(currentNode->fPresentName + (positionOfNodeTokens-nodeName) + XP_STRLEN(currentNodeToken)) = 0;
				
				currentNode->fNodeToken = XP_STRDUP(currentNodeToken);
				currentNode->fSubFolders = XP_ListNew();
				XP_ListAddObjectToEnd(subTree, currentNode);
			}
		}
		
		// recurse!
		char *subNodeTokens = nodeTokens + XP_STRLEN(currentNodeToken) + 1;	// +1 see note in AddToHierarchy
		AddToSubTree(subNodeTokens, nodeName, currentNode->fSubFolders);
	}
}


// depth first, post process unsubscribe and delete everything including and below source node name
void ImapHierarchyMover::DestroyOldFolderTree(XP_List *subTree)
{
	int numberOfNodesAtThisLevel = XP_ListCount(subTree);
	for (int nodeIndex=numberOfNodesAtThisLevel; nodeIndex > 0; nodeIndex--)
	{
		HierarchyNode *currentNode = (HierarchyNode *) XP_ListGetObjectNum(subTree, nodeIndex);
		DestroyOldFolderTree(currentNode->fSubFolders);
		
		if (fIMAPConnection.GetServerStateParser().LastCommandSuccessful() &&
		    XP_STRSTR(currentNode->fPresentName, fSourceNodeName))
		{
			fIMAPConnection.Unsubscribe(currentNode->fPresentName);
			if (fIMAPConnection.GetServerStateParser().LastCommandSuccessful())
				fIMAPConnection.DeleteMailbox(currentNode->fPresentName);
		}
	}
}



// depth first, pre-process create, subscribe, and fill with messages
void ImapHierarchyMover::CreateNewFolderTree(const char *destinationNodeName, XP_List *subTree)
{
	int numberOfNodesAtThisLevel = XP_ListCount(subTree);
	for (int nodeIndex=1; nodeIndex <= numberOfNodesAtThisLevel; nodeIndex++)
	{
		HierarchyNode *currentNode = (HierarchyNode *) XP_ListGetObjectNum(subTree, nodeIndex);
			
		if (!XP_STRSTR(fSourceNodeName, currentNode->fPresentName) ||
		    !XP_STRCMP(fSourceNodeName, currentNode->fPresentName))	// if we are not yet at the node name, don't rename
		{
			char *newNodeName = (char *) XP_ALLOC(XP_STRLEN(destinationNodeName) +
			                              			XP_STRLEN(currentNode->fNodeToken) + 2);
			XP_STRCPY(newNodeName, destinationNodeName);
			XP_STRCAT(newNodeName, "/");
			XP_STRCAT(newNodeName, currentNode->fNodeToken);
			
			fIMAPConnection.CreateMailbox(newNodeName);
			if (fIMAPConnection.GetServerStateParser().LastCommandSuccessful())
			{
				fIMAPConnection.Subscribe(newNodeName);
				if (fIMAPConnection.GetServerStateParser().LastCommandSuccessful())
				{
					// close the current folder if one is selected
					if (fIMAPConnection.GetServerStateParser().GetIMAPstate() == 
					    TImapServerState::kFolderSelected)
						fIMAPConnection.Close();
						
					// select the old folder
					fIMAPConnection.SelectMailbox(currentNode->fPresentName);
					if (fIMAPConnection.GetServerStateParser().LastCommandSuccessful())
					{
						// copy its entire contents to the destination
						int numOfMsgs = fIMAPConnection.GetServerStateParser().NumberOfMessages();
						if (numOfMsgs)
						{
							char msgList[100];	// enough for trillions?
							sprintf(msgList, "1:%d", numOfMsgs);
							fIMAPConnection.Copy(msgList, newNodeName, FALSE);	// not uids
						}
						if (fIMAPConnection.GetServerStateParser().LastCommandSuccessful())
							fIMAPConnection.Close();
					}
				}
			}
			// recurse to my children
			if (fIMAPConnection.GetServerStateParser().LastCommandSuccessful())
				CreateNewFolderTree(newNodeName, currentNode->fSubFolders);
			FREEIF( newNodeName);
		}
		else
			CreateNewFolderTree(destinationNodeName, currentNode->fSubFolders);
	}
}


int ImapHierarchyMover::DoMove()
{
	int returnValue = 0;
	
	CreateNewFolderTree(fDestinationNodeName, fMailboxHierarchy);
	
	if (fIMAPConnection.GetServerStateParser().LastCommandSuccessful())
	{
		DestroyOldFolderTree(fMailboxHierarchy);
		if (!fIMAPConnection.GetServerStateParser().LastCommandSuccessful())
			returnValue = -1;
	}
	else
		returnValue = -1;
	
	return returnValue;
}


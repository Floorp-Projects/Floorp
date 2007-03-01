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
 * The Original Code is the Mozilla Penelope project.
 *
 * The Initial Developer of the Original Code is
 * QUALCOMM incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Mark Charlebois <mcharleb@qualcomm.com> original author
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

function getHTMLSelection(selection, bodyElement, rangeList)
{
    dump("getHTMLSelection\n");

    var hasBreak = false;

    // Find the containing paragraph
    for (var i=0; i<selection.rangeCount; i++)
    {
        var range = selection.getRangeAt(i);

        // Force to beginning of paragraph
        var startnode = range.startContainer;
        if (startnode.nodeType != Node.TEXT_NODE)
        {
            startnode = range.startContainer.childNodes[range.startOffset];
            dump("setting start to "+startnode.nodeName+"\n");
        }

        dump("----- DOM Tree --\n");
        dumpNode(startnode.parentNode);
        dump("Got selection as: "+range.startContainer.nodeName+" "+range.startOffset+", "+range.endContainer.nodeName+" "+range.endOffset+"\n");
        dump("----- initial HTML --\n");
        dump(range+"\n");
        dump("----- end initial HTML --\n");

        var endnode = range.endContainer;
        if (endnode.nodeType != Node.TEXT_NODE)
        {
            endnode = range.endContainer.childNodes[range.endOffset];
            dump("setting end to "+endnode.nodeName+"\n");
        }

        // if current node is a text node  
        if (startnode.nodeType == Node.TEXT_NODE)
        {
            dump("startnode is text node\n");
            // if there is a prior <br> sibling
            while (startnode.previousSibling != null)
            {
                dump("Checking for leading BR: "+startnode.nodeName+"\n");
                startnode = startnode.previousSibling;
                if (startnode.nodeName.toUpperCase() == "BR")
                {
                    dump("Found leading BR\n");
                    startnode = startnode.nextSibling;
                    hasBreak = true;
                    break;
                }
            }
            var offset = getMyOffset(startnode);
            range.setStart(startnode.parentNode, offset);
        }

        // if current node is a text node
        if (endnode.nodeType == Node.TEXT_NODE)
        {
            dump("endnode is text node\n");
            while (endnode.nextSibling != null)
            {
                dump("Checking for trailing BR: "+endnode.nodeName+"\n");
                endnode = endnode.nextSibling;
                if (endnode.nodeName.toUpperCase() == "BR")
                {
                    dump("Found trailing BR\n");
                    endnode = endnode.previousSibling;
                    hasBreak = true;
                    break;
                }
            }
            if (endnode.nodeType == Node.TEXT_NODE)
            {
                range.setEnd(endnode, endnode.length);
            }
            else
            {
                range.setEndAfter(endnode);
            }
        }

        dump("Set HTML selection to: "+range.startContainer.nodeName+" "+range.startOffset+", "+range.endContainer.nodeName+" "+range.endOffset+"\n");
        // Set the range to the paragraph container
        
        dump("----- final HTML --\n");
        dump(range+"\n");
        dump("----- end final HTML --\n");
        rangeList[i] = range;
    }
}

function updateSelection(range)
{
    dump("updateSelection\n");
    var focusedWindow = document.commandDispatcher.focusedWindow;
    var selection = focusedWindow.getSelection();

    var newrange = document.createRange();
    newrange.setStart(range.startContainer, 0);

    // Extend selection to the end of line if this is a text node
    if (range.endContainer.nodeType == Node.TEXT_NODE)
    {
        newrange.setEnd(range.endContainer, range.endContainer.length);
    }
    else
    {
        newrange.setEnd(range.endContainer, range.endOffset);
    }

    // Replace the old selected range with the new range
    //selection.removeRange(range);
    //selection.addRange(newrange);

    return newrange;
}

function expandSelection(selection, rangeList)
{
    dump("resetSelection\n");
    var newrange = null;

    // Get the containing node
    for (var i=0; i < selection.rangeCount; i++)
    {
        rangeList[i] = selection.getRangeAt(i);
    }

    // Repopulate the selection
    for(var i=0; i < rangeList.length; i++)
    {
        rangeList[i] = updateSelection(rangeList[i]);
    }
}

function getSelectionRanges()
{
    dump("getSelectionRanges\n");
    var selection = null;
    var rangeList = new Array();
    var currentEditor = GetCurrentEditor();
    var bodyElement = currentEditor.rootElement;

    try 
    {
        var focusedWindow = document.commandDispatcher.focusedWindow;
        selection = focusedWindow.getSelection();
        dump("selected range count: "+selection.rangeCount+"\n");

        if (GetCurrentEditorType() == "textmail")
        {
            // Expand selection to beginning and end of line
            expandSelection(selection, rangeList);
        }
        else
        {
            getHTMLSelection(selection, bodyElement, rangeList);
        }
    }
    catch (e)
    {
        dump("Caught: "+e+"\n");
    }

    return rangeList;
}

function addQuoteLevel()
{
    dump("Add quote level\n");
    var currentEditor = GetCurrentEditor();

    currentEditor.beginTransaction();

    var rangeList = getSelectionRanges();

    if (GetCurrentEditorType() == "textmail")
    {
        var quotePattern = ">";
        var container = null;
        
        for (var i=0; i < rangeList.length; i++)
        {
            container = rangeList[i].commonAncestorContainer;

            if (container.nodeType == Node.TEXT_NODE)
            {
                container = container.parentNode;
            }
            addQuoteText(currentEditor, container.childNodes, rangeList[i], quotePattern);
            updateSelection(rangeList[i]);
        }
    }
    else
    {
        try
        {
            for (var i=0; i < rangeList.length; i++)
            {
                var blockquote = currentEditor.createElementWithDefaults("blockquote");
                blockquote.setAttribute("type", "cite");

                if (rangeList[i].collapsed)
                {
                    dump("Selection is empty\n");
                    continue;
                }

                // Put the blockquote in the DOM tree
                var frag = rangeList[i].cloneContents();

                blockquote.appendChild(frag);

                // Remove everything in the selected range in a way that undo recognizes
                while (!rangeList[i].collapsed)
                {
                    currentEditor.deleteNode(rangeList[i].startContainer.childNodes[rangeList[i].startOffset]);
                }
                currentEditor.insertNode(blockquote, rangeList[i].startContainer, rangeList[i].startOffset);

            }
        }
        catch(e)
        {
            dump(e);
        }
    }

    currentEditor.endTransaction();
}


function removeQuoteLevel()
{
    dump("Remove quote level\n");
    var currentEditor = GetCurrentEditor();
    var bodyElement = currentEditor.rootElement;
    var quotePattern = ">";

    currentEditor.beginTransaction();

    var rangeList = getSelectionRanges();

    if (GetCurrentEditorType() == "textmail")
    {
        var container = null;
        
        dump("Ranges: "+rangeList.length+"\n");
        for (var i=0; i < rangeList.length; i++)
        {
            container = rangeList[i].commonAncestorContainer;
            if (container.nodeType == Node.TEXT_NODE)
            {
                container = container.parentNode;
            }
            dump("Container is: "+container.nodeName+"\n");
            removeQuoteText(currentEditor, container.childNodes, rangeList[i], quotePattern);
            updateSelection(rangeList[i]);
        }
    }
    else
    {
        for (var i=0; i < rangeList.length; i++)
        {
            removeQuoteHTML(rangeList[i]);
        }
    }

    currentEditor.endTransaction();
}

function getMyOffset(node)
{
    var parentNode = node.parentNode;

    for (var i=0; i < parentNode.childNodes.length; i++)
    {
        if (parentNode.childNodes[i] == node)
        {
            return i;
        }
    }
    return 0;
}

function insertAfter(newnode, refnode)
{
    if (refnode.nextSibling)
    {
        return refnode.parentNode.insertBefore(newnode, refnode.nextSibling);
        
    }
    else
    {
        refnode.parentNode.appendChild(newnode);
        return refnode.nextSibling;
    }
}

function removeQuoteHTML(range)
{
    var fragmentArray = splitSelection(range);
    var blockquote = fragmentArray[3];
    var currentEditor = GetCurrentEditor();
    var bodyElement = currentEditor.rootElement;

    // if the parent is the body element, or there is no selection
    if (blockquote == null || blockquote == bodyElement)
    {
        dump("Selection is not in a blockquote\n");
        return;
    }

    // Set offset to the position after the blockquote
    var offset = getMyOffset(blockquote)+1;
    if (fragmentArray[2])
    {
        var bq = currentEditor.createElementWithDefaults("blockquote");
        currentEditor.cloneAttributes(bq, blockquote);
        bq.appendChild(fragmentArray[2]);
        currentEditor.insertNode(bq, blockquote.parentNode, offset);

        dump("Added fragmentArray[2]\n");
    }

    // Copy the selection into the blockquote parent
    while(fragmentArray[1].childNodes.length)
    {
        dump("inserting "+fragmentArray[1].lastChild.nodeName+"\n");
        currentEditor.insertNode(fragmentArray[1].lastChild, blockquote.parentNode, offset);
    }

    if (fragmentArray[0])
    {
        var bq = currentEditor.createElementWithDefaults("blockquote");
        currentEditor.cloneAttributes(bq, blockquote);
        bq.appendChild(fragmentArray[0]);
        currentEditor.insertNode(bq, blockquote.parentNode, offset);

        dump("Added fragmentArray[0]\n");
    }
    currentEditor.deleteNode(blockquote);
}

function removeQuoteText(editor, nodes, range, pattern)
{
    dump("remove quote text\n");
    for (var i = (nodes.length - 1); i >= 0; i--)
    {
        if (nodes[i].hasChildNodes())
        {
            removeQuoteText(editor, nodes[i].childNodes, range, pattern);
        }
        else if (range.isPointInRange(nodes[i], 0))
        {
            try
            {
                if (nodes[i].nodeType == Node.TEXT_NODE && nodes[i].nodeValue.substr(0, 1) == pattern)
                {
                    nodes[i].nodeValue = nodes[i].nodeValue.substr(1, nodes[i].nodeValue.length-1);
                }
            }
            catch (e) 
            {
                dump("Exception in removeQuoteText\n");
            }
        }
        else
        {
            //dump("Not in selection: " + nodes[i].nodeName+"\n");
        }
    }
}

function addQuoteText(editor, nodes, range, pattern)
{
    dump("Add quote text ("+nodes.length+")\n");

    for (var i = 0; i < nodes.length; i++)
    {
        dump("NodeName: " + nodes[i].nodeName+"\n");
        if (nodes[i].hasChildNodes())
        {
            addQuoteText(editor, nodes[i].childNodes, range, pattern);
        }
        else if (range.isPointInRange(nodes[i],0))
        {
            try
            {
                if (nodes[i].nodeType == Node.TEXT_NODE)
                {
                    nodes[i].nodeValue = pattern+nodes[i].nodeValue;
                }
            }
            catch (e) 
            {
                dump("Exception in addQuoteText: "+e+"\n");
            }
        }
        else
        {
            //dump("Not in selection: " + nodes[i].nodeName+"\n");
        }
    }
}


function splitSelection(range)
{
    var currentEditor = GetCurrentEditor();
    var bodyElement = currentEditor.rootElement;

    // find the root quoteblock
    var fragmentArray = new Array(4);
    var newrange = null;

    var node = range.startContainer;
    while (!(node instanceof HTMLQuoteElement))
    {
        if (node == bodyElement)
        {
            break;
        }
        node = node.parentNode;
    }

    // If whole document selected
    if (node == bodyElement)
    {
        dump("selection is not in a blockquote\n");

        // Selection is not in a blockquote
        newrange = document.createRange();
        fragmentArray[0] = null;
        fragmentArray[1] = range.cloneContents(true);
        fragmentArray[2] = null;
    }
    else
    {
        fragmentArray[1] = range.cloneContents();
        try
        {
            newrange = document.createRange();
            newrange.setStart(node, 0);

            // The next line fails when the range is empty
            if (range.startContainer.nodeType == Node.TEXT_NODE)
            {
                newrange.setEndBefore(range.startContainer);
            }
            else
            {
                newrange.setEndBefore(range.startContainer.childNodes[range.startOffset]);
            }
            fragmentArray[0] = newrange.cloneContents();

        }
        catch(e)
        {
            fragmentArray[0] = null;
        }

        try
        {
            newrange = document.createRange();
            if (range.endContainer.nodeType == Node.TEXT_NODE)
            {
                newrange.setStartAfter(range.endContainer);
            }
            else
            {
                newrange.setStartAfter(range.endContainer.childNodes[range.endOffset]);
            }

            // The next line fails when the range is empty
            newrange.setEnd(node, node.childNodes.length);
            fragmentArray[2] = newrange.cloneContents();
        }
        catch(e)
        {
            fragmentArray[2] = null;
        }
    }

    walkFragment(fragmentArray[0], "Before Selection");
    walkFragment(fragmentArray[1], "Selection");
    walkFragment(fragmentArray[2], "After Selection");

    // Save the parent as the 4th entry
    fragmentArray[3]=node;

    return fragmentArray;
}

function walkFragment(fragment, text)
{
    dump("--------- BEGIN "+text+" ----------\n");
    var node = fragment;

    while (node)
    {
        dumpNode(node);
        if (node.nextSibling)
        {
            node = node.nextSibling;
        }
        else
        {
            node = node.parentNode;
        }
    }
    dump("--------- END "+text+" ------------\n");
}

function dumpNode(node)
{
    if (!node)
    {
        return;
    }
    var attrs = node.attributes;
    var text = ""; 
    if (attrs)
    {
        for(var j=attrs.length-1; j>=0; j--) 
        {
            text += attrs[j].name + "=" + "'"+attrs[j].value+"' ";
        }
    }
    dump("<"+node.nodeName+" "+text+">");

    if (node.nodeValue)
    {
        dump(node.nodeValue);

    }
    if (node.hasChildNodes())
    {
        for (var child=node.firstChild; child != null; child = child.nextSibling)
        {
            dumpNode(child);
        }
    }
    dump("</"+node.nodeName+">\n");
}

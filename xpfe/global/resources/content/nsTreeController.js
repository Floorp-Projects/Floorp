
// helper routines, for doing rdf-based cut/copy/paste/etc

const NC_NS  = "http://home.netscape.com/NC-rdf#";
const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const nsTransferable_contractid = "@mozilla.org/widget/transferable;1";
const clipboard_contractid = "@mozilla.org/widget/clipboard;1";
const rdf_contractid = "@mozilla.org/rdf/rdf-service;1";
const separatorUri = NC_NS + "BookmarkSeparator";
const supportswstring_contractid = "@mozilla.org/supports-wstring;1";

// some oft-used interfaces
const nsISupportsWString = Components.interfaces.nsISupportsWString;
const nsIClipboard = Components.interfaces.nsIClipboard;
const nsITransferable = Components.interfaces.nsITransferable;
const nsIRDFLiteral = Components.interfaces.nsIRDFLiteral;
const nsIRDFContainer = Components.interfaces.nsIRDFContainer;

var Clipboard = Components.classes[clipboard_contractid].getService(Components.interfaces.nsIClipboard);
var RDF = Components.classes[rdf_contractid].getService(Components.interfaces.nsIRDFService);

var nameResource = RDF.GetResource(NC_NS + "Name");
var typeRes = RDF.GetResource(RDF_NS + "type");
var bmTypeRes = RDF.GetResource(NC_NS + "Bookmark");

function debug(foo)
{
    dump(foo);
}

function isContainer(node)
{
    return node.getAttribute("container") == "true";
}

function getWStringData(wstring, len)
{
    wstring = wstring.QueryInterface(nsISupportsWString);
    var result = wstring.data.substring(0, len/2);
    return result;
}

function nsTreeController_SetTransferData(transferable, flavor, text)
{
    if (!text) return;
    var textData = Components.classes[supportswstring_contractid].createInstance(nsISupportsWString);
    textData.data = text;

    transferable.addDataFlavor(flavor);
    transferable.setTransferData(flavor, textData, text.length*2);
}

function nsTreeController_copy(tree)
{
    var tree = this.getTree();
    var select_list = tree.selectedItems;
    if (!select_list) return false;
    if (select_list.length < 1) return false;
    
    var datasource = tree.database;

    // Build a url that encodes all the select nodes 
    // as well as their parent nodes
    var url = "";
    var text = "";
    var html = "";

    for (var nodeIndex = 0; nodeIndex < select_list.length; nodeIndex++)
    {
        var node = select_list[nodeIndex];
        if (!node) continue;

        // for now just use the .id attribute.. at some point we should support
        // the #URL property for anonymous resources (see bookmarks.js)
        //var ID = getAbsoluteID("bookmarksTree", node);
        var ID = node.id;
        
        var IDRes = RDF.GetResource(ID);

        var nameNode = datasource.GetTarget(IDRes, nameResource, true);
        nameNode =
            nameNode.QueryInterface(nsIRDFLiteral);
        
        var theName = nameNode.Value;

        debug("Node " + nodeIndex + ": " + ID + "    name: " + theName + "\n");
        url += "ID:{" + ID + "};";
        url += "NAME:{" + theName + "};";

        if (isContainer(node))
        {
            // gack, need to generalize this
            var type = node.getAttribute("type");
            if (type == seperatorUri)
            {
                // Note: can't encode separators in text, just html
                html += "<hr><p>";
            }
            else
            {
                text += ID + "\r";
            
                html += "<a href='" + ID + "'>";
                if (theName) html += theName;
                html += "</a><p>";
            }
        }
    }

    if (url == "") return false;
    debug("Copy URL: " + url + "\n");

    // get some useful components
    var trans = Components.classes[nsTransferable_contractid].createInstance(nsITransferable);

    Clipboard.emptyClipboard(nsIClipboard.kGlobalClipboard);

    this.SetTransferData(trans, "moz/bookmarkclipboarditem", url);
    this.SetTransferData(trans, "text/unicode", text);
    this.SetTransferData(trans, "text/html", html);

    Clipboard.setData(trans, null, nsIClipboard.kGlobalClipboard);
    return true;
}

function nsTreeController_cut(tree)
{
    if (this.copy()) {
        this.doDelete();
        return true;            // copy succeeded, don't care if delete failed
    } else
        return false;           // copy failed, so did cut
}

function nsTreeController_paste(tree)
{
    var select_list = tree.selectedItems;
    if (!select_list) return false;
    if (select_list.length != 1) return false;
    
    var datasource = tree.database;
    
    var pasteNodeID = select_list[0].id;
    debug("Paste onto " + pasteNodeID + "\n");
    var isContainerFlag = isContainer(select_list[0]);
    debug("Container status: " + isContainerFlag + "\n");


    var trans_uri = "@mozilla.org/widget/transferable;1";
    var trans = Components.classes[nsTransferable_contractid].createInstance(nsITransferable);
    trans.addDataFlavor("moz/bookmarkclipboarditem");

    Clipboard.getData(trans, nsIClipboard.kGlobalClipboard);
    var data = new Object();
    var dataLen = new Object();
    trans.getTransferData("moz/bookmarkclipboarditem", data, dataLen);
    
    var url = getWStringData(data.value, dataLen.value);
    if (!url) return false;

    var strings = url.split(";");
    if (!strings) return false;

    var rdfc_uri = "@mozilla.org/rdf/container;1";
    var RDFC = Components.classes[rdfc_uri].getService(nsIRDFContainer);
    
    var nameRes = RDF.GetResource(NC_NS + "Name");
    if (!nameRes) return false;

    pasteNodeRes = RDF.GetResource(pasteNodeID);
    if (!pasteNodeRes) return false;
    
    var pasteContainerRes = null;
    var pasteNodeIndex = -1;
    if (isContainerFlag == true)
    {
        pasteContainerRes = pasteNodeRes;
    }
    else
    {
        var parID = select_list[0].parentNode.parentNode.getAttribute("ref");
        if (!parID) {
            parID = select_list[0].parentNode.parentNode.getAttribute("id");
        }
        if (!parID) return false;
        pasteContainerRes = RDF.GetResource(parID);
        if (!pasteContainerRes) return false;
    }
    RDFC.Init(datasource, pasteContainerRes);

    debug("Inited RDFC\n");

    if (isContainerFlag == false)
    {
        pasteNodeIndex = RDFC.IndexOf(pasteNodeRes);
        if (pasteNodeIndex < 0) return false; // how did that happen?
    }

    debug("Loop over strings\n");

    var dirty = false;
    for (var x=0; x<strings.length; x=x+2)
    {
        var theID = strings[x];
        var theName = strings[x+1];

        // make sure we have a real name/id combo
        if ((theID.indexOf("ID:{") != 0) ||
            (theName.indexOf("NAME:{") != 0))
            continue;
        
        theID = theID.substr(4, theID.length-5);
        theName = theName.substr(6, theName.length-7);
        debug("Paste  ID: " + theID + "    NAME: " + theName + "\n");
        
        var IDRes = RDF.GetResource(theID);
        if (!IDRes) continue;
        
        if (RDFC.IndexOf(IDRes) > 0)
        {
            debug("Unable to add ID:'" + theID +
                  "' as its already in this folder.\n");
            continue;
        }
        
        // assert the name into the DS
        if (theName)
        {
            var NameLiteral = RDF.GetLiteral(theName);
            datasource.Assert(IDRes, nameRes, NameLiteral, true);
            dirty = true;
        }

        // add the element at the appropriate location
        if (isContainerFlag == true)
        {
            RDFC.AppendElement(IDRes);
            debug("Appended node onto end of container\n");
        }
        else
        {
            RDFC.InsertElementAt(IDRes, pasteNodeIndex++, true);
            debug("Pasted at index # " + pasteNodeIndex + "\n");
        }
        dirty = true;

            // make sure appropriate bookmark type is set
        var bmTypeNode = datasource.GetTarget( IDRes, typeRes, true );
        if (!bmTypeNode)
        {
            // set default bookmark type
            datasource.Assert(IDRes, typeRes, bmTypeRes, true);
            debug("Setting default bookmark type\n");
        }
    }
    
    if (dirty)
    {
        var remote = datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
        if (remote)
        {
            remote.Flush();
            debug("Wrote out bookmark changes.\n");
        }
    }
    return true;
}

function nsTreeController_delete(tree)
{

}

function nsTreeController(tree)
{
    this.treeId = tree.id;
    tree.controllers.appendController(this);
}

nsTreeController.prototype =
{
    // store the tree's Id, rather than the tree,
    // to avoid holding a strong ref
    treeId: null,
    getTree : function()
    {
        return document.getElementById(this.treeId);
    },
    SetTransferData : nsTreeController_SetTransferData,

    supportsCommand: function(command)
    {
        switch(command)
        {
            case "cmd_cut":
            case "cmd_copy":
            case "cmd_paste":
            case "cmd_delete":
            case "cmd_selectAll":
                return true;
            default:
                return false;
        }
    },

    isCommandEnabled: function(command)
    {
        var haveCommand;
        switch (command)
        {
            // commands which do not require selection
            case "cmd_paste":
                return (this.doPaste != undefined);
                
            case "cmd_selectAll":
                // we can always select all
                return true;
                
            // these commands require selection
            case "cmd_cut":
                haveCommand = (this.cut != undefined);
                break;
            case "cmd_copy":
                haveCommand = (this.copy != undefined);
                break;
            case "cmd_delete":
                haveCommand = (this.doDelete != undefined);
                break;
        }
        
        // if we get here, then we have a command that requires selection
        var tree = this.getTree();
        var haveSelection = (tree.selectedItems.length > 0)
        return haveCommand && haveSelection;
    },
    doCommand: function(command)
    {
        switch(command)
        {
            case "cmd_cut":
                return this.cut(this.getTree());
            
            case "cmd_copy":
                return this.copy(this.getTree());
            
            case "cmd_paste":
                return this.paste(this.getTree());

            case "cmd_delete":
                return this.doDelete(this.getTree());
            
            case "cmd_selectAll":
                return this.getTree().selectAll();
        }
        dump("How did I get here?\n");
        return false;
    },
    copy: nsTreeController_copy,
    cut: nsTreeController_cut,
    paste: nsTreeController_paste,
    doDelete: nsTreeController_delete
}


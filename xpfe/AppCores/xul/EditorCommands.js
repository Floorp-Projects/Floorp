  
  /* the following variables are expected to be defined
  	in the embedding file:

  	var editorType  -- the editor type, i.e. "text" or "html"
    var editorName	-- the name of the editor. Must be unique to this window clique
  */
  
  function Startup()
  {
    dump("Doing Startup...\n");
    appCore = XPAppCoresManager.Find("EditorAppCore");  
    dump("Looking up EditorAppCore...\n");
    if (appCore == null) {
      dump("Creating EditorAppCore...\n");
      appCore = new EditorAppCore();
      if (appCore != null) {
        dump("EditorAppCore has been created.\n");
				appCore.Init(editorName);
				appCore.setEditorType(editorType);
				appCore.setContentWindow(window.frames[0]);
				appCore.setWebShellWindow(window);
				appCore.setToolbarWindow(window);
      }
    } else {
      dump("EditorAppCore has already been created! Why?\n");
    }
  }

  function EditorApplyStyle(styleName)
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	  	dump("Applying Style\n");
      appCore.setTextProperty(styleName, null, null);
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorRemoveStyle(styleName)
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	  	dump("Removing Style\n");
      appCore.removeTextProperty(styleName, null);
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorGetText()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	  	dump("Getting text\n");
			var	outputText = appCore.contentsAsText;
			dump(outputText + "\n");
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorGetHTML()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	  	dump("Getting HTML\n");
			var	outputText = appCore.contentsAsHTML;
			dump(outputText + "\n");
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorUndo()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	    dump("Undoing\n");
      appCore.undo();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorRedo()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	    dump("Redoing\n");
      appCore.redo();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

	function EditorCut()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	    dump("Cutting\n");
      appCore.cut();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

	function EditorCopy()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	    dump("Copying\n");
      appCore.copy();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

	function EditorPaste()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	    dump("Pasting\n");
      appCore.paste();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorSelectAll()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	    dump("Selecting all\n");
      appCore.selectAll();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

	function EditorInsertText()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	    dump("Inserting text\n");
      appCore.insertText("Once more into the breach, dear friends.\n");
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorInsertLink()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
      dump("Inserting link\n");
      appCore.insertLink();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorInsertList(listType)
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
      dump("Inserting link\n");
      appCore.insertList(listType);
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorInsertImage()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
      dump("Inserting image\n");
      appCore.insertImage();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

  function EditorExit()
  {
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null) {
	    dump("Exiting\n");
      appCore.exit();
    } else {
      dump("EditorAppCore has not been created!\n");
    }
  }

 function EditorPrintPreview() {
    var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore)
        toolkitCore.Init("ToolkitCore");
    }
    if (toolkitCore) {
      toolkitCore.ShowWindow("resource:/res/samples/printsetup.html", window);
    }
  }


	function EditorTestSelection()
	{
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null)
    {
 	    dump("Testing selection\n");
	    var selection = appCore.editorSelection;
	    if (selection)
	    {
		    dump("Got selection\n");
	    	var	firstRange = selection.getRangeAt(0);
	    	if (firstRange)
	    	{
	    		dump("Range contains \"");
	    		dump(firstRange.toString() + "\"\n");
	    	}
	    }
	    
    }
    else
    {
      dump("EditorAppCore has not been created!\n");
    }
	}

	function EditorTestDocument()
	{
    appCore = XPAppCoresManager.Find(editorName);  
    if (appCore != null)
    {
	    dump("Getting document\n");
	    var theDoc = appCore.editorDocument;
    	if (theDoc)
    	{
    		dump("Got the doc\n");
    	  dump("Document name:" + theDoc.nodeName + "\n");
    	  dump("Document type:" + theDoc.doctype + "\n");
	    }
	    else
	    {
	    		dump("Failed to get the doc\n");
	    }
    }
    else
    {
      dump("EditorAppCore has not been created!\n");
    }
	}

	/* Status calls */
	function onBoldChange()
	{
		var button = document.getElementById("Editor:Style:IsBold");
		if (button)
		{
			var bold = button.getAttribute("bold");

			if ( bold == "true" ) {
				button.setAttribute( "disabled", false );
			}
			else {
				button.setAttribute( "disabled", true );
			}

		}
		else
		{
			dump("Can't find bold broadcaster!\n");
		}
	}

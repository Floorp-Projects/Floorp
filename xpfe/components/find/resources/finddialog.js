        var finder; // Find component.
        var data;   // Search context (passed as argument).
        var dialog; // Quick access to document/form elements.

        function string2Bool( value )
        {
            return value != "false";
        }

        function bool2String( value )
        {
            if ( value ) {
                return "true";
            } else {
                return "false";
            }
        }

        function initDialog()
        {
            // Create dialog object and initialize.
            dialog = new Object;
            dialog.findKey         = document.getElementById("dialog.findKey");
            dialog.caseSensitive   = document.getElementById("dialog.caseSensitive");
            dialog.wrap            = document.getElementById("dialog.wrap");
            dialog.searchBackwards = document.getElementById("dialog.searchBackwards");
            dialog.find            = document.getElementById("ok");
            dialog.cancel          = document.getElementById("cancel");
            dialog.enabled         = false;
        }

        function loadDialog()
        {
            // Set initial dialog field contents.
            dialog.findKey.setAttribute( "value", data.searchString );

            if ( data.caseSensitive ) {
                dialog.caseSensitive.setAttribute( "checked", "" );
            } else {
                dialog.caseSensitive.removeAttribute( "checked" );
            }

            if ( data.wrapSearch ) {
                dialog.wrap.setAttribute( "checked", "" );
            } else {
                dialog.wrap.removeAttribute( "checked" );
            }

            if ( data.searchBackwards ) {
                dialog.searchBackwards.setAttribute( "checked", "" );
            } else {
                dialog.searchBackwards.removeAttribute( "checked" );
            }
            
            // disable the OK button if no text
            if (dialog.findKey.getAttribute("value") == "") {
	            dialog.find.setAttribute("disabled", "true");
	       	}
	    dialog.findKey.focus();
        }

        function loadData()
        {
            // Set data attributes per user input.
            data.searchString = dialog.findKey.value;
            data.caseSensitive = dialog.caseSensitive.checked;
            data.wrapSearch = dialog.wrap.checked;
            data.searchBackwards = dialog.searchBackwards.checked;
        }

        function onLoad()
        {
            // Init dialog.
            initDialog();

            // Get find component.
            finder = Components.classes[ "component://netscape/appshell/component/find" ].getService();
            finder = finder.QueryInterface( Components.interfaces.nsIFindComponent );
            if ( !finder ) {
                alert( "Error accessing find component\n" );
                window.close();
                return;
            }

            // change OK to find
            dialog.find.setAttribute("value", document.getElementById("fBLT").getAttribute("value"));

            // setup the dialogOverlay.xul button handlers
            doSetOKCancel(onOK, onCancel);

            // Save search context.
            data = window.arguments[0];

            // Tell search context about this dialog.
            data.findDialog = window;

            // Fill dialog.
            loadDialog();

            // Give focus to search text field.
            dialog.findKey.focus();
        }

        function onUnload() {
            // Disconnect context from this dialog.
            data.findDialog = null;
        }

        function onOK()
        {
            // Transfer dialog contents to data elements.
            loadData();

            // Search.
            finder.FindNext( data );

            // don't close the window
            return false;
        }

        function onCancel()
        {
            // Close the window.
            return true;
        }

        function onTyping()
        {
                if ( dialog.enabled ) {
                    // Disable OK if they delete all the text.
                    if ( dialog.findKey.value == "" ) {
                        dialog.enabled = false;
                        dialog.find.setAttribute("disabled", "true");
                    }
                } else {
                    // Enable OK once the user types something.
                    if ( dialog.findKey.value != "" ) {
                        dialog.enabled = true;
                        dialog.find.removeAttribute("disabled");
                    }
                }
        }

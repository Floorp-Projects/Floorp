        var data;
        var dialog;

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

        function initData()
        {
            // Create data object and initialize.
            data = new Object;
            data.findKey         = document.getElementById("data.findKey");
            //data.replaceKey      = document.getElementById("data.replaceKey");
            data.caseSensitive   = document.getElementById("data.caseSensitive");
            data.wrap            = document.getElementById("data.wrap");
            data.searchBackwards = document.getElementById("data.searchBackwards");
            data.execute         = document.getElementById("data.execute");
        }

        function initDialog()
        {
            // Create dialog object and initialize.
            dialog = new Object;
            dialog.findKey         = document.getElementById("dialog.findKey");
            dialog.caseSensitive   = document.getElementById("dialog.caseSensitive");
            dialog.wrap            = document.getElementById("dialog.wrap");
            dialog.searchBackwards = document.getElementById("dialog.searchBackwards");
            dialog.find            = document.getElementById("dialog.find");
            dialog.cancel          = document.getElementById("dialog.cancel");
            dialog.enabled         = false;
        }

        function loadDialog()
        {
            // Set initial dialog field contents.
            dialog.findKey.setAttribute( "value", data.findKey.getAttribute("value") );

            if ( data.caseSensitive.getAttribute("value") == "true" ) {
                dialog.caseSensitive.setAttribute( "checked", "" );
            } else {
                dialog.caseSensitive.removeAttribute( "checked" );
            }

            if ( data.wrap.getAttribute("value") == "true" ) {
                dialog.wrap.setAttribute( "checked", "" );
            } else {
                dialog.wrap.removeAttribute( "checked" );
            }

            if ( data.searchBackwards.getAttribute("value") == "true" ) {
                dialog.searchBackwards.setAttribute( "checked", "" );
            } else {
                dialog.searchBackwards.removeAttribute( "checked" );
            }
            
            // disable the OK button if no text
            if (dialog.findKey.getAttribute("value") == "") {
	            dialog.find.setAttribute( "disabled", "" );
	       	}
	       	
	       	dialog.findKey.focus();
        }

        function loadData()
        {
            // Set data attributes per user input.
            data.findKey.setAttribute( "value", dialog.findKey.value );

            if ( dialog.caseSensitive.checked ) {
                data.caseSensitive.setAttribute( "value", "true" );
            } else {
                data.caseSensitive.setAttribute( "value", "false" );
            }

            if ( dialog.wrap.checked ) {
                data.wrap.setAttribute( "value", "true" );
            } else {
                data.wrap.setAttribute( "value", "false" );
            }

            if ( dialog.searchBackwards.checked ) {
                data.searchBackwards.setAttribute( "value", "true" );
            } else {
                data.searchBackwards.setAttribute( "value", "false" );
            }
        }

        function dumpData()
        {
            dump( "data.key = " + data.key + "\n" );
                dump( "\tvalue=" + data.key.getAttribute("value") + "\n" );
            dump( "data.caseSensitive = " + data.caseSensitive + "\n" );
                dump( "\tvalue=" + data.caseSensitive.getAttribute("value") + "\n" );
            dump( "data.searchBackwards = " + data.searchBackwards + "\n" );
                dump( "\tvalue=" + data.searchBackwards.getAttribute("value") + "\n" );
            dump( "data.wrap = " + data.wrap + "\n" );
                dump( "\tvalue=" + data.wrap.getAttribute("value") + "\n" );
            dump( "data.execute = " + data.execute + "\n" );
                dump( "\tkey=" + data.execute.getAttribute("key") + "\n" );
                dump( "\tcaseSensitive=" + data.execute.getAttribute("caseSensitive") + "\n" );
                dump( "\tsearchBackwards=" + data.execute.getAttribute("searchBackwards") + "\n" );
                dump( "\twrap=" + data.execute.getAttribute("wrap") + "\n" );
        }

        function onLoad()
        {
            // Init data.
            initData();

            // Init dialog.
            initDialog();

            // Fill dialog.
            loadDialog();
        }

        function find()
        {
            // Note: This is broken!  We must detect whether the user has changed
            //       the dialog contents and only trigger "find" if they have.
            //       Otherwise, trigger "next."

            // Transfer dialog contents to data elements.
            loadData();

            // Set data.execute argument attributes from data.
            data.execute.setAttribute( "findKey", data.findKey.getAttribute("value") );
            data.execute.setAttribute( "caseSensitive", data.caseSensitive.getAttribute("value") );
            data.execute.setAttribute( "searchBackwards", data.searchBackwards.getAttribute("value") );
            data.execute.setAttribute( "wrap", data.wrap.getAttribute("value") );

            // Fire command.
            data.execute.setAttribute( "command", "find" );
        }

        function cancel()
        {
            // Close the window.
            data.execute.setAttribute( "command", "cancel" );
        }

        function onTyping( key )
        {
            if ( key == 13 && dialog.enabled) {
                find();
            } else
            {
                if ( dialog.enabled ) {
                    // Disable OK if they delete all the text.
                    if ( dialog.findKey.value == "" ) {
                        dialog.enabled = false;
                        dialog.find.setAttribute( "disabled", "" );
                    }
                } else {
                    // Enable OK once the user types something.
                    if ( dialog.findKey.value != "" ) {
                        dialog.enabled = true;
                        dialog.find.removeAttribute( "disabled" );
                    }
                }
            }
        }

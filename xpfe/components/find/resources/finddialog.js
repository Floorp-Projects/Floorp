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
            data.key             = document.getElementById("data.key");
            data.ignoreCase      = document.getElementById("data.ignoreCase");
            data.wrap            = document.getElementById("data.wrap");
            data.searchBackwards = document.getElementById("data.searchBackwards");
            data.execute         = document.getElementById("data.execute");
        }

        function initDialog()
        {
            // Create dialog object and initialize.
            dialog = new Object;
            dialog.key             = document.getElementById("dialog.key");
            dialog.ignoreCase      = document.getElementById("dialog.ignoreCase");
            dialog.wrap            = document.getElementById("dialog.wrap");
            dialog.searchBackwards = document.getElementById("dialog.searchBackwards");
            dialog.ok              = document.getElementById("dialog.ok");
            dialog.cancel          = document.getElementById("dialog.cancel");
            dialog.enabled         = false;
        }

        function loadDialog()
        {
            // Set initial dialog field contents.
            dialog.key.setAttribute( "value", data.key.getAttribute("value") );

            // Note: dialog.ignoreCase is actually "Case Sensitive" so
            //       we must reverse things when getting the data.
            if ( data.ignoreCase.getAttribute("value") == "true" ) {
                dialog.ignoreCase.removeAttribute( "checked" );
            } else {
                dialog.ignoreCase.setAttribute( "checked", "" );
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
            if (dialog.key.getAttribute("value") == "") {
	            dialog.ok.setAttribute( "disabled", "" );
	       	}
        }

        function loadData()
        {
            // Set data attributes per user input.
            data.key.setAttribute( "value", dialog.key.value );

            // Note: dialog.ignoreCase is actually "Case Sensitive" so
            //       we must reverse things when storing the data.
            if ( dialog.ignoreCase.checked ) {
                data.ignoreCase.setAttribute( "value", "false" );
            } else {
                data.ignoreCase.setAttribute( "value", "true" );
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
            dump( "data.ignoreCase = " + data.ignoreCase + "\n" );
                dump( "\tvalue=" + data.ignoreCase.getAttribute("value") + "\n" );
            dump( "data.searchBackwards = " + data.searchBackwards + "\n" );
                dump( "\tvalue=" + data.searchBackwards.getAttribute("value") + "\n" );
            dump( "data.wrap = " + data.wrap + "\n" );
                dump( "\tvalue=" + data.wrap.getAttribute("value") + "\n" );
            dump( "data.execute = " + data.execute + "\n" );
                dump( "\tkey=" + data.execute.getAttribute("key") + "\n" );
                dump( "\tignoreCase=" + data.execute.getAttribute("ignorecase") + "\n" );
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

        function ok()
        {
            // Note: This is broken!  We must detect whether the user has changed
            //       the dialog contents and only trigger "find" if they have.
            //       Otherwise, trigger "next."

            // Transfer dialog contents to data elements.
            loadData();

            // Set data.execute argument attributes from data.
            data.execute.setAttribute( "key", data.key.getAttribute("value") );
            data.execute.setAttribute( "ignoreCase", data.ignoreCase.getAttribute("value") );
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
		//if ( key == 13 &amp;&amp; dialog.enabled ) {
            if ( key == 13 && dialog.enabled) {
                ok();
            } else
            {
                if ( dialog.enabled ) {
                    // Disable OK if they delete all the text.
                    if ( dialog.key.value == "" ) {
                        dialog.enabled = false;
                        dialog.ok.setAttribute( "disabled", "" );
                    }
                } else {
                    // Enable OK once the user types something.
                    if ( dialog.key.value != "" ) {
                        dialog.enabled = true;
                        dialog.ok.removeAttribute( "disabled" );
                    }
                }
            }
        }
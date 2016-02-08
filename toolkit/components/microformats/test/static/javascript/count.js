/*!
    parse
    Used by http://localhost:3000/
    Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
    MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
*/

window.onload = function() {

    var form;
    form= document.getElementById('mf-form');

    form.onsubmit = function(e){
        e.preventDefault();

        var html,
            doc,
            node,
            options,
            mfJSON,
            parserJSONElt;

        // get data from html
        html = document.getElementById('html').value;
        parserJSONElt = document.querySelector('#parser-json pre code')

        // createHTMLDocument is not well support below ie9
        doc = document.implementation.createHTMLDocument("New Document");
        node =  document.createElement('div');
        node.innerHTML = html;
        doc.body.appendChild(node);

        options ={
            'node': node
        };

        // parse direct into Modules to help debugging
        if(window.Modules){
            var parser = new Modules.Parser();
            mfJSON = parser.count(options);
        }else if(window.Microformats){
            mfJSON = Microformats.count(options);
        }


        // format output
        parserJSONElt.innerHTML = htmlEscape( js_beautify( JSON.stringify(mfJSON) ) );
        //prettyPrint();

    }

    function htmlEscape(str) {
        return String(str)
                .replace(/&/g, '&amp;')
                .replace(/"/g, '&quot;')
                .replace(/'/g, '&#39;')
                .replace(/</g, '&lt;')
                .replace(/>/g, '&gt;');
    }


};

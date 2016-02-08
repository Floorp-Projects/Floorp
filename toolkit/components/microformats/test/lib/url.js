/*
   url
   Where possible use the modern window.URL API if its not available use the DOMParser method.

   Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
   MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
*/

var Modules = (function (modules) {


	modules.url = {


		/**
		 * creates DOM objects needed to resolve URLs
		 */
        init: function(){
            //this._domParser = new DOMParser();
            this._domParser = modules.domUtils.getDOMParser();
            // do not use a head tag it does not work with IE9
            this._html = '<base id="base" href=""></base><a id="link" href=""></a>';
            this._nodes = this._domParser.parseFromString( this._html, 'text/html' );
            this._baseNode =  modules.domUtils.getElementById(this._nodes,'base');
            this._linkNode =  modules.domUtils.getElementById(this._nodes,'link');
        },


		/**
		 * resolves url to absolute version using baseUrl
		 *
		 * @param  {String} url
		 * @param  {String} baseUrl
		 * @return {String}
		 */
		resolve: function(url, baseUrl) {
			// use modern URL web API where we can
			if(modules.utils.isString(url) && modules.utils.isString(baseUrl) && url.indexOf('://') === -1){
				// this try catch is required as IE has an URL object but no constuctor support
				// http://glennjones.net/articles/the-problem-with-window-url
				try {
					var resolved = new URL(url, baseUrl).toString();
					// deal with early Webkit not throwing an error - for Safari
					if(resolved === '[object URL]'){
						resolved = URI.resolve(baseUrl, url);
					}
					return resolved;
				}catch(e){
                    // otherwise fallback to DOM
                    if(this._domParser === undefined){
                        this.init();
                    }

                    // do not use setAttribute it does not work with IE9
                    this._baseNode.href = baseUrl;
                    this._linkNode.href = url;

                    // dont use getAttribute as it returns orginal value not resolved
                    return this._linkNode.href;
				}
			}else{
				if(modules.utils.isString(url)){
					return url;
				}
				return '';
			}
		},

	};

	return modules;

} (Modules || {}));

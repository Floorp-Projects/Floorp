msa = {};

msa.Vowels = function($, ttl, cdns, numImages, RID) {
	this.magic = "5SnM";
	this.$ = $;
	this.ttl = ttl;
    this.RID = RID;
	this.cdnDomains = cdns;
	// must start with / and end with /	
	this.imagePrefix = "/images/C/";
    this.numImages = numImages;
	this.protocol = 'http';
	this.cookieName = this.magic + "amzvowels";
	this.divName = this.magic + "vowelsdiv";
	this.imageCounter = 0;
	this.pool = null;
	this.expectLoadSuccess = null;
        this.browserAgent = $.browser + "[{"+$.browser.version+"}]";
};

msa.Vowels.prototype.setDivName = function(divName) {
	this.divName = divName;
}

msa.Vowels.prototype.setExpectLoadSucces = function (v) {
	this.expectLoadSuccess = v;
}

msa.Vowels.prototype.attachLoadEvent = function(elem, func) {
	if (elem.load) {
		if (this.pool != null) {
			elem.ready(function() {
				elem.load(this.pool.add(func));
			});

		} else {
			elem.ready(function() {
				elem.load(func);
			});
		}
	}
};

msa.Vowels.prototype.attachErrorEvent = function(elem, func) {
	if (elem.error) {
		if (this.pool != null) {
			elem.ready(function() {
				elem.error(this.pool.add(func));
			});

		} else {
			elem.ready(function() {
				elem.error(func);
			});
		}
	}
};

msa.Vowels.prototype.setCallbackPool = function(pool) {
	if (typeof pool == 'function') {
		this.pool = pool;
	}
}

msa.Vowels.prototype.getCookie = function(name) {
	var returnValue = {};
	var found = false;
	
	if (document.cookie) {
		var cookies = document.cookie.split(';');
		for ( var i = 0; i < cookies.length; i++) {
			var cookie = this.$.trim(cookies[i]);
			// Does this cookie string begin with the name we want?
			if (!name) {
				var nameLength = cookie.indexOf('=');
				returnValue[cookie.substr(0, nameLength)] = decodeURIComponent(cookie
						.substr(nameLength + 1));
				found = true;
			} else if (cookie.substr(0, name.length + 1) == (name + '=')) {
				returnValue = decodeURIComponent(cookie.substr(name.length + 1));
				found = true;
				break;
			}
		}
	}
	
	if(found){
		return returnValue;		
	}
	else {
		return null;
	}
};

msa.Vowels.prototype.setCookie = function(name, value, options) {
	if (typeof name == 'string') {
		options = options || {};
		if (value === null) {
			value = '';
			options.expires = -1;
		}
		var expires = '';
		if (options.expires
				&& (typeof options.expires == 'number' || options.expires.toUTCString)) {
			var date;
			if (typeof options.expires == 'number') {
				date = new Date();
				date.setTime(date.getTime()
						+ (options.expires * 60 * 60 * 1000));
			} else {
				date = options.expires;
			}
			expires = '; expires=' + date.toUTCString(); // use expires attribute, max-age is not supported by IE
		}
		// CAUTION: Needed to parenthesize options.path and options.domain
		// in the following expressions, otherwise they evaluate to undefined
		// in the packed version for some reason...
		var path = options.path ? '; path=' + (options.path) : '';
		var domain = options.domain ? '; domain=' + (options.domain) : '';
		var secure = options.secure ? '; secure' : '';
		document.cookie = name + '=' + encodeURIComponent(value) + expires
				+ path + domain + secure;
	} else { // `name` is really an object of multiple cookies to be set.
		for ( var n in name) {
			jQuery.cookie(n, name[n], value || options);
		}
	}
};

msa.Vowels.prototype.getNextId = function () {
	var imageId = this.magic + this.imageCounter;
	this.imageCounter += 1;
	return imageId;
};

msa.Vowels.prototype.sendToCLOG = function(cdn, i, eventType, t,pos) {
    var l = (new Date).getTime() - t;
    var params = {};
    params.src = i.attr('src');
    params.l = l;
    params.s = document.domain;
    params.u = window.location.pathname;
    params.b = pos;
    params.sy = this.RID;
    params.tz = this.browserAgent;


    if (typeof(window.ue) !== 'undefined' && typeof(window.ue.furl) !== 'undefined')
    {
        new Image().src = this.generateUrl(params);
    }
};

msa.Vowels.prototype.generateUrl = function(params)
{
    var uefurl = window.ue.furl;

    if (uefurl === 'fls-pek.amazon.com') {
        uefurl = 'fls-cn.amazon.com';
    }

    var url = '//' + uefurl + '/1/msa-vowels/1/OP/?';

    var key;
    var value;
    for (key in params)
    {   
        value = params[key];

        url = url + window.escape(key);
        url = url + '=';
        url = url + window.escape(value);
        url = url + '&';
    }   

    return url;
};

msa.Vowels.prototype.addImageEventToDOM = function(cdn, url,pos) {
	var imageId = this.getNextId();
	var imageUrl = this.protocol + "://" + cdn + url;
	var vowels = this;

	var startTime = (new Date).getTime();
	this.$("#" + this.divName).append(
			"<img id='" + imageId + "' width='0' height='0' src='" + imageUrl
					+ "'></img>");

	var vowels = this;

	this.$("#" + imageId).ready(function() {

		if(this.expectLoadSuccess==null || this.expectLoadSuccess==true){
			vowels.attachLoadEvent(vowels.$("#"+imageId), function () {
				vowels.sendToCLOG(cdn, vowels.$("#"+imageId), "l", startTime,pos);
			});
		}
		
		vowels.attachErrorEvent(vowels.$("#" + imageId), function() {
			vowels.sendToCLOG(cdn, vowels.$("#" + imageId), "e", startTime,pos);
		});			

	});
};


msa.Vowels.prototype.addDummyImageToDOM = function(cdn, imagePath, pos) {
	var imageId = this.getNextId();
	var imageUrl = this.protocol + "://" + cdn + "/images/C/" + imageId;

	
	this.$("#" + this.divName).append(
			"<img id='" + imageId + "' width='0' height='0' src='" + imageUrl
					+ "'></img>");
					
    var vowels = this;

	this.$("#" + imageId).ready(function() {
		vowels.attachLoadEvent(vowels.$("#"+imageId), function () {
        	vowels.addImageEventToDOM(cdn, imagePath,pos);	
		});
	});
                
};


msa.Vowels.prototype.sendRequestToCDN = function (cdnDomain, imagePath,pos) {
    this.addDummyImageToDOM(cdnDomain, imagePath,pos);
};

msa.Vowels.prototype.sendRequestToAllCDNs = function (imagePath,pos) {
	for (var i = 0; i < this.cdnDomains.length; i += 1) {
		var cdnDomain = this.cdnDomains[i];
		this.sendRequestToCDN(cdnDomain, imagePath,pos);
	}
};

msa.Vowels.prototype.startQueryArgsAlgorithm = function () {
    var currTime = (new Date()).getTime();
    var imagePath = "/images/G/01/nav/amazon/amzn-logo-118w.gif" + "?time=" + currTime;
    this.sendRequestToAllCDNs(imagePath,-100);
};

msa.Vowels.prototype.startStatisticalAlgorithm = function () {

	var posCookieName = this.cookieName + ".pos";
	var options = { expires: 3650, path: '/', domain: document.domain, secure: false };
	var pos = this.getCookie(posCookieName);

	if(pos == null){
		pos = "0";
	}

	pos = parseInt(pos);
	pos = pos%this.numImages;
	
	var timeCookieName = this.cookieName  + ".time." + pos; 
	var time = this.getCookie(timeCookieName);
	
	if(time != null){
		time = parseInt(time);	
	}

	var currTime = (new Date()).getTime();
	if(time==null || currTime - time > this.ttl*60*1000){
		this.sendRequestToAllCDNs(this.imagePrefix+pos,pos);
		this.setCookie(posCookieName,pos+1,options);
		this.setCookie(timeCookieName, currTime, options);
	}
};

msa.Vowels.prototype.start = function () {
    this.startQueryArgsAlgorithm();
    this.startStatisticalAlgorithm();
};


msa.Vowels.prototype.initializeAndStart = function () {
    if(location && location.href){
        if(location.href.substring(0,5)=="https"){}
        else{
            this.protocol = 'http';
            var vowels = this;
            vowels.$("body").ready(function () {
                    var existingDivElem = vowels.$("#"+vowels.divName);
                    
                    if(existingDivElem.length) {
                            vowels.$("body").append(existingDivElem);       
                    }
                    else {
                            vowels.$("body").append("<div id='"+vowels.divName+"'>Fooooo</div>");
                    }

                    vowels.$("#"+vowels.divName).ready(function () {
                            vowels.divElem = vowels.$("#"+vowels.divName); 
                            vowels.divElem.empty(); 
                            vowels.start();
                    });     
            });
        }
    }
}

/**
 * 网易邮箱通用显示邮件数,可以添加到任意163域下的页面
 */
function NtesMailInfo(){
	// 根据显示不同需求配置
	// 显示帐号
	this.showAccount = true;
	// 显示邮件数
	this.showMsgCount = true;
	// 显示退出|登录
	this.showLoginout = true;
	// 显示帐号后缀
	this.showAccountSuffix  = true;
	// 显示帐号最多的长度
	this.maxAccountLength = 7;

	this.getCookie = function (sName){
		var sSearch = sName + "=";
		if(document.cookie.length > 0) {
			var sOffset = document.cookie.indexOf(sSearch);
			if(sOffset != -1) {
				sOffset += sSearch.length;
				sEnd = document.cookie.indexOf(";", sOffset);
				if(sEnd == -1) sEnd = document.cookie.length;
				return unescape(document.cookie.substring(sOffset, sEnd));
			}
			else return "";
		}
	};
	// 初始化
	this.init = function (){
		if(this.P_INFO){ // 如果cookie有pinfo,获取帐号
			var arr = this.P_INFO.split("|");
			this.username = arr[0];
			if(this.username.indexOf("@126.com") > -1){
				this.domain = "126.com";
			}
			if(this.username.indexOf("@yeah.net") > -1){
				this.domain = "yeah.net";
			}
			if(this.username.indexOf("@163.com") > -1){
				this.domain = "163.com";
			}
			/*if(this.username.indexOf("@188.com") > -1){
				this.domain = "188.com";
			}
			if(this.username.indexOf("@vip.163.com") > -1){
				this.domain = "vip.163.com";
			}*/
			if(arr[2] == 1){
				this.isLogin = true;
			}
		}else{// 否则返回
			return;
		}
		if(this.cm_newmsg){// 如果有新邮件数目cookie,并且帐号和pinfo里面的一致,设置hasnew标记为true
			var arr = this.cm_newmsg.split("&");
			if(arr[0]){
				var sUserName = arr[0].substr(5);
				if(sUserName == this.username){
					this.hasnew = true;
					if(arr[1]){
						this.newCount = arr[1].substr(4);
					}
				}
			}
		}
	};
	// 生成html
	this.render = function (){
		if(this.domain == "") return;
		if(this.hasnew){
			var sUserName = this.username;
			if(sUserName.indexOf("@") > -1){
				sUserName = sUserName.split("@")[0];
			}
			if(sUserName.length > this.maxAccountLength){
				sUserName = sUserName.substring(0, this.maxAccountLength) + "..";
			}
			this.$("dvNewMsg").style.display = "";		// 显示界面
			
			this.$("imgNewMsg").title = "您有"+ this.newCount +"封未读邮件";
			this.$("lnkNewMsg").innerHTML = this.newCount > 999 ? "999+" : this.newCount; // 新邮件数目
			if(this.newCount == 0){ // 邮件数为0或者大于0是显示不同的图标
				this.$("imgNoNewMsg").style.display = "";
				this.$("imgNewMsg").style.display = "none";
			}else{
				this.$("imgNoNewMsg").style.display = "none";
				this.$("imgNewMsg").style.display = "";
			}
			
			this.$("lnkNewMsg").href = this.$("lnkMsgImg").href = this.getLoginUrl(); // 设置邮件数的链接
		}else{
			if(this.domain != "163.com" && location.hostname.indexOf("163.com") > -1){
				location.href = this.getShowNewMsgUrl();
			}else{
				if(!window.gGetNewCount){
					window.gGetNewCount = true;
					void('<iframe src="about:blank" style="display:none;" id="ifrmNtesMailInfo" onloaddisabled="gNtesMailInfo=new NtesMailInfo();"></iframe>');
					this.$("ifrmNtesMailInfo").src = this.getNewCountUrl();
				}
			}
		}
	};
	this.redirect = function (bType){
		if(this.redirected) return;
		this.redirected = true;
		if(bType == "iframe"){
			this.$("ifrmNtesMailInfo").src = this.getShowNewMsgUrl();
		}else{
			location.href = this.getShowNewMsgUrl();
		}
	};
	this.$ = function (sId){
		return document.getElementById(sId);
	};
	this.P_INFO = this.getCookie("P_INFO");			// 获取pinfocookie
	this.cm_newmsg = this.getCookie("cm_newmsg");	// 获取新邮件数目cookie
	this.isLogin = this.getCookie("S_INFO") ? true : false; // 当前是否登录urs
	this.username = "";								// 帐号
	this.hasnew = false;							// cookie是否有新邮件数目
	this.domain = "";						// 域名
	this.newCount = 0;								// 新邮件数目
	this.redirected = false;						// 是否redirect
	this.isHomePage = location.hostname == "www.163.com" ? true : false;
	// 类型,show:显示数目页面, crossdomain:跨域跳转页面, init:引用js的163频道页面
	this.type = (location.href.indexOf("/mailinfo/shownewmsg_0225.htm") > -1 ? "show" : (location.href.indexOf("/mailinfo/crossdomain_0225.htm") > -1 ? "crossdomain" : "init")); 
	
	this.getShowNewMsgUrl = function (){// 显示邮件数目信息页面
		return "httpdisabled://p.mail."+ this.domain +"/mailinfo/shownewmsg_www_1222.htm";
	};
	
	this.getNewCountUrl = function (){ // 获取新邮件接口url
		return "httpdisabled://msg.mail."+ this.domain +"/cgi/mc?funcid=getusrnewmsgcnt&fid=1&addSubFdrs=1&language=0&style=0&template=newmsgres_setcookie.htm&username=" + this.username;
	};
	this.getLoginUrl = function (){ // 获取登录url
		var oEntryUrl = {
			"163.com" : "httpdisabled://entry.mail.163.com/coremail/fcg/ntesdoor2?lightweight=1&verifycookie=1&language=-1&style=-1&from=newmsg_www",
			"126.com" : "httpdisabled://entry.mail.126.com/cgi/ntesdoor?lightweight=1&verifycookie=1&language=-1&style=-1&from=newmsg_www",
			"yeah.net" : "httpdisabled://entry.mail.yeah.net/cgi/ntesdoor?lightweight=1&verifycookie=1&language=-1&style=-1&from=newmsg_www"
		};
		if(!this.isLogin){
			oEntryUrl = {
				"163.com" : "httpdisabled://email.163.com/?from=newmsg#163",
				"126.com" : "httpdisabled://email.163.com/?from=newmsg#126",
				"yeah.net" : "httpdisabled://email.163.com/?from=newmsg#yeah"
			}
		}
		return oEntryUrl[this.domain];
	};
	
	// if(!this.isLogin) return; // 如果没有登录直接返回
	this.init(); // 初始化
	this.render();// 生成html
}
var gNtesMailInfo = new NtesMailInfo();
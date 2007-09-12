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
 * The Original Code is the Mozilla Community QA Extension
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Zach Lipton <zach@zachlipton.com>
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
// Generate XML result data and submit it to Litmus

// Note that this is essentially a direct port of the Test::Litmus perl
// module. Please see http://search.cpan.org/~zlipton/Test-Litmus-0.03/lib/Test/Litmus.pm
// for further information

// See also the Litmus Web Services Specification at
// http://wiki.mozilla.org/Litmus:Web_Services

const VERSION = '0.0.2';

/**
 * Result constructor.
 *
 * @param machinename
 *  (optional)
 *  The name of the machine running the tests
 * @param username
 *  Username of the account submitting results
 * @param password
 *        Authentication token to be sent to the Litmus server
 * @param server
 *        (optional)
 *  The server to send results to
*/
function LitmusResults(a) {
  this.machinename = a.machinename || '';
  this.requireField('username', a);
  this.requireField('password', a);
  this.server = a.server || 'http://litmus.mozilla.org/process_test.cgi';
  this.action = 'submit';

  this.results = new Array();
  this.logs = new Array();
}

LitmusResults.prototype = {
  // add fieldname to this unless it's missing, at which point we throw an
  // exception to the caller
  requireField : function (name, args) {
    if (args[name] === undefined) {
      throw "Missing required field in Litmus result submission: "+name;
    }
    this[name] = args[name];
  },
  sysconfig : function (a) {
    this.requireField('product', a);
    this.requireField('platform', a);
    this.requireField('opsys', a);
    this.requireField('branch', a);
    this.requireField('buildid', a);
    this.requireField('locale', a);
    this.buildtype = a.buildtype;
  },
  addLog : function(log) {
    this.logs.push(log);
  },
  addResult : function(result) {
    this.results.push(result);
  },
  toXML : function() {
    var d = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>'+"\n";
    d += '<!DOCTYPE litmusresults PUBLIC' +
               ' "-//Mozilla Corporation//Litmus Result Submission DTD//EN/"' +
               ' "http://litmus.mozilla.org/litmus_results.dtd">'+"\n";
    d += '<litmusresults action="'+this.action+'" useragent="'+
      'litmusReporter.js/'+VERSION+' ('+this.machinename+')" '+
      (this.machinename ? 'machinename="'+this.machinename+'">' : '>') +"\n";
    d += '  <testresults username="'+this.username+'"'+"\n";
    d += '     authtoken="'+this.password+'"'+"\n";
    d += '     product="'+this.product+'"'+"\n";
    d += '     platform="'+this.platform+'"'+"\n";
    d += '     opsys="'+this.opsys+'"'+"\n";
    d += '     branch="'+this.branch+'"'+"\n";
    d += '     buildid="'+this.buildid+'"'+"\n";
    d += '     locale="'+this.locale+'"';
    if (this.buildtype) {
      d += "\n"+'     buildtype="'+this.buildtype+'"'+">\n";
    } else {
      d += ">\n";
    }
    if (this.logs) {
      if (this.logs instanceof Array) {
        for(var i=0; i<this.logs.length; i++) {
          if (this.logs[i] instanceof Log) {
            d += this.logs[i].toXML();
          }
        }
      } else {
        if (this.logs instanceof Log) {
          d += this.logs.toXML();
        }
      }
    }
    if (this.results) {
      for(var i=0; i<this.results.length; i++) {
        d += this.results[i].toXML();
      }
    }
    d += '  </testresults>'+"\n";
      d += '</litmusresults>'+"\n";
    return d;
  }
};

/**
  * Log constructor.
  *
  * @param type
  *        The log type
  * @param data
  *        The log data to submit
*/
function Log(a) {
  this.requireField('type', a);
  this.requireField('data', a);
}

Log.prototype = {
  type: null,
  data: null,
  requireField : function (name, args) {
    if (args[name] === undefined) {
      throw "Missing required field in Litmus result submission: "+name;
    }
    this[name] = args[name];
  },
  toXML : function () {
    var d = '<log logtype="'+this.type+'">'+"\n";
    d += '  <![CDATA['+this.data+']]>'+"\n";
    d += '</log>'+"\n";
    return d;
  }
};

/**
  * Result constructor.
  *
*/
function Result(a) {
  this.requireField('testid', a);
  this.requireField('resultstatus', a);
  this.requireField('exitstatus', a);
  this.requireField('duration', a);

  // if no timestamp specified, use the current time
  if (a.timestamp) { this.timestamp = a.timestamp; }
  else {
    var d = new Date();
    this.timestamp=''+d.getFullYear()+leadZero((d.getMonth()+1))+
      leadZero(d.getDate())+leadZero(d.getHours())+leadZero(d.getMinutes())+
      leadZero(d.getSeconds());
  }

  this.comment = a.comment;
  this.bugnumber = a.bugnumber;
  this.logs = a.log;
  this.automated = a.isAutomatedResult ? a.isAutomatedResult : 1;
  }

  Result.prototype = {
  requireField : function (name, args) {
    if (args[name] === undefined) {
      throw "Missing required field in Litmus result submission: "+name;
    }
    this[name] = args[name];
  },
  toXML : function() {
    var d  = '<result testid="'+this.testid+'"'+"\n";
    d += '		 is_automated_result="'+this.automated+'"'+"\n";
    d += '      resultstatus="'+this.resultstatus+'"'+"\n";
    d += '      exitstatus="'+this.exitstatus+'"'+"\n";
    d += '      duration="'+this.duration+'"'+"\n";
    d += '      timestamp="'+this.timestamp+'">'+"\n";

    if (this.comment) {
      d += '  <comment>'+this.comment+'</comment>'+"\n";
    }

    if (this.bugnumber) {
      d += '  <bugnumber>'+this.bugnumber+'</bugnumber>'+"\n";
    }
    if (this.logs) {
      if (this.logs instanceof Array) {
        for(var i=0; i<this.logs.length; i++) {
          if (this.logs[i] instanceof Log) {
            d += this.logs[i].toXML();
          }
        }
      } else {
        if (this.logs instanceof Log) {
          d += this.logs.toXML();
        }
      }
    }
    d += '</result>'+"\n";
    return d;
  }
};

function leadZero(num) {
  if (num.toString().length == 1) { return '0'+num }
  return num;
}

var noDHTML = false;
if (parseInt(navigator.appVersion) < 4) {
  window.event = 0;
  noDHTML = true;
} else if (navigator.userAgent.indexOf("MSIE") > 0 ) {
  noDHTML = true;
}
if (document.body && document.body.addEventListener) {
  document.body.addEventListener("click",maybeclosepopup,false);
}
setTimeout('location.reload()',900000);

function nodewrite(n,t) {
  var r = document.createRange();
  r.setStart(n,0);
  n.appendChild(r.createContextualFragment(t));
}

function closepopup() {
  var p = document.getElementById("popup");
  if (p && p.parentNode) {
    p.parentNode.removeChild(p);
  }
}

function maybeclosepopup(e) {
  var n = e.target;
  var close = true;
  while(close && n && (n != document)) {
    close = (n.id != "popup") && !(n.tagName && (n.tagName.toLowerCase() == "a"));
    n = n.parentNode;
  }
  if (close) closepopup();
}

function who(d) {
  if (noDHTML) {
    return true;
  }
  if (typeof document.layers != 'undefined') {
    var l  = document.layers['popup'];
    l.src  = d.target.href;
    l.top  = d.target.y - 6;
    l.left = d.target.x - 6;
    if (l.left + l.clipWidth > window.width) {
      l.left = window.width - l.clipWidth;
    }
    l.visibility="show";
  } else {
    var t = d.target;
    while (t.nodeType != 1) {
      t = t.parentNode;
    }
    closepopup()
    l = document.createElement("iframe");
    l.setAttribute("src", t.href);
    l.setAttribute("id", "popup");
    l.className = "who";
    t.appendChild(l);
  }
  return false;
}

function log_url(report_id) {
  return "display_report.pl?report_id=" + report_id;
}

function comment(d,commentid,logfile) {
  if (noDHTML) {
    document.location = log_url(logfile);
    return false;
  }
  if (typeof document.layers != 'undefined') {
    var l = document.layers['popup'];
    l.document.write("<table border=1 cellspacing=1><tr><td>"
                     + comments[commentid] + "</tr></table>");
    l.document.close();
    l.top = d.y-10;
    var zz = d.x;
    if (zz + l.clip.right > window.innerWidth) {
      zz = (window.innerWidth-30) - l.clip.right;
      if (zz < 0) { zz = 0; }
    }
    l.left = zz;
    l.visibility="show";
  } else {
    var t = d.target;
    while (t.nodeType != 1) {
      t = t.parentNode;
    }
    closepopup()
    l = document.createElement("div");
    nodewrite(l,comments[commentid]);
    l.setAttribute("id", "popup");
    l.style.position = "absolute";
    l.className = "comment";
    t.parentNode.parentNode.appendChild(l);
  }
  return false;
}

function log(e,buildindex,logfile) {
  var logurl = log_url(logfile);
  var commenturl = "add_comment.pl?log=" + buildtree + "/" + logfile;

  if (noDHTML) {
    document.location = logurl;
    return false;
  }
  if (typeof document.layers != 'undefined') {
    var q = document.layers["logpopup"];
    q.top = e.target.y - 6;

    var yy = e.target.x;
    if ( yy + q.clip.right > window.innerWidth) {
      yy = (window.innerWidth-30) - q.clip.right;
      if (yy < 0) { yy = 0; }
    }
    q.left = yy;
    q.visibility="show"; 
    q.document.write("<TABLE BORDER=1><TR><TD><B>"
      + builds[buildindex] + "</B><BR>"
      + "<A HREF=" + logurl + ">View Brief Log</A><BR>"
      + "<A HREF=" + logurl + "&fulltext=1"+">View Full Log</A><BR>"
      + "<A HREF=" + commenturl + ">Add a Comment</A>"
      + "</TD></TR></TABLE>");
    q.document.close();
  } else {
    var t = e.target;
    while (t.nodeType != 1) {
      t = t.parentNode;
    }
    closepopup();
    var l = document.createElement("div");
    nodewrite(l,"<B>" + builds[buildindex] + "</B><BR>"
      + "<A HREF=" + logurl + ">View Brief Log</A><BR>"
      + "<A HREF=" + logurl + "&fulltext=1"+">View Full Log</A><BR>"
      + "<A HREF=" + commenturl + ">Add a Comment</A><BR>");
    l.setAttribute("id", "popup");
    l.className = "log";
    t.parentNode.appendChild(l);
  }
  return false;
}

function empty_comparison_text() {
  if (document.buffalo_search.comparison_text.value == "Enter comparison text") {
    document.buffalo_search.comparison_text.value = "";
  }
}

 ----------------
 mochitest README
 ----------------

Steps to get started:

 1.) Run the runtests.pl script to start the server.
 
     Currently, the test script automatically determines the location
     of *Firefox*.

 2.) gen_template.pl will generate test templates for HTML, XUL, and XHTML.
     Read the comment at the top of the file for usage instructions.

 3.) Write a test.


Example test:

<!DOCTYPE HTML>
<html>
<!--
https://bugzilla.mozilla.org/show_bug.cgi?id=345656
-->
<head>
  <title>Test for Bug 345656</title>
  <script type="text/javascript" src="/MochiKit/MochiKit.js"></script>
  <script type="text/javascript" src="/tests/SimpleTest/SimpleTest.js"></script>        
  <link rel="stylesheet" type="text/css" href="/tests/SimpleTest/test.css" />
</head>
<body>
<p id="display"></p>
<div id="content" style="display: none">
  
</div>
<pre id="test">
<script class="testbody" type="text/javascript">

/** Test for Bug 345656 **/
//
//add information to show on the test page
//
$("display").innerHTML = "doing stuff...";

//
// The '$' is function is shorthand for getElementById. This is the same thing:
//
document.getElementById("display").innerHTML = "doing stuff...";

//
// you can add content that you don't want to clutter 
// the display to the content div.
//
// You can write directly, or you can use MochiKit functions
// to do it in JS like this:
//
appendChildNodes($("content"),
                 DIV({class: "qux"},
                     SPAN({id: "span42"}, "content"))
                 );

//
// the ok() function is like assert
//
ok(true, "checking to see if true is true);

//
// this will fail
//
ok(1==2, "1 equals 2?");


//
// this will be marked as a todo.
// When we fix 1 so it equals 2, we'll need to change this 
// function to ok() or is().
//
todo(1==2, "1 equals 2?");

//
// is() takes two args
//
myVar = "foo";
is(myVar, "foo", "checking to see if myVar is 'foo'");

//
// Tests can run in event handlers.
// Call this to tell SimpleTest to wait for SimpleTest.finish() 
//
SimpleTest.waitForExplicitFinish();

//
// event handler:
//
function event_fired(ev) {
  is(ev.newValue, "width: auto;", "DOMAttrModified event reports correct newValue");
  SimpleTest.finish(); // trigger the end of our test sequence
}

//
// Hook up the event. Mochikit.Signal has many conveniences for this, if you want.
//
$("content").addEventListener("DOMAttrModified", event_fired, false);

//
// Fire the event.
//
$("content").style.width = "auto";

</script>
</pre>
</body>
</html>

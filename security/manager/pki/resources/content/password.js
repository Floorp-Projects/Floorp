const nsPK11TokenDB = "thayes@netscape.com/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var params;
var tokenName;
var pw1;

function onLoad()
{
  pw1 = document.getElementById("pw1");

  params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
  tokenName = params.GetString(1);

  // Set token name in display
  var t = document.getElementById("tokenName");
  t.setAttribute("value", tokenName);

  // Select first password field
  document.getElementById('pw1').focus();
}

function setPassword()
{
  var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
  var token = pk11db.findTokenByName(tokenName);
  token.initPassword(pw1.value);

  // Return value
  params.SetInt(1, 1);

  // Terminate dialog
  window.close();
}

function setPasswordStrength()
{
// Here is how we weigh the quality of the password
// number of characters
// numbers
// non-alpha-numeric chars
// upper and lower case characters

  var pw=document.getElementById('pw1').value;
//  alert("password='" + pw +"'");

//length of the password
  var pwlength=(pw.length);
  if (pwlength>5)
    pwlength=5;
  document.getElementById('pwchars').setAttribute("value",pw.length);


//use of numbers in the password
  var numnumeric = pw.replace (/[0-9]/g, "");
  var numeric=(pw.length - numnumeric.length);
  document.getElementById('pwnumbers').setAttribute("value",numeric);
  if (numeric>3)
    numeric=3;

//use of symbols in the password
  var symbols = pw.replace (/\W/g, "");
  var numsymbols=(pw.length - symbols.length);
  document.getElementById('pwsymbols').setAttribute("value",numsymbols);
  if (numsymbols>3)
    numsymbols=3;

//use of uppercase in the password
  var numupper = pw.replace (/[A-Z]/g, "");
  var upper=(pw.length - numupper.length);
  document.getElementById('pwupper').setAttribute("value",upper);
  if (upper>3)
    upper=3;


  var pwstrength=((pwlength*10)-20) + (numeric*10) + (numsymbols*15) + (upper*10);

  var mymeter=document.getElementById('pwmeter');
  mymeter.setAttribute("value",pwstrength);

  return;
}

function checkPasswords()
{
  var pw1=document.getElementById('pw1').value;
  var pw2=document.getElementById('pw2').value;

  var ok=document.getElementById('ok-button');

  if (pw1 == "") {
    ok.setAttribute("disabled","true");
    return;
  }

  if (pw1 == pw2){
    ok.setAttribute("disabled","false");
  } else
  {
    ok.setAttribute("disabled","true");
  }

}

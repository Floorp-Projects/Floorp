var ScriptableUnicodeConverter =
  Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
                         "nsIScriptableUnicodeConverter"); 
                         
function getHMAC(data, key)
{
  var converter = new ScriptableUnicodeConverter(); 
  converter.charset = 'utf8'; 
  var keyarray = converter.convertToByteArray(key, {});
  var dataarray = converter.convertToByteArray(data, {});
  
  var cryptoHMAC = Components.classes["@mozilla.org/security/hmac;1"]
    .createInstance(Components.interfaces.nsICryptoHMAC);
    
  cryptoHMAC.init(Components.interfaces.nsICryptoHMAC.SHA1, keyarray, keyarray.length);
  cryptoHMAC.update(dataarray, dataarray.length);
  var digest1 = cryptoHMAC.finish(true);
  
  cryptoHMAC.reset();
  cryptoHMAC.update(dataarray, dataarray.length);
  var digest2 = cryptoHMAC.finish(true);
  
  do_check_eq(digest1, digest2);
  
  return digest1;
}

function run_test() {
  const key1 = 'MyKey_ABCDEFGHIJKLMN';
  const key2 = 'MyKey_01234567890123';
  
  const dataA = "Secret message";
  const dataB = "Secres message";
  
  var diegest1a = getHMAC(key1, dataA);
  var diegest2 = getHMAC(key2, dataA);
  var diegest1b = getHMAC(key1, dataA);
  
  do_check_eq(diegest1a, diegest1b);
  do_check_neq(diegest1a, diegest2);
  
  var diegest1 = getHMAC(key1, dataA);
  diegest2 = getHMAC(key1, dataB);
  
  do_check_neq(diegest1, diegest2);
}

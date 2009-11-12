var ScriptableUnicodeConverter =
  Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
                         "nsIScriptableUnicodeConverter"); 
                         
function getHMAC(data, key, alg)
{
  var converter = new ScriptableUnicodeConverter(); 
  converter.charset = 'utf8'; 
  var dataarray = converter.convertToByteArray(data);
  
  var keyObject = Components.classes["@mozilla.org/security/keyobjectfactory;1"]
    .getService(Components.interfaces.nsIKeyObjectFactory)
      .keyFromString(Components.interfaces.nsIKeyObject.HMAC, key);
  
  var cryptoHMAC = Components.classes["@mozilla.org/security/hmac;1"]
    .createInstance(Components.interfaces.nsICryptoHMAC);
    
  cryptoHMAC.init(alg, keyObject);
  cryptoHMAC.update(dataarray, dataarray.length);
  var digest1 = cryptoHMAC.finish(false);
  
  cryptoHMAC.reset();
  cryptoHMAC.update(dataarray, dataarray.length);
  var digest2 = cryptoHMAC.finish(false);
  
  do_check_eq(digest1, digest2);
  
  return digest1;
}

function testHMAC(alg) {
  const key1 = 'MyKey_ABCDEFGHIJKLMN';
  const key2 = 'MyKey_01234567890123';
  
  const dataA = "Secret message";
  const dataB = "Secres message";
  
  var diegest1a = getHMAC(key1, dataA, alg);
  var diegest2 = getHMAC(key2, dataA, alg);
  var diegest1b = getHMAC(key1, dataA, alg);
  
  do_check_eq(diegest1a, diegest1b);
  do_check_neq(diegest1a, diegest2);
  
  var diegest1 = getHMAC(key1, dataA, alg);
  diegest2 = getHMAC(key1, dataB, alg);
  
  do_check_neq(diegest1, diegest2);
  
  return diegest1;
}

function hexdigest(data) {
  return [("0" + data.charCodeAt(i).toString(16)).slice(-2) for (i in data)].join("");
}

function testVectors() {
  // These are test vectors taken from RFC 4231, section 4.3. (Test Case 2)
  
  const keyTestVector = "Jefe";
  const dataTestVector = "what do ya want for nothing?";

  var diegest;
  /*
  Bug 356713    
  diegest = hexdigest(getHMAC(dataTestVector, keyTestVector, 
          Components.interfaces.nsICryptoHMAC.SHA224));
  do_check_eq(diegest, "a30e01098bc6dbbf45690f3a7e9e6d0f8bbea2a39e6148008fd05e44");
  */

  diegest = hexdigest(getHMAC(dataTestVector, keyTestVector, 
          Components.interfaces.nsICryptoHMAC.SHA256));
  do_check_eq(diegest, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"); 

  diegest = hexdigest(getHMAC(dataTestVector, keyTestVector, 
          Components.interfaces.nsICryptoHMAC.SHA384));
  do_check_eq(diegest, "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649"); 

  diegest = hexdigest(getHMAC(dataTestVector, keyTestVector, 
          Components.interfaces.nsICryptoHMAC.SHA512));
  do_check_eq(diegest, "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737"); 
}

function run_test() {
  testVectors();

  testHMAC(Components.interfaces.nsICryptoHMAC.SHA1);
  testHMAC(Components.interfaces.nsICryptoHMAC.SHA512);
  testHMAC(Components.interfaces.nsICryptoHMAC.MD5);
}

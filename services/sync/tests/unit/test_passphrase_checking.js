Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/identity.js");
Function.prototype.async = Async.sugar;

let __fakeCryptoID = {
  keypairAlg: "RSA",
  // This private key is encrypted with the passphrase 'passphrase',
  // contained in our testing infrastructure.
  privkey: "3ytj94K6Wo0mBjAVsiIwjm5x2+ENvpKTDUqLCz19iXbESf8RT6O8PmY7Pqcndpn+adqaQdvmr0T1JQ5bfLEHev0WBfo8oWJb+OS4rKoCWxDNzGwrOlW5hCfxSekw0KrKjqZyDZ0hT1Qt9vn6thlV2v9YWfmyn0OIxNC9hUqGwU3Wb2F2ejM0Tw40+IIW4eLEvFxLGv0vnEXpZvesPt413proL6FGQJe6vyapBg+sdX1JMYGaKZY84PUGIiDPxTbQg7yIWTSe3WlDhJ001khFiyEoTZvPhiAGXfML9ycrCRZUWkHp/cfS7QiusJXs6co0tLjrIk/rTk8h4mHBnyPkFIxh4YrfC7Bwf9npwomhaZCEQ32VK+a8grTDsGYHPZexDm3TcD2+d+hZ/u4lUOHFscQKX4w83tq942yqFtElCD2yQoqEDr1Z9zge5XBnLcYiH9hL0ozfpxBlTtpR1kSH663JHqlYim0qhuk0zrGAPkHna07UMFufxvgQBSd/YUqWCimJFGi+5QeOOFO20Skj882Bh1QDYsmbxZ/JED5ocGNHWSqpaOL2ML1F9nD5rdtffI0BsTe+j9h+HV4GlvzUz0Jd6RRf9xN4RyxqfENb8iGH5Pwbry7Qyk16rfm0s6JgG8pNb/8quKD+87RAtQFybZtdQ9NfGg+gyRiU9pbb6FPuPnGp+KpktaHu/K3HnomrVUoyLQALfCSbPXg2D9ta6dRV0JRqOZz4w52hlHIa62iJO6QecbdBzPYGT0QfOy/vp6ndRDR+2xMD/BmlaQwm3+58cqhIw9SVV5h/Z5PVaXxAOqg5vpU1NjrbF4uIFo5rmR0PyA/6qtxZaBY6w3I4sUWdDkIer8QsyrFrO7MIEdxksvDoFIeIM5eN8BufLu3ymS5ZXBiFr/iRxlYcQVHK2hz0/7syWUYsrz5/l1mj+qbWGx+6daWOk3xt4SH+p0hUpMC4FbJ9m/xr4im+X5m5ZYiajaF1QPOXTTny2plE0vfqMVlwX1HFFTJrAP+E85sZI8LPHAYO80qhSi3tV/LHjxCnC1LHJXaRkG202pQFWF1yVT/o82HBt9OC1xY6TVcy4Uh+6piNIQ9FxXGWrzjz0AUkxwkSN3Foqlfiq+mqJmNwzIdEQTmNAcBBsN3vWngU4elHjYI5qFZBzxJIkH8tfvivOshrOZIZB9TD9GIRhQwIBWc6i4fnqE9GUK2Jle6werdFATiMU4msQg7ClURaMn/p3MOLoxTmsPd1iBYPQkqnJgEAdNfKj3KRqSc6M/x09hGDSzK2d9Y03pyDGPh2sopcMFdCQbMy8VOld2/hEGakMJv6BPoRfhKmJbgGVf5x4B9dWZNa8WCmlcxaZ7KG43UA0zLm1VgfTcDW5qazDFoxIcfhmO5KoRI3q8vNs+Wh+smLC6yFODdF9HzrPimEYSc6OWHWgUcuiIBRjKeo5gBTbExWmri2VG+cn005vQNxK+0s7JVyFB8TzZ96pV3nFjkYy9OUkaiJxGd6OVGcvhbbrcNsKkaZff7OsLqczf6x0bhwh+y8+bLjLkuusGYUdBvdeiuv12IfeRupvwD8Z3aZOgcD7d+8VTyTyd/KX9fu8P7tD5SojJ5joRPjcv4Q8/mhRgtwx1McMIL3YnKHG+U=",
  privkeyWrapIV: "fZ7CB/KQAUjEhkmrEkns4Q==",
  passphraseSalt: "JFs5h2RKX9m0Op9DlQIcCOSfOH1MuDrrrHxCx+CpCUU=",
  pubkey: "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxxwObnXIoYeQKMG9RbvLkgsu/idDo25qOX87jIEiXkgW1wLKp/1D/DBLUEW303tVszNGVt6bTyAXOIj6skpmYoDs9Z48kvU3+g7Vi4QXEw5moSS4fr+yFpKiYd2Kx1+jCFGvGZjBzAAvnjsWmWrSA+LHJSrFlKY6SM3kNg8KrE8dxUi3wztlZnhZgo1ZYe7/VeBOXUfThtoadIl1VdREw2e79eiMQpPa0XLv4grCaMd/wLRs0be1/nPt7li4NyT0fnYFWg75SU3ni/xSaq/zR4NmW/of5vB2EcKyUG+/mvNplQ0CX+v3hRBCdhpCyPmcbHKUluyKzj7Ms9pKyCkwxwIDAQAB"
  };

function testGenerator() {
  let self = yield;

  let id = ID.get("WeaveCryptoID");
  for (name in __fakeCryptoID)
    id[name] = __fakeCryptoID[name];

  Crypto.isPassphraseValid.async(Crypto, self.cb, id);
  let result = yield;

  do_check_eq(result, true);

  id.setTempPassword("incorrect passphrase");

  Crypto.isPassphraseValid.async(Crypto, self.cb, id);
  result = yield;

  do_check_eq(result, false);

  self.done();
}

function run_test() {
  let syncTesting = new SyncTestingInfrastructure();

  syncTesting.runAsyncFunc(
    "Ensuring isPassphraseValid() works",
    function runTest(cb) { testGenerator.async({}, cb); }
    );
}

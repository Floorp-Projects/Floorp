function squareAndOne(arg){
  return (arg * arg) + 1;
}
function squareUntil(arg, limit){
  if(arg * arg >= limit){
    return arg * arg;
  }else{
    return squareUntil(arg * arg, limit);
  }
}

function addUntil(arg1, arg2, limit){
  if(arg1 + arg2 > limit){
    return arg1 + arg2;
  }else{
    return addUntil(arg1 + arg2, arg2, limit);
  }
}

function testStart(aArg) {
  var r = 10;
  var a = squareAndOne(r);
  var b = squareUntil(r, 99999999999); //recurses 3 times, returns on 4th call
  var c = addUntil(r, 5, 1050); // recurses 208 times and returns on the 209th call
  return a + b + c;
}

export default testStart;

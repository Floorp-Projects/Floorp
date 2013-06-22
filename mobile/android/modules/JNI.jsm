/* Very basic JNI support for JS
 *
 * Example Usage:
 *   let jni = new JNI();
 *   cls = jni.findClass("org/mozilla/gecko/GeckoAppShell");
 *   method = jni.getStaticMethodID(cls, "getPreferredIconSize", "(I)I");
 *
 *   let val = jni.callStaticIntMethod(cls, method, 3);
 *   // close the jni library when you are done
 *   jni.close();
 *
 * Note: the getters in this file are deleted and replaced with static
 * values once computed, as in, for example
 * http://code.activestate.com/recipes/577310-using-a-getter-for-a-one-time-calculation-of-a-jav/
 */
this.EXPORTED_SYMBOLS = ["JNI"];

Components.utils.import("resource://gre/modules/ctypes.jsm")
Components.utils.import("resource://gre/modules/Services.jsm")

this.JNI = function JNI() { }

JNI.prototype = {
  get lib() {
    delete this.lib;
    return this.lib = ctypes.open("libxul.so");
  },

  getType: function(aType) {
    switch(aType) {
      case "B": return ctypes.char;
      case "C": return ctypes.char;
      case "D": return ctypes.double;
      case "F": return ctypes.float;
      case "I": return ctypes.int32_t;
      case "J": return ctypes.long;
      case "S": return ctypes.short;
      case "V": return ctypes.void_t;
      case "Z": return ctypes.bool;
      default: return this.types.jobject
    }
  },

  getArgs: function(aMethod, aArgs) {
    if (aArgs.length != aMethod.parameters.length)
      throw ("Incorrect number of arguments passed to " + aMethod.name);

    // Convert arguments to an array of jvalue objects
    let modifiedArgs = new ctypes.ArrayType(this.types.jvalue, aMethod.parameters.length)();
    for (let i = 0; i < aMethod.parameters.length; i++) {
      let parameter = aMethod.parameters[i];
      let arg = new this.types.jvalue();

      if (aArgs[i] instanceof Array || parameter[0] == "[")
        throw "No support for array arguments yet";
      else
        ctypes.cast(arg, this.getType(parameter)).value = aArgs[i];

      modifiedArgs[i] = arg;
    }

    return modifiedArgs;
  },

  types: {
    jobject: ctypes.StructType("_jobject").ptr,
    jclass: ctypes.StructType("_jobject").ptr,
    jmethodID: ctypes.StructType("jmethodID").ptr,
    jvalue: ctypes.double
  },

  get _findClass() {
    delete this._findClass;
    return this._findClass = this.lib.declare("jsjni_FindClass",
                                              ctypes.default_abi,
                                              this.types.jclass,
                                              ctypes.char.ptr);
  },

  findClass: function(name) {
    let ret = this._findClass(name);
    if (this.exceptionCheck())
       throw("Can't find class " + name);
    return ret;
  },

  get _getStaticMethodID() {
    delete this._getStaticMethodID;
    return this._getStaticMethodID = this.lib.declare("jsjni_GetStaticMethodID",
                                                      ctypes.default_abi,
                                                      this.types.jmethodID,
                                                      this.types.jclass, // class
                                                      ctypes.char.ptr,   // method name
                                                      ctypes.char.ptr);  // signature
  },

  getStaticMethodID: function(aClass, aName, aSignature) {
    let ret = this._getStaticMethodID(aClass, aName, aSignature);
    if (this.exceptionCheck())
       throw("Can't find method " + aName);
    return new jMethod(aName, ret, aSignature);
  },

  get _exceptionCheck() {
    delete this._exceptionCheck;
    return this._exceptionCheck = this.lib.declare("jsjni_ExceptionCheck",
                                                   ctypes.default_abi,
                                                   ctypes.bool);
  },

  exceptionCheck: function() {
    return this._exceptionCheck();
  },

  get _callStaticVoidMethod() {
    delete this._callStaticVoidMethod;
    return this._callStaticVoidMethod = this.lib.declare("jsjni_CallStaticVoidMethodA",
                                                   ctypes.default_abi,
                                                   ctypes.void_t,
                                                   this.types.jclass,
                                                   this.types.jmethodID,
                                                   this.types.jvalue.ptr);
  },

  callStaticVoidMethod: function(aClass, aMethod) {
    let args = Array.prototype.slice.apply(arguments, [2]);
    this._callStaticVoidMethod(aClass, aMethod.methodId, this.getArgs(aMethod, args));
    if (this.exceptionCheck())
       throw("Error calling static void method");
  },

  get _callStaticIntMethod() {
    delete this._callStaticIntMethod;
    return this._callStaticIntMethod = this.lib.declare("jsjni_CallStaticIntMethodA",
                                                   ctypes.default_abi,
                                                   ctypes.int,
                                                   this.types.jclass,
                                                   this.types.jmethodID,
                                                   this.types.jvalue.ptr);
  },

  callStaticIntMethod: function(aClass, aMethod) {
    let args = Array.prototype.slice.apply(arguments, [2]);
    let ret = this._callStaticIntMethod(aClass, aMethod.methodId, this.getArgs(aMethod, args));
    if (this.exceptionCheck())
       throw("Error calling static int method");
    return ret;
  },

  close: function() {
    this.lib.close();
  },
}

function jMethod(name, jMethodId, signature) {
  this.name = name;
  this.methodId = jMethodId;
  this.signature = signature;
}

jMethod.prototype = {
  parameters: [],
  returnType: null,
  _signature: "",

  // this just splits up the return value from the parameters
  signatureRegExp: /^\(([^\)]*)\)(.*)$/,

  // This splits up the actual parameters
  parameterRegExp: /(\[*)(B|C|D|F|I|J|S|V|Z|L[^;]*;)/y,

  parseSignature: function(aSignature) {
    let [, parameters, returnType] = this.signatureRegExp.exec(aSignature);

    // parse the parameters that should be passed to this method
    if (parameters) {
      let parameter = this.parameterRegExp.exec(parameters);
      while (parameter) {
        this.parameters.push(parameter[0]);
        parameter = this.parameterRegExp.exec(parameters);
      }
    } else {
      this.parameters = [];
    }

    // parse the return type
    this.returnType = returnType;
  },

  _signature: "",
  get signature() { return this._signature; },
  set signature(val) {
    this.parameters = [];
    this.returnType = null;
    this.parseSignature(val);
  }
}

/**
* W3C 3-clause BSD License
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
* o Redistributions of works must retain the original copyright notice,
*     this list of conditions and the following disclaimer.
*
* o Redistributions in binary form must reproduce the original copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*
* o Neither the name of the W3C nor the names of its contributors may be
*     used to endorse or promote products derived from this work without
*     specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
* IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
**/

(function() {
  
  /**
  *
  * partial interface Navigator {
  *     readonly attribute BatteryManager battery;
  * };
  *
  */
  
  test(function() {
    assert_idl_attribute(navigator, 'battery', 'navigator must have battery attribute');
  }, 'battery is present on navigator');
    
  test(function() {
    assert_readonly(navigator, 'battery', 'battery must be readonly');
  }, 'battery is readonly');
  
  /**
  *
  * interface BatteryManager : EventTarget {
  *     readonly attribute boolean             charging;
  *     readonly attribute unrestricted double chargingTime;
  *     readonly attribute unrestricted double dischargingTime;
  *     readonly attribute double              level;
  *              attribute EventHandler        onchargingchange;
  *              attribute EventHandler        onchargingtimechange;
  *              attribute EventHandler        ondischargingtimechange;
  *              attribute EventHandler        onlevelchange;
  * };
  *
  */

  // interface BatteryManager : EventTarget {

  test(function() {
      assert_own_property(window, 'BatteryManager');
  }, 'window has an own property BatteryManager');
  
  test(function() {
      assert_true(navigator.battery instanceof EventTarget);
  }, 'battery inherits from EventTarget');
  
  // readonly attribute boolean charging;
  
  test(function() {
    assert_idl_attribute(navigator.battery, 'charging', 'battery must have charging attribute');
  }, 'charging attribute exists');
  
  test(function() {
    assert_readonly(navigator.battery, 'charging', 'charging must be readonly')
  }, 'charging attribute is readonly');

  // readonly attribute unrestricted double chargingTime;

  test(function() {
    assert_idl_attribute(navigator.battery, 'chargingTime', 'battery must have chargingTime attribute');
  }, 'chargingTime attribute exists');

  test(function() {
    assert_readonly(navigator.battery, 'chargingTime', 'chargingTime must be readonly')
  }, 'chargingTime attribute is readonly');

  // readonly attribute unrestricted double dischargingTime;

  test(function() {
    assert_idl_attribute(navigator.battery, 'dischargingTime', 'battery must have dischargingTime attribute');
  }, 'dischargingTime attribute exists');

  test(function() {
    assert_readonly(navigator.battery, 'dischargingTime', 'dischargingTime must be readonly')
  }, 'dischargingTime attribute is readonly');
  
  // readonly attribute double level;
  
  test(function() {
    assert_idl_attribute(navigator.battery, 'level', 'battery must have level attribute');
  }, 'level attribute exists');

  test(function() {
    assert_readonly(navigator.battery, 'level', 'level must be readonly')
  }, 'level attribute is readonly');
  
  // attribute EventHandler onchargingchange;
  
  test(function() {
    assert_idl_attribute(navigator.battery, 'onchargingchange', 'battery must have onchargingchange attribute');
  }, 'onchargingchange attribute exists');
  
  test(function() {
    assert_equals(navigator.battery.onchargingchange, null, 'onchargingchange must be null')
  }, 'onchargingchange is null');
  
  test(function() {
      var desc = 'onchargingchange did not accept callable object',
          func = function() {},
          desc = 'Expected to find onchargingchange attribute on battery object';
      assert_idl_attribute(navigator.battery, 'onchargingchange', desc);
      window.onchargingchange = func;
      assert_equals(window.onchargingchange, func, desc);
  }, 'onchargingchange is set to function');
  
  test(function() {
      var desc = 'onchargingchange did not treat noncallable as null';
      navigator.battery.onchargingchange = function() {};
      navigator.battery.onchargingchange = {};
      assert_equals(navigator.battery.onchargingchange, null, desc);
  }, 'onchargingchange: treat object as null');
  
  test(function() {
      var desc = 'onchargingchange did not treat noncallable as null';
      navigator.battery.onchargingchange = function() {};
      navigator.battery.onchargingchange = {
          call: 'test'
      };
      assert_equals(navigator.battery.onchargingchange, null, desc);
  }, 'onchargingchange: treat object with non-callable call property as null');
  
  test(function() {
      var desc = 'onchargingchange did not treat noncallable (string) as null';
      navigator.battery.onchargingchange = function() {};
      navigator.battery.onchargingchange = 'string';
      assert_equals(navigator.battery.onchargingchange, null, desc);
  }, 'onchargingchange: treat string as null');

  test(function() {
      var desc = 'onchargingchange did not treat noncallable (number) as null';
      navigator.battery.onchargingchange = function() {};
      navigator.battery.onchargingchange = 123;
      assert_equals(navigator.battery.onchargingchange, null, desc);
  }, 'onchargingchange: treat number as null');

  test(function() {
      var desc = 'onchargingchange did not treat noncallable (undefined) as null';
      navigator.battery.onchargingchange = function() {};
      navigator.battery.onchargingchange = undefined;
      assert_equals(navigator.battery.onchargingchange, null, desc);
  }, 'onchargingchange: treat undefined as null');

  test(function() {
      var desc = 'onchargingchange did not treat noncallable (array) as null';
      navigator.battery.onchargingchange = function() {};
      navigator.battery.onchargingchange = [];
      assert_equals(navigator.battery.onchargingchange, null, desc);
  }, 'onchargingchange: treat array as null');

  test(function() {
      var desc = 'onchargingchange did not treat noncallable host object as null';
      navigator.battery.onchargingchange = function() {};
      navigator.battery.onchargingchange = Node;
      assert_equals(navigator.battery.onchargingchange, null, desc);
  }, 'onchargingchange: treat non-callable host object as null');
  
  // attribute EventHandler onchargingtimechange;
  
  test(function() {
    assert_idl_attribute(navigator.battery, 'onchargingtimechange', 'battery must have onchargingtimechange attribute');
  }, 'onchargingtimechange attribute exists');
  
  test(function() {
    assert_equals(navigator.battery.onchargingtimechange, null, 'onchargingtimechange must be null')
  }, 'onchargingtimechange is null');

  test(function() {
      var desc = 'onchargingtimechange did not accept callable object',
          func = function() {},
          desc = 'Expected to find onchargingtimechange attribute on battery object';
      assert_idl_attribute(navigator.battery, 'onchargingtimechange', desc);
      window.onchargingtimechange = func;
      assert_equals(window.onchargingtimechange, func, desc);
  }, 'onchargingtimechange is set to function');

  test(function() {
      var desc = 'onchargingtimechange did not treat noncallable as null';
      navigator.battery.onchargingtimechange = function() {};
      navigator.battery.onchargingtimechange = {};
      assert_equals(navigator.battery.onchargingtimechange, null, desc);
  }, 'onchargingtimechange: treat object as null');

  test(function() {
      var desc = 'onchargingtimechange did not treat noncallable as null';
      navigator.battery.onchargingtimechange = function() {};
      navigator.battery.onchargingtimechange = {
          call: 'test'
      };
      assert_equals(navigator.battery.onchargingtimechange, null, desc);
  }, 'onchargingtimechange: treat object with non-callable call property as null');

  test(function() {
      var desc = 'onchargingtimechange did not treat noncallable (string) as null';
      navigator.battery.onchargingtimechange = function() {};
      navigator.battery.onchargingtimechange = 'string';
      assert_equals(navigator.battery.onchargingtimechange, null, desc);
  }, 'onchargingtimechange: treat string as null');

  test(function() {
      var desc = 'onchargingtimechange did not treat noncallable (number) as null';
      navigator.battery.onchargingtimechange = function() {};
      navigator.battery.onchargingtimechange = 123;
      assert_equals(navigator.battery.onchargingtimechange, null, desc);
  }, 'onchargingtimechange: treat number as null');

  test(function() {
      var desc = 'onchargingtimechange did not treat noncallable (undefined) as null';
      navigator.battery.onchargingtimechange = function() {};
      navigator.battery.onchargingtimechange = undefined;
      assert_equals(navigator.battery.onchargingtimechange, null, desc);
  }, 'onchargingtimechange: treat undefined as null');

  test(function() {
      var desc = 'onchargingtimechange did not treat noncallable (array) as null';
      navigator.battery.onchargingtimechange = function() {};
      navigator.battery.onchargingtimechange = [];
      assert_equals(navigator.battery.onchargingtimechange, null, desc);
  }, 'onchargingtimechange: treat array as null');

  test(function() {
      var desc = 'onchargingtimechange did not treat noncallable host object as null';
      navigator.battery.onchargingtimechange = function() {};
      navigator.battery.onchargingtimechange = Node;
      assert_equals(navigator.battery.onchargingtimechange, null, desc);
  }, 'onchargingtimechange: treat non-callable host object as null');
    
  // attribute EventHandler ondischargingtimechange;
  
  test(function() {
    assert_idl_attribute(navigator.battery, 'ondischargingtimechange', 'battery must have ondischargingtimechange attribute');
  }, 'ondischargingtimechange attribute exists');

  test(function() {
    assert_equals(navigator.battery.ondischargingtimechange, null, 'ondischargingtimechange must be null')
  }, 'ondischargingtimechange is null');
  
  test(function() {
      var desc = 'ondischargingtimechange did not accept callable object',
          func = function() {},
          desc = 'Expected to find ondischargingtimechange attribute on battery object';
      assert_idl_attribute(navigator.battery, 'ondischargingtimechange', desc);
      window.ondischargingtimechange = func;
      assert_equals(window.ondischargingtimechange, func, desc);
  }, 'ondischargingtimechange is set to function');

  test(function() {
      var desc = 'ondischargingtimechange did not treat noncallable as null';
      navigator.battery.ondischargingtimechange = function() {};
      navigator.battery.ondischargingtimechange = {};
      assert_equals(navigator.battery.ondischargingtimechange, null, desc);
  }, 'ondischargingtimechange: treat object as null');

  test(function() {
      var desc = 'ondischargingtimechange did not treat noncallable as null';
      navigator.battery.ondischargingtimechange = function() {};
      navigator.battery.ondischargingtimechange = {
          call: 'test'
      };
      assert_equals(navigator.battery.ondischargingtimechange, null, desc);
  }, 'ondischargingtimechange: treat object with non-callable call property as null');

  test(function() {
      var desc = 'ondischargingtimechange did not treat noncallable (string) as null';
      navigator.battery.ondischargingtimechange = function() {};
      navigator.battery.ondischargingtimechange = 'string';
      assert_equals(navigator.battery.ondischargingtimechange, null, desc);
  }, 'ondischargingtimechange: treat string as null');

  test(function() {
      var desc = 'ondischargingtimechange did not treat noncallable (number) as null';
      navigator.battery.ondischargingtimechange = function() {};
      navigator.battery.ondischargingtimechange = 123;
      assert_equals(navigator.battery.ondischargingtimechange, null, desc);
  }, 'ondischargingtimechange: treat number as null');

  test(function() {
      var desc = 'ondischargingtimechange did not treat noncallable (undefined) as null';
      navigator.battery.ondischargingtimechange = function() {};
      navigator.battery.ondischargingtimechange = undefined;
      assert_equals(navigator.battery.ondischargingtimechange, null, desc);
  }, 'ondischargingtimechange: treat undefined as null');

  test(function() {
      var desc = 'ondischargingtimechange did not treat noncallable (array) as null';
      navigator.battery.ondischargingtimechange = function() {};
      navigator.battery.ondischargingtimechange = [];
      assert_equals(navigator.battery.ondischargingtimechange, null, desc);
  }, 'ondischargingtimechange: treat array as null');

  test(function() {
      var desc = 'ondischargingtimechange did not treat noncallable host object as null';
      navigator.battery.ondischargingtimechange = function() {};
      navigator.battery.ondischargingtimechange = Node;
      assert_equals(navigator.battery.ondischargingtimechange, null, desc);
  }, 'ondischargingtimechange: treat non-callable host object as null');

  // attribute EventHandler onlevelchange;

  test(function() {
    assert_idl_attribute(navigator.battery, 'onlevelchange', 'battery must have onlevelchange attribute');
  }, 'onlevelchange attribute exists');
  
  test(function() {
    assert_equals(navigator.battery.onlevelchange, null, 'onlevelchange must be null')
  }, 'onlevelchange is null');
  
  test(function() {
      var desc = 'onlevelchange did not accept callable object',
          func = function() {},
          desc = 'Expected to find onlevelchange attribute on battery object';
      assert_idl_attribute(navigator.battery, 'onlevelchange', desc);
      window.onlevelchange = func;
      assert_equals(window.onlevelchange, func, desc);
  }, 'onlevelchange is set to function');

  test(function() {
      var desc = 'onlevelchange did not treat noncallable as null';
      navigator.battery.onlevelchange = function() {};
      navigator.battery.onlevelchange = {};
      assert_equals(navigator.battery.onlevelchange, null, desc);
  }, 'onlevelchange: treat object as null');

  test(function() {
      var desc = 'onlevelchange did not treat noncallable as null';
      navigator.battery.onlevelchange = function() {};
      navigator.battery.onlevelchange = {
          call: 'test'
      };
      assert_equals(navigator.battery.onlevelchange, null, desc);
  }, 'onlevelchange: treat object with non-callable call property as null');

  test(function() {
      var desc = 'onlevelchange did not treat noncallable (string) as null';
      navigator.battery.onlevelchange = function() {};
      navigator.battery.onlevelchange = 'string';
      assert_equals(navigator.battery.onlevelchange, null, desc);
  }, 'onlevelchange: treat string as null');

  test(function() {
      var desc = 'onlevelchange did not treat noncallable (number) as null';
      navigator.battery.onlevelchange = function() {};
      navigator.battery.onlevelchange = 123;
      assert_equals(navigator.battery.onlevelchange, null, desc);
  }, 'onlevelchange: treat number as null');

  test(function() {
      var desc = 'onlevelchange did not treat noncallable (undefined) as null';
      navigator.battery.onlevelchange = function() {};
      navigator.battery.onlevelchange = undefined;
      assert_equals(navigator.battery.onlevelchange, null, desc);
  }, 'onlevelchange: treat undefined as null');

  test(function() {
      var desc = 'onlevelchange did not treat noncallable (array) as null';
      navigator.battery.onlevelchange = function() {};
      navigator.battery.onlevelchange = [];
      assert_equals(navigator.battery.onlevelchange, null, desc);
  }, 'onlevelchange: treat array as null');

  test(function() {
      var desc = 'onlevelchange did not treat noncallable host object as null';
      navigator.battery.onlevelchange = function() {};
      navigator.battery.onlevelchange = Node;
      assert_equals(navigator.battery.onlevelchange, null, desc);
  }, 'onlevelchange: treat non-callable host object as null');
  
})();
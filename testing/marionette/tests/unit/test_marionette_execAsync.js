/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test()
{
  add_test(test_executeAsync);
  add_test(test_executeAsyncTimeout);
  add_test(test_executeAsyncUnload); //TODO: fix unload listener
  run_next_test();
}

function test_executeAsync()
{
  do_test_pending();
  got_session = false;
  received = false;
  id = "";

  let transport = debuggerSocketConnect("127.0.0.1", 2828);
  transport.hooks = {
    onPacket: function(aPacket) {
      this.onPacket = function(aPacket) {
        if(!got_session) {
          got_session=true;
          id = aPacket.id;
          transport.send({to: id,
                        type: "newSession",
                        });
        } 
        else {
        if (received) {
          do_check_eq(aPacket.from, id);
          if(aPacket.ok == true) {
            transport.send({to: id,
                          type: "executeAsyncScript",
                          value: "arguments[arguments.length - 1](5+1);",
                          });
          }
          else if(aPacket.value == "6") {
              transport.send({to: id,
                            type: "deleteSession",
                            });
            transport.close();
          }
          else if(aPacket.error != undefined) {
            do_throw("Received error: " + aPacket.error);
            transport.close();
          }
        }
        else {
          received = true;
          do_check_eq('session', aPacket.value);
          transport.send({to: id,
                        type: "setScriptTimeout",
                        value: "2000",
                        });
        }
      }
      }
      transport.send({to: "root",
                      type: "getMarionetteID",
                      });
    },
    onClosed: function(aStatus) {
      do_check_eq(aStatus, 0);
      do_test_finished();
      delete transport;
      run_next_test();
    },
  };
  transport.ready();
}

function test_executeAsyncTimeout()
{
  do_test_pending();
  got_session = false;
  received = false;
  id = "";

  let transport = debuggerSocketConnect("127.0.0.1", 2828);
  transport.hooks = {
    onPacket: function(aPacket) {
      this.onPacket = function(aPacket) {
        if(!got_session) {
          got_session=true;
          id = aPacket.id;
          transport.send({to: id,
                        type: "newSession",
                        });
        } 
        else {
        if (received) {
          do_check_eq(aPacket.from, id);
          if(aPacket.ok == true) {
            transport.send({to: id,
                          type: "executeAsyncScript",
                          value: "window.setTimeout(arguments[arguments.length - 1], 5000, 6);",
                          });
          }
          else if(aPacket.value == "6") {
            do_throw("Should have timed out!");
            transport.close();
          }
          else if(aPacket.error != undefined) {
            do_check_eq(aPacket.error.message, "timed out");
            do_check_eq(aPacket.error.status, 28);
            transport.close();
          }
        }
        else {
          received = true;
          do_check_eq('session', aPacket.value);
          transport.send({to: id,
                        type: "setScriptTimeout",
                        value: "2000",
                        });
        }
      }
      }
      transport.send({to: "root",
                      type: "getMarionetteID",
                      });
    },
    onClosed: function(aStatus) {
      do_check_eq(aStatus, 0);
      do_test_finished();
      delete transport;
      run_next_test();
    },
  };
  transport.ready();
}

function test_executeAsyncUnload()
{
  do_test_pending();
  got_session = false;
  received = false;
  id = "";

  let transport = debuggerSocketConnect("127.0.0.1", 2828);
  transport.hooks = {
    onPacket: function(aPacket) {
      this.onPacket = function(aPacket) {
        if(!got_session) {
          got_session=true;
          id = aPacket.id;
          transport.send({to: id,
                        type: "newSession",
                        });
        } 
        else {
        if (received) {
          do_check_eq(aPacket.from, id);
          if(aPacket.ok == true) {
            transport.send({to: id,
                          type: "executeAsyncScript",
                          value: "window.location.reload();",
                          });
          }
          else if(aPacket.value == "6") {
            do_throw("Should have thrown unload error!");
            transport.close();
          }
          else if(aPacket.error != undefined) {
            do_check_eq(aPacket.error.status, 17);
            transport.close();
          }
        }
        else {
          received = true;
          do_check_eq('session', aPacket.value);
          transport.send({to: id,
                        type: "setScriptTimeout",
                        value: "2000",
                        });
        }
      }
      }
      transport.send({to: "root",
                      type: "getMarionetteID",
                      });
    },
    onClosed: function(aStatus) {
      do_check_eq(aStatus, 0);
      do_test_finished();
      delete transport;
      run_next_test();
    },
  };
  transport.ready();
}

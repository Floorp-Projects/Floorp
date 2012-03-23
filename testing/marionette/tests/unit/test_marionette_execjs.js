/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test()
{
  add_test(test_execute);
  add_test(test_execute_async);
  add_test(test_execute_async_timeout);
  add_test(test_execute_chrome);
  add_test(test_execute_async_chrome);
  add_test(test_execute_async_timeout_chrome);
  run_next_test();
}

function test_execute()
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
            do_check_eq(aPacket.value.passed, 1);
            do_check_eq(aPacket.value.failed, 0);
            transport.close();
          }
          else {
            received = true;
            do_check_eq('mobile', aPacket.value);
            transport.send({to: id,
                        type: "executeJSScript",
                        value: "Marionette.is(1,1, 'should return 1'); Marionette.finish();",
                        timeout: false
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
      run_next_test();
      delete transport;
    },
  };
  transport.ready();
}

function test_execute_async()
{
  do_test_pending();
  got_session = false;
  received = false;
  received2 = false;
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
            if(received2) {
              do_check_eq(aPacket.from, id);
              do_check_eq(aPacket.value.passed, 1);
              do_check_eq(aPacket.value.failed, 0);
              transport.close();
            }
            else {
              received2 = true;
              transport.send({to: id,
                          type: "executeJSScript",
                          value: "Marionette.is(1,1, 'should return 1'); Marionette.finish();",
                          timeout: true 
                          });
            }
          }
          else {
            received = true;
            do_check_eq('mobile', aPacket.value);
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
      run_next_test();
      delete transport;
    },
  };
  transport.ready();
}

function test_execute_async_timeout()
{
  do_test_pending();
  got_session = false;
  received = false;
  received2 = false;
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
            if(received2) {
              do_check_eq(aPacket.from, id);
              do_check_eq(aPacket.error.status, 28);
              transport.close();
            }
            else {
              received2 = true;
              transport.send({to: id,
                          type: "executeJSScript",
                          value: "Marionette.is(1,1, 'should return 1');",
                          timeout: true 
                          });
            }
          }
          else {
            received = true;
            do_check_eq('mobile', aPacket.value);
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
      run_next_test();
      delete transport;
    },
  };
  transport.ready();
}

function test_execute_chrome()
{
  do_test_pending();
  got_session = false;
  got_context = false;
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
        else if (!got_context) {
          got_context = true;
          do_check_eq('mobile', aPacket.value);
          transport.send({to: id,
                          type: "setContext",
                          value: "chrome",
                         });
        }
        else if (!received) {
          received = true;
          transport.send({to: id,
                      type: "executeJSScript",
                      value: "Marionette.is(1,1, 'should return 1'); Marionette.finish();",
                      timeout: false
                      });
        }
        else {
          do_check_eq(aPacket.from, id);
          do_check_eq(aPacket.value.passed, 1);
          do_check_eq(aPacket.value.failed, 0);
          transport.close();
        }
      }
     transport.send({to: "root",
                     type: "getMarionetteID",
                    });
    },
    onClosed: function(aStatus) {
      do_check_eq(aStatus, 0);
      do_test_finished();
      run_next_test();
      delete transport;
    },
  };
  transport.ready();
}

function test_execute_async_chrome()
{
  do_test_pending();
  got_session = false;
  got_context = false;
  received = false;
  received2 = false;
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
        else if (!got_context) {
          got_context = true;
          do_check_eq('mobile', aPacket.value);
          transport.send({to: id,
                          type: "setContext",
                          value: "chrome",
                         });
        }
        else if (!received) {
          received = true;
          transport.send({to: id,
                        type: "setScriptTimeout",
                        value: "2000",
                        });
        }
        else if (!received2) {
          received2 = true;
          transport.send({to: id,
                          type: "executeJSScript",
                          value: "Marionette.is(1,1, 'should return 1'); Marionette.finish();",
                          timeout: true 
                          });
        }
        else {
          do_check_eq(aPacket.from, id);
          do_check_eq(aPacket.value.passed, 1);
          do_check_eq(aPacket.value.failed, 0);
          transport.close();
        }
      }
      transport.send({to: "root",
                      type: "getMarionetteID",
                      });
    },
    onClosed: function(aStatus) {
      do_check_eq(aStatus, 0);
      do_test_finished();
      run_next_test();
      delete transport;
    },
  };
  transport.ready();
}

function test_execute_async_timeout_chrome()
{
  do_test_pending();
  got_session = false;
  got_context = false;
  received = false;
  received2 = false;
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
        else if (!got_context) {
          got_context = true;
          do_check_eq('mobile', aPacket.value);
          transport.send({to: id,
                          type: "setContext",
                          value: "chrome",
                         });
        }
        else if (!received) {
            received = true;
            transport.send({to: id,
                          type: "setScriptTimeout",
                          value: "2000",
                          });
       }
       else if (!received2) {
              received2 = true;
              transport.send({to: id,
                          type: "executeJSScript",
                          value: "Marionette.is(1,1, 'should return 1');",
                          timeout: true 
                          });
       }
       else {
              do_check_eq(aPacket.from, id);
              do_check_eq(aPacket.error.status, 28);
              transport.close();
       }
      }
      transport.send({to: "root",
                      type: "getMarionetteID",
                      });
    },
    onClosed: function(aStatus) {
      do_check_eq(aStatus, 0);
      do_test_finished();
      run_next_test();
      delete transport;
    },
  };
  transport.ready();
}

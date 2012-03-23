/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test()
{
  //DebuggerServer.addActors("resource:///modules/marionette-actors.js");
  //DebuggerServer.init();

  add_test(test_execute);
  run_next_test();
}

function test_execute()
{
  //DebuggerServer.openListener(2828, true);
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
          if(aPacket.value == "3") {
            transport.send({to: id,
                          type: "executeScript",
                          value: "return 5+arguments[0];",
                          args: [1],
                          });
          }
          if(aPacket.value == "6") {
              transport.send({to: id,
                            type: "deleteSession"
                            });
            transport.close();
          }
          if(aPacket.error != undefined) {
            do_throw("Received error: " + aPacket.error);
            transport.close();
          }
        }
        else {
          received = true;
          do_check_eq('session', aPacket.value);
          transport.send({to: id,
                        type: "executeScript",
                        value: "alert('asdf'); return 2+1;",
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

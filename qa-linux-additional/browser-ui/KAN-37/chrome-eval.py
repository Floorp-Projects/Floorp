from marionette_driver.marionette import Marionette
import json,sys
m=Marionette('localhost',port=29837,socket_timeout=90);m.start_session();m.set_context('chrome');print(json.dumps(m.execute_script(sys.argv[1]),ensure_ascii=False));m.delete_session()

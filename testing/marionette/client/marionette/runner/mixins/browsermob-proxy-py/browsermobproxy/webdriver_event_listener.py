from selenium.webdriver.support.abstract_event_listener import AbstractEventListener

class WebDriverEventListener(AbstractEventListener):
    
    def __init__(self, client, refs={}):
        self.client = client
        self.hars = []
        self.refs = refs

    def before_navigate_to(self, url, driver):
        if len(self.hars) != 0:
            self.hars.append(self.client.har)
        self.client.new_har("navigate-to-%s" % url, self.refs)
       
    def before_navigate_back(self, driver=None):
        if driver:
            name = "-from-%s" % driver.current_url
        else: 
            name = "navigate-back"
        self.client.new_page(name)

    def before_navigate_forward(self, driver=None):
        if driver:
            name = "-from-%s" % driver.current_url
        else: 
            name = "navigate-forward"
        self.client.new_page(name)
                                       
    def before_click(self, element, driver):
        name = "click-element-%s" % element.id
        self.client.new_page(name)
                                                                   
    def before_quit(self, driver):
        self.hars.append(self.client.har)

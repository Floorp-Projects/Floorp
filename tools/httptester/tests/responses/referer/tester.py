class RefererTest(BaseTester):
	def verify_request(self, req):
		BaseTester.verify_request(self, req)
		if req.fname == 'iframe':
			if not req.headers.getheader('referer'):
				self.res = 0
				self.reason = "No referer header"
				return 0
		return 1

tester = RefererTest

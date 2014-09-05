#!/usr/bin/python

template = """<!DOCTYPE html>
<html>
<head>
<title>%s</title>
<link href='foo' media='%s' rel='stylesheet'>
</head>
<body>
<p>%s</p>
</body>
</html>"""

i = 1
f = open("source.txt", 'r')
for line in f:
  line = line.rstrip('\n')
  o = open("%03d.html" % i, 'wb')
  o.write(template % (line, line, line))
  o.close()
  i = i + 1
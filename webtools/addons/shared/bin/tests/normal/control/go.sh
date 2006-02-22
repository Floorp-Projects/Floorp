ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/static.html" > /root/tests/normal/control/index.250.out

sleep 5

ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/static.html" > /root/tests/normal/control/index.500.out

sleep 5

ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/static.html" > /root/tests/normal/control/index.1000.out

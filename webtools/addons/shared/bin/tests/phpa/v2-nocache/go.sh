echo "---- 250s"
echo "Index:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-nocache/public/htdocs/" > /root/tests/phpa/v2-nocache/index.250.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-nocache/public/htdocs/update.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/phpa/v2-nocache/update.250.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-nocache/public/htdocs/search.php" > /root/tests/phpa/v2-nocache/search.250.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-nocache/public/htdocs/extensions/" > /root/tests/phpa/v2-nocache/extensions.250.out
sleep 60

echo "Rss:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-nocache/public/htdocs/rss/firefox/extensions/popular" > /root/tests/phpa/v2-nocache/rss.250.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-nocache/public/htdocs/firefox/220" > /root/tests/phpa/v2-nocache/addon.250.out
sleep 60

echo "---- 500s"
echo "Index:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-nocache/public/htdocs/" > /root/tests/phpa/v2-nocache/index.500.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-nocache/public/htdocs/update.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/phpa/v2-nocache/update.500.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-nocache/public/htdocs/search.php" > /root/tests/phpa/v2-nocache/search.500.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-nocache/public/htdocs/extensions/" > /root/tests/phpa/v2-nocache/extensions.500.out
sleep 60

echo "Rss:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-nocache/public/htdocs/rss/firefox/extensions/popular" > /root/tests/phpa/v2-nocache/rss.500.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-nocache/public/htdocs/firefox/220" > /root/tests/phpa/v2-nocache/addon.500.out
sleep 60

echo "---- 1000s"
echo "Index:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-nocache/public/htdocs/" > /root/tests/phpa/v2-nocache/index.1000.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-nocache/public/htdocs/update.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/phpa/v2-nocache/update.1000.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-nocache/public/htdocs/search.php" > /root/tests/phpa/v2-nocache/search.1000.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-nocache/public/htdocs/extensions/" > /root/tests/phpa/v2-nocache/extensions.1000.out
sleep 60

echo "Rss:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-nocache/public/htdocs/rss/firefox/extensions/popular/" > /root/tests/phpa/v2-nocache/rss.1000.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-nocache/public/htdocs/firefox/220" > /root/tests/phpa/v2-nocache/addon.1000.out

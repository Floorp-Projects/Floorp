echo "---- 250s"
echo "Index:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-cache_lite/public/htdocs/" > /root/tests/normal/v2-cache_lite/index.250.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-cache_lite/public/htdocs/update.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/normal/v2-cache_lite/update.250.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-cache_lite/public/htdocs/search.php" > /root/tests/normal/v2-cache_lite/search.250.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-cache_lite/public/htdocs/extensions/" > /root/tests/normal/v2-cache_lite/extensions.250.out
sleep 60

echo "Rss:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-cache_lite/public/htdocs/rss/firefox/extensions/popular" > /root/tests/normal/v2-cache_lite/rss.250.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v2-cache_lite/public/htdocs/firefox/220" > /root/tests/normal/v2-cache_lite/addon.250.out
sleep 60

echo "---- 500s"
echo "Index:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-cache_lite/public/htdocs/" > /root/tests/normal/v2-cache_lite/index.500.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-cache_lite/public/htdocs/update.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/normal/v2-cache_lite/update.500.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-cache_lite/public/htdocs/search.php" > /root/tests/normal/v2-cache_lite/search.500.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-cache_lite/public/htdocs/extensions/" > /root/tests/normal/v2-cache_lite/extensions.500.out
sleep 60

echo "Rss:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v2-cache_lite/public/htdocs/rss/firefox/extensions/popular" > /root/tests/normal/v2-cache_lite/rss.500.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-cache_lite/public/htdocs/firefox/220" > /root/tests/normal/v2-cache_lite/addon.500.out
sleep 60

echo "---- 1000s"
echo "Index:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-cache_lite/public/htdocs/" > /root/tests/normal/v2-cache_lite/index.1000.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-cache_lite/public/htdocs/update.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/normal/v2-cache_lite/update.1000.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-cache_lite/public/htdocs/search.php" > /root/tests/normal/v2-cache_lite/search.1000.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-cache_lite/public/htdocs/extensions/" > /root/tests/normal/v2-cache_lite/extensions.1000.out
sleep 60

echo "Rss:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-cache_lite/public/htdocs/rss/firefox/extensions/popular/" > /root/tests/normal/v2-cache_lite/rss.1000.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v2-cache_lite/public/htdocs/firefox/220" > /root/tests/normal/v2-cache_lite/addon.1000.out

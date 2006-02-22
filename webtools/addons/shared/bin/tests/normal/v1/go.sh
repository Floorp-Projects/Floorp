echo "---- 250s"
echo "Index:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v1/" > /root/tests/normal/v1/index.250.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v1/update/VersionCheck.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/normal/v1/versioncheck.250.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v1/extensions/showlist.php?application=firefox" > /root/tests/normal/v1/search.250.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v1/extensions/" > /root/tests/normal/v1/extensions.250.out
sleep 60

echo "RSS:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v1/rss/?application=firefox&type=E&list=popular" > /root/tests/normal/v1/rss.250.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 250 -v 1 "http://localhost/v1/extensions/moreinfo.php?application=firefox&id=220" > /root/tests/normal/v1/addon.250.out
sleep 60

echo "---- 500s"
echo "Index:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v1/" > /root/tests/normal/v1/index.500.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v1/update/VersionCheck.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/normal/v1/versioncheck.500.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v1/extensions/showlist.php?application=firefox" > /root/tests/normal/v1/search.500.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v1/extensions/" > /root/tests/normal/v1/extensions.500.out
sleep 60

echo "RSS:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v1/rss/?application=firefox&type=E&list=popular" > /root/tests/normal/v1/rss.500.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 500 -v 1 "http://localhost/v1/extensions/moreinfo.php?application=firefox&id=220" > /root/tests/normal/v1/addon.500.out
sleep 60

echo "---- 1000s"
echo "Index:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v1/" > /root/tests/normal/v1/index.1000.out
sleep 60

echo "Update:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v1/update/VersionCheck.php?reqVersion=1&id=%7B19503e42-ca3c-4c27-b1e2-9cdb2170ee34%7D&version=0.1&appID=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D&appVersion=1.0" > /root/tests/normal/v1/versioncheck.1000.out
sleep 60

echo "Search:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v1/extensions/showlist.php?application=firefox" > /root/tests/normal/v1/search.1000.out
sleep 60

echo "Extensions:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v1/extensions/" > /root/tests/normal/v1/extensions.1000.out
sleep 60

echo "RSS:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v1/rss/?application=firefox&type=E&list=popular" > /root/tests/normal/v1/rss.1000.out
sleep 60

echo "Addon:"
ab -t 30 -n 999999 -c 1000 -v 1 "http://localhost/v1/extensions/moreinfo.php?application=firefox&id=220" > /root/tests/normal/v1/addon.1000.out

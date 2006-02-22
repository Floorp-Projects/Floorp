# normal tests
echo "normal tests ----------------"
echo "-- control --"
/root/tests/normal/control/go.sh
sleep 60
echo "-- v1 --"
/root/tests/normal/v1/go.sh
sleep 60
echo "-- v2-cache_lite --"
/root/tests/normal/v2-cache_lite/go.sh
sleep 60
echo "-- v2-nocache --"
/root/tests/normal/v2-nocache/go.sh
sleep 60
echo "-- v2-smarty --"
/root/tests/normal/v2-smarty/go.sh
sleep 60

# set up phpa
cp /root/tests/php.ini /etc/php.ini
/etc/init.d/httpd restart
sleep 20

# phpa tests
echo "phpa tests ------------------"
echo "-- control --"
/root/tests/phpa/control/go.sh
sleep 60
echo "-- v1 --"
/root/tests/phpa/v1/go.sh
sleep 60
echo "-- v2-cache_lite --"
/root/tests/phpa/v2-cache_lite/go.sh
sleep 60
echo "-- v2-nocache --"
/root/tests/phpa/v2-nocache/go.sh
sleep 60
echo "-- v2-smarty --"
/root/tests/phpa/v2-smarty/go.sh
sleep 60

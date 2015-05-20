#! /bin/bash -e

while getopts "t:i:k:s:" arg; do
  case $arg in
    t)
      TAG=$OPTARG
      ;;
    i)
      KEY_ID=$OPTARG
      ;;
    k)
      SECRET_KEY=$OPTARG
      ;;
    s)
      SOCORRO_TOKEN=$OPTARG
      ;;
  esac
done

pushd $(dirname $0)

test $TAG
test $KEY_ID
test $SECRET_KEY
test $SOCORRO_TOKEN

(echo '[default]'
echo "aws_access_key_id = $KEY_ID"
echo "aws_secret_access_key = $SECRET_KEY") > config

echo $SOCORRO_TOKEN > socorro.token

docker build -t $TAG .
rm -f config
rm -f socorro.token

popd

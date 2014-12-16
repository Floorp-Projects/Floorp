#! /bin/bash -e

while getopts "t:i:k:" arg; do
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
  esac
done

pushd $(dirname $0)

test $TAG
test $KEY_ID
test $SECRET_KEY

(echo '[default]'
echo "aws_access_key_id = $KEY_ID"
echo "aws_secret_access_key = $SECRET_KEY") > config

docker build -t $TAG .
rm -f config

popd

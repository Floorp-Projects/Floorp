#! /bin/bash -e

while getopts "t:i:k:s:" arg; do
  case $arg in
    t)
      TAG=$OPTARG
      ;;
    i)
      AWS_ACCESS_KEY_ID=$OPTARG
      ;;
    k)
      AWS_SECRET_ACCESS_KEY=$OPTARG
      ;;
    s)
      SOCORRO_TOKEN=$OPTARG
      ;;
  esac
done

pushd $(dirname $0)

test $TAG
test $AWS_ACCESS_KEY_ID
test $AWS_SECRET_ACCESS_KEY
test $SOCORRO_TOKEN

(echo '[default]'
echo "aws_access_key_id = $AWS_ACCESS_KEY_ID"
echo "aws_secret_access_key = $AWS_SECRET_ACCESS_KEY") > config

echo $SOCORRO_TOKEN > socorro.token

docker build -t $TAG .
rm -f config
rm -f socorro.token

popd

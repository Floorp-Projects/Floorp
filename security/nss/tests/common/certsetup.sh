# Generate input to certutil
certscript() {
  ca=n
  while [ $# -gt 0 ]; do
    case $1 in
      sign) echo 0 ;;
      kex) echo 2 ;;
      ca) echo 5;echo 6;ca=y ;;
    esac; shift
  done;
  echo 9
  echo n
  echo $ca
  echo
  echo n
}

# $1: name
# $2: type
# $3+: usages: sign or kex
make_cert() {
  name=$1
  type=$2

  # defaults
  type_args=()
  trust=',,'
  sign=(-x)
  sighash=(-Z SHA256)

  case $type in
    dsa) type_args=(-g 1024) ;;
    rsa) type_args=(-g 1024) ;;
    rsa2048) type_args=(-g 2048);type=rsa ;;
    rsa8192) type_args=(-g 8192);type=rsa ;;
    rsapss) type_args=(-g 1024 --pss);type=rsa ;;
    rsapss384) type_args=(-g 1024 --pss);type=rsa;sighash=(-Z SHA384) ;;
    rsapss512) type_args=(-g 2048 --pss);type=rsa;sighash=(-Z SHA512) ;;
    rsapss_noparam) type_args=(-g 2048 --pss);type=rsa;sighash=() ;;
    p256) type_args=(-q nistp256);type=ec ;;
    p384) type_args=(-q secp384r1);type=ec ;;
    p521) type_args=(-q secp521r1);type=ec ;;
    rsa_ca) type_args=(-g 1024);trust='CT,CT,CT';type=rsa ;;
    rsa_chain) type_args=(-g 1024);sign=(-c rsa_ca);type=rsa;;
    rsapss_ca) type_args=(-g 1024 --pss);trust='CT,CT,CT';type=rsa ;;
    rsapss_chain) type_args=(-g 1024);sign=(-c rsa_pss_ca);type=rsa;;
    rsa_ca_rsapss_chain) type_args=(-g 1024 --pss-sign);sign=(-c rsa_ca);type=rsa;;
    ecdh_rsa) type_args=(-q nistp256);sign=(-c rsa_ca);type=ec ;;
    delegator_p256)
        touch empty.txt
        type_args=(-q nistp256 --extGeneric 1.3.6.1.4.1.44363.44:not-critical:empty.txt)
        type=ec
        ;;
  esac
  msg="create certificate: $@"
  shift 2
  counter=$(($counter + 1))
  certscript $@ | ${BINDIR}/certutil -S \
    -z "$R_NOISE_FILE" -d "$PROFILEDIR" \
    -n $name -s "CN=$name" -t "$trust" "${sign[@]}" -m "$counter" \
    -w -2 -v 120 -k "$type" "${type_args[@]}" "${sighash[@]}" -1 -2
  html_msg $? 0 "$msg"
}

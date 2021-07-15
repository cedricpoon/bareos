#!/bin/bash

set -e
set -o pipefail
set -u

cd $(dirname "$BASH_SOURCE")
#. ../environment
. ../environment-local

# $1: method (POST, GET)
# $2: endpoint_url
# $3: string to grep for
# $4: extra curl options
# $5: repeat n times with a pause of 1 second between tries to get the result

method="$1"
shift
endpoint="$1"
shift
curl_extra_options="$@"

if [ -z "${REST_API_TOKEN:-}" ]; then
  REST_API_TOKEN=$(./curl-auth.sh)
fi

url="${REST_API_URL}/${endpoint}"

RC=0
# curl doesn't like empty string "" as option, will exit with code 3
curl_cmd="curl -w \nHTTP_CODE=%{http_code}\n -s -X ${method} $url"
OUT=$(${curl_cmd} -H "Content-Type: application/json" -H "accept: application/json" -H "Authorization: Bearer $REST_API_TOKEN" "$@") || RC=$?

printf "$OUT\n"
exit $RC

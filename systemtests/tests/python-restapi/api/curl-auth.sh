#!/bin/bash

. $(dirname "$BASH_SOURCE")/../environment-local

USERNAME=${1:-admin-tls}
PASSWORD=${2:-secret}

curl --silent -X POST "${REST_API_URL}/token" -H  "accept: application/json" -H  "Content-Type: application/x-www-form-urlencoded" -d "username=admin-tls&password=secret" | grep access_token | cut -d '"' -f 4

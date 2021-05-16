#!/bin/bash

export PYTHONUNBUFFERED=on
submission_dir="$(pwd)"
assignment_dir="${submission_dir}/assignment3"
timeout="10s"
ip_regex='([0-9]{1,3}\.){3}[0-9]{1,3}'
domain_regex='^(\.|([a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+)$'
output=$(mktemp)

function clean_up {
    rm "${output}"
}

trap clean_up EXIT

function test_case {
    address="$1"
    shift
    domain="$1"
    shift

    docker run --platform linux/amd64 --rm -v "${assignment_dir}:/opt" -w /opt baseline timeout -s 9 "${timeout}" /opt/resolve -a "${address}" -d "${domain}" > "${output}"
	
    local status=0

    while read -a line
    do
        echo "${line[@]}"
        if [[ ! ${line[0]} =~ ${ip_regex} ]]; then
            echo "Invalid NS IP: '${line[0]}' doesn't match '${ip_regex}'" >&2
            status=1
        fi
        if [[ ! "${line[1]}" =~ ${domain_regex} ]]; then
            echo "Invalid RR Key: '${line[1]}' doesn't match '${domain_regex}'" >&2
            status=1
        fi
        if [[ "${line[2]}" == "CNAME" ]]; then
            if [[ ! "${line[3]}" =~ ${domain_regex} ]]; then
                echo "Invalid CNAME Domain: '${line[3]}' doesn't match '${domain_regex}'" >&2
                status=1
            fi
        elif [[ "${line[2]}" == "A" ]]; then
            if [[ ! "${line[3]}" =~ ${ip_regex} ]]; then
                echo "Invalid A IP: '${line[3]}' doesn't match '${ip_regex}'" >&2
                status=1
            fi
        elif [[ "${line[2]}" == "NS" ]]; then
            if [[ ! "${line[3]}" =~ ${domain_regex} ]]; then
                echo "Invalid NS Domain: '${line[3]}' doesn't match '${domain_regex}'" >&2
                status=1
            fi
        else
            echo "Invalid RR Type: '${line[2]}'" >&2
            status=1
        fi
    done < "${output}"

    if [[ 0 -ne ${status} ]]; then
        echo "ERROR: Failed test case" >&2
        exit 1
    else
        echo "INFO: Passed test case" >&2
    fi
}

if [[ -f "${assignment_dir}/makefile" || -f "${assignment_dir}/Makefile" ]]; then
    docker run --platform linux/amd64 --rm -v "${assignment_dir}:/opt" -w /opt baseline make clean
    docker run --platform linux/amd64 --rm -v "${assignment_dir}:/opt" -w /opt baseline make
    if [ 0 -ne $? ]; then
        echo "ERROR: Make failed" >&2
        exit 1
    fi
    echo "INFO: Built correctly with make" >&2
else
    echo "WARNING: No makefile found" >&2
fi


if [[ ! -x "${assignment_dir}/resolve" ]]; then
    echo "ERROR: No executable found at '${assignment_dir}/resolve'" >&2
    exit 1
fi

test_case "199.7.91.13" "www.google.com"

git status -u | grep -E '\.(c|h|py|java|erl)$'
if [[ 0 -eq $? ]]; then
    echo "WARNING: Uncommitted source code found, Add/commit these or remove them" >&2
fi

echo "INFO: Test passed" >&2

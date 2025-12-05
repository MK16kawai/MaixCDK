#!/bin/bash

set -e
set -x

platform=$1
need_run=$2
start_from="$3"
if [ "${need_run}x" == "1x" ]; then
    run_cmd="maixcdk run"
else
    run_cmd=""
fi

blacklist=("maixcdk-example")
start_test_flag=false
if [ "$start_from" == "" ]; then
    start_test_flag=true
fi
function test_script()
{
    set +x
    echo ""
    echo "--------------------------------"
    echo "-- Test [$1] Start --"
    echo "--------------------------------"
    set -x
    cd $1
    maixcdk distclean
    maixcdk build -p $platform
    $run_cmd
    cd ..
    set +x
    echo "--------------------------------"
    echo "-- Test [$1] End --"
    echo "--------------------------------"
    echo ""
    set -x
}

function test_start()
{
    test_script $1
    if [ $? -ne 0 ]; then
        echo "Error: $1 failed to execute."
        exit 1
    fi
}


cd ../../examples

for dir in */; do
  if [ -d "$dir" ]; then
    # 检查是否在黑名单
    skip=false
    for b in "${blacklist[@]}"; do
    if [[ "$dir" == "$b/" ]]; then
        echo "skip $dir for $platform"
        skip=true
        break
    fi
    done

    if $skip; then
        continue
    fi

    # 检查是否开始测试
    if [ -n $start_test_flag ]; then
        if [[ "${dir}" == "${start_from}/" ]]; then
            start_test_flag=true
        fi
    fi

    if ! $start_test_flag; then
        echo "skip $dir, wait ${start_from}"
        continue
    fi

    test_start "${dir%/}"
  fi
done





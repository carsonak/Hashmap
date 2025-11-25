#!/usr/bin/env bash

# Fail fast
set -e

function print_help {
    cat <<EOF
USAGE
  $0 [OPTIONS] [SUITE]

DESCRIPTION
    Build and run HashMap benchmarks under different tools. SUITE is one of:
    cachegrind, gprofng, no-tool or timing and defaults to timing if not given.

OPTIONS
    -h, --help
        print this help message and exit.
    -cNAME, -c NAME, --config=NAME
        cmake presets configuration to use.
EOF
}

function build {
    config=$1
    cmake --preset "$config" --fresh
    cmake --build --preset "$config" -j 4
}

function run {
    if [[ $# -lt 2 ]]
    then echo "$0:$LINENO: invalid number of arguments" >&2
        return 1
    fi

    suite=$1
    shift

    result_dir="data/$suite"
    if [[ $suite = "timing" ]]
    then rm -f -vrd "$result_dir/password_chars" "$result_dir/random_data" "$result_dir/source_code"
        mkdir -vp "$result_dir/password_chars" "$result_dir/random_data" "$result_dir/source_code"
    else rm -f -vrd "$result_dir"
        mkdir -vp "$result_dir"
    fi

    for exe in $@
    do  base=$(basename "$exe")
        case $suite in
        cachegrind)
            operations=80000
            initial_capacity=1021
            sh -xc "valgrind -q --tool=callgrind --cache-sim=yes --branch-sim=yes \
                --compress-strings=no --callgrind-out-file=$result_dir/${base}.out \
                $exe -n $operations -c $initial_capacity"
            ;;
        gprofng)
            sh -xc "gprofng collect app -p hi -O $result_dir/${base}.er $exe"
            ;;
        no-tool)
            sh -xc "./$exe"
            ;;
        timing)
            sh -xc "./$exe benchmarks/10mb-password_chars > $result_dir/password_chars/$base.txt"
            sh -xc "./$exe benchmarks/10mb-random_data > $result_dir/random_data/$base.txt"
            sh -xc "./$exe benchmarks/10mb-source_code > $result_dir/source_code/$base.txt"
            ;;
        *)
            echo "$0:$LINENO: unknown suite '$suite'" >&2
            return 1
            ;;
        esac
    done
}

CONFIG=release
while [[ $# -gt 0 ]]
do case "$1" in
    -c*)
        if [[ $1 = -c ]]
        then shift
            CONFIG="$1"
        else CONFIG="${1#-c}"
        fi
        ;;
    --config=*)
        CONFIG="${1#--config=}"
        ;;
    -h|--help)
        print_help
        exit 0
        ;;
    --*|-*)
        echo "Unknown option: $1" >&2
        exit 1
        ;;
    *)
        break
        ;;
    esac

    shift
done

SUITE="${1:-timing}"
if [[ $SUITE = "gprofng" ]] || [[ $SUITE = "cachegrind" ]]
then export CFLAGS="${CFLAGS:-} -g"
fi

case $SUITE in
cachegrind|gprofng|no-tool)
    build "$CONFIG"
    run "$SUITE" "$(find "$CONFIG/benchmarks" -maxdepth 1 -name "benchmark_*")"
    ;;
timing)
    build "$CONFIG"
    run timing "$(find "$CONFIG/benchmarks" -maxdepth 1 -name "time_*")"
    ;;
*)
    echo "$0:$LINENO: unknown argument $SUITE" >&2
    exit 1
    ;;
esac

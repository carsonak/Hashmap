#!/usr/bin/env bash
# Build and run benchmarks for the HashMap under different tools.
# USAGE: ./profile.sh [--cachegrind|--gprofng|--no-tool]

# Fail fast
set -e

function run_benchmarks {
    if [[ $# -lt 2 ]]
    then echo "$0:$LINENO: invalid number of arguments" >&2
        return 1
    fi

    suite=$1
    shift

    result_dir="data/$suite"
    if [[ $suite = "timing" ]]
    then mkdir -vp "$result_dir/password_chars" "$result_dir/random_data" "$result_dir/source_code"
    else mkdir -vp "$result_dir"
    fi

    for exe in $@
    do  base=$(basename "$exe")
        case $suite in
        cachegrind)
            set -x
            valgrind -q --tool=callgrind --cache-sim=yes --branch-sim=yes \
                --compress-strings=no --callgrind-out-file="$result_dir/${base}.out" \
                "$exe" -n 80000 -c 1021
            ;;
        gprofng)
            set -x
            gprofng collect app -p hi -O "$result_dir/${base}.er" "$exe"
            ;;
        no_tool)
            set -x
            "./$exe"
            ;;
        timing)
            set -x
            "./$exe" benchmarks/10mb-password_chars > "$result_dir/password_chars/$base.txt"
            "./$exe" benchmarks/10mb-random_data > "$result_dir/random_data/$base.txt"
            "./$exe" benchmarks/10mb-source_code > "$result_dir/source_code/$base.txt"
            ;;
        *)
            echo "$0:$LINENO: unknown suite '$suite'" >&2
            return 1
            ;;
        esac

        set +x
    done
}

CONFIG=release
if [[ $1 = "--gprofng" ]] || [[ $1 = "--cachegrind" ]]
then export CFLAGS="${CFLAGS:-} -g"
fi

cmake --preset "$CONFIG" --fresh
cmake --build --preset "$CONFIG" -j 4

case $1 in
--cachegrind)
    run_benchmarks cachegrind "$(find "$CONFIG"/benchmarks -maxdepth 1 -name "benchmark_*")"
    ;;
--gprofng)
    run_benchmarks gprofng "$(find "$CONFIG"/benchmarks -maxdepth 1 -name "benchmark_*")"
    ;;
--no-tool)
    run_benchmarks no_tool "$(find "$CONFIG"/benchmarks -maxdepth 1 -name "benchmark_*")"
    ;;
*)
    run_benchmarks timing "$(find "$CONFIG"/benchmarks -maxdepth 1 -name "time_*")"
    ;;
esac

#!/usr/bin/env bash

set -e # Fail fast

function run_timings {
    [[ -n $1 ]] || return 1

    result_dir=$1
    shift
    mkdir -vp "$result_dir/password_chars" "$result_dir/random_data" "$result_dir/source_code"

    for exe in $@
    do  base=$(basename "$exe")
        set -x
        "./$exe" benchmarks/10mb-password_chars > "$result_dir/password_chars/$base.txt"
        "./$exe" benchmarks/10mb-random_data > "$result_dir/random_data/$base.txt"
        "./$exe" benchmarks/10mb-source_code > "$result_dir/source_code/$base.txt"
        set +x
    done
}

function run_benchmarks {
    [[ -n $1 ]] || return 1

    result_dir=$1
    shift
    mkdir -vp "$result_dir"

    for exe in $@
    do  base=$(basename "$exe")
        set -x
        gprofng collect app -p hi -O "$result_dir/${base}.er" "$exe"

        # valgrind --tool=callgrind --cache-sim=yes --branch-sim=yes --compress-strings=no \
        #     --callgrind-out-file="$result_dir/${base}_callgrind.out" "$exe"
        set +x
    done
}

CONFIG=release-debug

cmake --preset "$CONFIG"
cmake --build --preset "$CONFIG" -j 4
rm -rd data

if [[ $1 = "-b" ]]
then
    run_benchmarks data/benchmarks "$(find "$CONFIG"/benchmarks -maxdepth 1 -name "benchmark_*")"
else
    run_timings data/timings "$(find "$CONFIG"/benchmarks -maxdepth 1 -name "time_*")"
fi

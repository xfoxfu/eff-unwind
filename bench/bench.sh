set -x

for case in $(fd -e kk --format '{/.}'); do
# for case in resume_nontail; do
    if [[ "$case" = "test_exception" ]]; then
        continue
    fi
    case $case in
        countdown)           SIZE=200000000;;
        fibonacci_recursive) SIZE=42;;
        generator)           SIZE=25;;
        handler_sieve)       SIZE=60000;; # SIZE=60000;;
        iterator)            SIZE=40000000;;
        nqueens)             SIZE=12;;
        parsing_dollars)     SIZE=20000;;
        product_early)       SIZE=100000;;
        resume_nontail)      SIZE=2000;; # SIZE=20000;;
        tree_explore)        SIZE=16;;
        triples)             SIZE=100;; # SIZE=300;;
    esac
    hyperfine -r 10 --export-json $case.json -N "../build/$case $SIZE" "./koka/bin/$case-kk $SIZE" "./cpp-effects/bin/$case $SIZE"  "./ocaml/bin/$case $SIZE"
    # python plot.py $case.json --labels eff-unwind,koka,cpp-effects -o $case.png
    # echo "Running $case"
    # echo $(./$case-kk $SIZE)
done

hyperfine -r 10 --export-json test_exception.json -N \
    "../build/test_exception" \
    "./koka/bin/test_exception-kk" \
    "./cpp-effects/bin/test_exception" \
    "./vanilla-cpp/bin/exception"

# hyperfine -r 10 --export-json test_exception2.json -N \
#     "../build/test_exception2" \
#     "./koka/bin/test_exception2-kk" \
#     "./cpp-effects/bin/test_exception2" \
#     "./vanilla-cpp/bin/exception2"

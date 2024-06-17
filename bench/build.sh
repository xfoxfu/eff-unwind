fd -e kk -x koka -O2 -c '{}' -o '{.}-kk'
cd ../build && ninja && cd ../bench

for case in $(fd -e kk --format '{/.}'); do
    case $case in
        countdown)           SIZE=200000000;;
        fibonacci_recursive) SIZE=42;;
        product_early)       SIZE=100000;;
        iterator)            SIZE=40000000;;
        nqueens)             SIZE=12;;
        generator)           SIZE=25;;
        tree_explore)        SIZE=16;;
        triples)             SIZE=300;;
        parsing_dollars)     SIZE=20000;;
        resume_nontail)      SIZE=20000;;
        handler_sieve)       SIZE=2000;; # SIZE=60000;;
    esac
    chmod +x ./$case-kk
    hyperfine --export-json $case.json -N "../build/$case $SIZE" "./$case-kk $SIZE"
    python plot.py $case.json --labels eff-unwind,koka,cpp-effects -o $case.png
done

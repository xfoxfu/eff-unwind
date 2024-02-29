fd -e kk -x koka -O2 -c '{}' -o '{.}-kk'
cd ../build && ninja && cd ../bench

for case in counter counter10 exception noexception mstate; do
    chmod +x ./$case-kk
    hyperfine --export-json $case.json -w 10 -N ../build/test_$case ./$case-kk ../build/test_eff_$case
    python ../plot.py $case.json --labels eff-unwind,koka,cpp-effects -o $case.png
done

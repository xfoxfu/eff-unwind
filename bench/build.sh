set -x

rm -rf koka/bin/*
rm -rf cpp-effects/bin/*
rm -rf ocaml/bin/*

export PATH="$HOME/.local/bin:$PATH"
fd -e kk -x koka -O2 -c '{}' -o 'koka/bin/{/.}-kk'
fd -e kk -x chmod +x 'koka/bin/{/.}-kk'
cd ../build && ninja && cd ../bench

# download and extract cpp-effects
# https://github.com/maciejpirog/cpp-effects/archive/refs/heads/main.zip
fd -e cpp -p cpp-effects -x clang++ 'cpp-effects/src/{/}' -std=c++17 -O2 \
  -o 'cpp-effects/bin/{/.}' \
  -Icpp-effects/include \
  -I/opt/homebrew/opt/boost/include \
  -L/opt/homebrew/opt/boost/lib \
  -lboost_fiber \
  -lboost_context \
  -lboost_coroutine

#  -I/opt/homebrew/Cellar/fmt/11.0.2/include -L/opt/homebrew/Cellar/fmt/11.0.2/lib -lfmt
clang++ vanilla-cpp/src/exception.cpp -std=c++17 -O2 -o vanilla-cpp/bin/exception

fd -e ml -p ocaml -x ocamlopt -O3 -o 'ocaml/bin/{/.}' 'ocaml/src/{/}'

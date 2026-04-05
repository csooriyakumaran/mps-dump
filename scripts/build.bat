cmake -S . -B build -G "Ninja" -D CMAKE_C_COMPILER=clang -D CMAKE_BUILD_TYPE=Release -D CMAKE_C_FLAGS_RELEASE="-O3 -g0 -DNDEBUG"
cmake --build build

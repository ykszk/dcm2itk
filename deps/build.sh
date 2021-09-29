mkdir -p build
cd build
cmake ..
cmake --build . --config Release --parallel 4
cmake -DZLIB_INCLUDE_DIR="$(pwd)/zlib-ng;$(pwd)/../zlib-ng" .. 
cmake --build . --config Release --parallel 4

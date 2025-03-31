mkdir -p build
cd build
cmake ..
cmake --build . --config Release --parallel
cmake -DZLIB_INCLUDE_DIR="$(pwd)/zlib-ng;$(pwd)/../zlib-ng" .. 
cmake --build . --config Release --parallel

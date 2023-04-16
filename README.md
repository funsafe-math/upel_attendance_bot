[![CMake](https://github.com/tadeuszk733/upel_attendance_bot/actions/workflows/cmake.yml/badge.svg)](https://github.com/tadeuszk733/upel_attendance_bot/actions/workflows/cmake.yml) 
[![CodeQL](https://github.com/tadeuszk733/upel_attendance_bot/actions/workflows/codeql.yml/badge.svg)](https://github.com/tadeuszk733/upel_attendance_bot/actions/workflows/codeql.yml)
# UPEL attendance bot
The purpose of this project is to automatically register attendance on UPEL university platform.

## Build
```bash
mkdir build
pushd build
cmake .. # -DBUILD_SHARED_LIBS:BOOL=OFF
cmake --build . -j$(nproc)
popd
```

## Usage
```bash
./build/upel_bot
```


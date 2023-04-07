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


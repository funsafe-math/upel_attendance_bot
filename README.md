# UPEL attendance bot
The purpose of this project is to automatically register attendance on UPEL university platform.

## Build
```bash
mkdir build
pushd build
cmake .. # Might take a long time
cmake --build . -j$(nproc)
popd
```

## Usage
```bash
./build/upel_bot
```


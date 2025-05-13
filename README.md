# daemon-screen

## Build Go:
```
CGO_CFLAGS="-I/usr/include/libdrm -I./src" \ CGO_LDFLAGS="-ldrm" go build -o my_program
```

## Build C:
```
make // for build executable file

make clean // for clean build 
```
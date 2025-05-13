# daemon-screen (in further will not a daemon ^_^)

The functionality was based on [GPU Screen Recorder](https://git.dec05eba.com/gpu-screen-recorder/about/) ([flathub link](https://flathub.org/apps/com.dec05eba.gpu_screen_recorder))

## Build Go:
```
CGO_CFLAGS="-I/usr/include/libdrm -I./src" CGO_LDFLAGS="-ldrm" go build -o my_program
```

## Build C:
```
make // for build executable file

make clean // for clean build 
```



## Dependencies
**Install on Debian like distrs**: 
```
sudo apt install libdrm-dev gcc make build-essential
```
If you want use Go also you need installed Go. [How to do this](https://go.dev/doc/install)
### Important points
User should be in `video` group:

- How to check:
```
groups
```
- How to add:
```
sudo usermod -aG video "username"
```

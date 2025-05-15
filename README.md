# daemon-screen (in further will not a daemon ^_^)

The functionality was based on [GPU Screen Recorder](https://git.dec05eba.com/gpu-screen-recorder/about/) ([flathub link](https://flathub.org/apps/com.dec05eba.gpu_screen_recorder))

## Build Go:
```shell
CGO_CFLAGS="-I/usr/include/libdrm -I./src" CGO_LDFLAGS="-ldrm" go build -o my_program
```

## Build C:
```shell
make # for build executable file

make clean # for clean build 
```

## Dependencies
**Install on Debian like distrs**: 
```shell
sudo apt install libdrm-dev gcc make build-essential
```
If you want use Go also you need installed Go. [How to do this](https://go.dev/doc/install)

### Some stuff:
Need OpenGL for create image:
```shell
sudo apt install libgl1-mesa-dev libglu1-mesa-dev freeglut3-dev mesa-vulkan-drivers mesa-utils
```
Check version of OpenGL:
```shell
glxinfo | grep "OpenGL version"
```
Test:
```shell
glxgears
```


### Important points
User should be in `video` group:

- How to check:
```shell
groups
```
- How to add:
```shell
sudo usermod -aG video "username"
```

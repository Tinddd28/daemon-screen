🌐 **English** · [Русский](README.ru.md)

# daemon-screen

**Proof of Concept: rendering a DRM/KMS dma-buf framebuffer through EGL.**

This is not a finished product. It is a PoC that demonstrates one specific
thing end-to-end: taking a **dma-buf** framebuffer, importing it into a headless
**EGL/GBM** context, letting the GPU detile/convert it, reading the pixels back
and writing them to PNG. The KMS screen-capture path is built on top of that
core, but the point of the project is the **dma-buf → EGL → pixels** interaction
itself.

Originally explored as part of a diploma project focused on DRM/KMS internals;
the capture idea is modeled on
[GPU Screen Recorder](https://git.dec05eba.com/gpu-screen-recorder/about/)
([flathub](https://flathub.org/apps/com.dec05eba.gpu_screen_recorder)).

## The PoC pipeline

```
dma-buf fd  ──▶  EGLImage (EGL_LINUX_DMA_BUF_EXT, with modifiers)
            ──▶  external OES texture
            ──▶  render into an RGBA8 FBO   (GPU detile + format convert)
            ──▶  glReadPixels
            ──▶  PNG (libpng)
```

Runs headless on a render node, so it needs **no DRM master and no window
system**. The same code path serves two entry points:

- `ds-selftest` — feeds a **synthetic** dma-buf (a GBM buffer painted with a
  known pattern) through the pipeline. Proves the whole flow works **without
  root and without a display**.
- `ds-capture` — feeds **real** KMS framebuffers (the compositor's scanned-out
  planes) through the same pipeline to produce a screenshot.

## Build / run

Extra deps for the PoC: `libepoxy`, `mesa`/`libgbm`, `libpng`, `libjpeg`
(Arch: `sudo pacman -S libepoxy libpng libjpeg-turbo mesa`).

Both tools take `-f=png` (default, lossless) or `-f=jpg` (lossy, alpha dropped)
to pick the output format.

### Self-test — validate the dma-buf/EGL pipeline (no root, no display)

Round-trips a known pattern (top red / bottom blue) through the exact dma-buf
import path and checks the read-back pixels. Writes `selftest.png` to the
current directory (or to the path given as an argument).

```
make selftest
./ds-selftest              # -> selftest.png
./ds-selftest out.png      # custom path
./ds-selftest -f=jpg       # -> selftest.jpg
```

### Screen capture — the same pipeline on real framebuffers (needs root)

```
make capture
sudo ./ds-capture screenshot          # -> screenshot-<N>[-cursor].png per plane
sudo ./ds-capture -f=jpg screenshot   # -> screenshot-<N>[-cursor].jpg
```

**Why root:** `drmModeGetFB2` zeroes the GEM handles of other clients'
framebuffers unless the caller is the DRM master or has `CAP_SYS_ADMIN`. A
Wayland compositor holds the master and won't give it up, so capturing another
client's framebuffer requires privilege. (In a bare TTY with no compositor you
become master on `open()`, which is why capture "worked" only there before.)

### Legacy scanner (KMS enumeration only)

```
make        # builds drm_test from src/main.c + src/mydrm.c + src/kms.c
```

## Documentation

Technical notes live in **[`docs/`](docs/README.md)** — the end-to-end pipeline,
the DRM/KMS and EGL primers, design choices (what's swappable at each step), the
original bugs, and the correct architecture. Start at
[`docs/README.md`](docs/README.md).

## Layout

| File | Role |
|------|------|
| `src/egl_capture.c` / `include/egl_capture.h` | **the PoC core** — dma-buf → EGL → RGBA → PNG |
| `src/selftest.c` | synthetic dma-buf driver (`ds-selftest`) |
| `src/capture_main.c` | real KMS-capture driver (`ds-capture`) |
| `src/kms.c` / `include/kms.h` | KMS plane enumeration, framebuffer → dma-buf export |
| `src/mydrm.c`, `src/drmcheck.c` | early DRM/KMS experiments (not on the PoC path) |

## Dependencies (base DRM tooling)

**Debian-like:**
```
sudo apt install libdrm-dev gcc make build-essential
# for the PoC pipeline: libepoxy-dev libgbm-dev libpng-dev
```

**Note on the `video` group:** on a live session logind grants device access via
ACLs, but `ds-capture` needs root anyway (see above), so root already covers it.
```
groups                              # check
sudo usermod -aG video "username"   # add
```

## License

**GNU General Public License v3.0 or later** — see [LICENSE](LICENSE).
Copyright © 2026 Shibakov Nikita &lt;tind.nik.28@gmail.com&gt;.

GPL is a deliberate choice: any derivative work must also stay free and
open-source under the GPL, so this code cannot be absorbed into a closed-source
product (e.g. a proprietary DLP tool). Note that copyleft only guarantees the
*source stays open* — by design it does not restrict the field of use.

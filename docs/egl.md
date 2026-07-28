# EGL: что это и как с ним работать

Вводный разбор второй половины проекта. DRM/KMS дают **буфер** (dma-buf) —
см. [drm-kms.md](drm-kms.md); EGL с OpenGL ES превращают его в **читаемые
пиксели**. Как оно складывается в конвейер — [pipeline.md](pipeline.md).

---

## 1. Что такое EGL

**EGL — это «клей» между API рендеринга (OpenGL / OpenGL ES) и платформой**
(оконной системой или, как у нас, её отсутствием).

Сам OpenGL ES умеет только рисовать — он ничего не знает про окна, дисплеи,
выделение и показ буферов. Всю платформенную интеграцию берёт на себя EGL:

- создать **контекст** рендеринга;
- договориться о **формате** буферов (сколько бит на канал и т.п.);
- дать **поверхность** для рисования (окно / offscreen / ничего);
- **связать** результат с платформой (показать в окне, экспортировать буфер).

Аналоги для сравнения: **GLX** — тот же клей для OpenGL под X11, **WGL** — под
Windows. EGL — переносимый, работает под X11, Wayland, Android и **headless**
(через GBM/DRM).

> Важно: EGL **не рисует**. Рисует OpenGL ES. EGL только подключает GL к
> платформе и управляет ресурсами вокруг него.

---

## 2. Ключевые объекты EGL

```
EGLDisplay ──▶ EGLConfig ──▶ EGLContext ──▶ (EGLSurface | surfaceless)
(подключение)  (формат)      (состояние GL)   (куда рисуем)
```

- **EGLDisplay** — подключение к рендер-подсистеме. Получается **для конкретной
  платформы**: X11, Wayland, **GBM** (headless на DRM), surfaceless/device.
- **EGLConfig** — описание формата пикселей (глубина R/G/B/A, наличие depth/
  stencil, тип поверхности). Выбирается `eglChooseConfig` под требования.
- **EGLContext** — экземпляр состояния OpenGL ES (текстуры, шейдеры, привязки).
  Рисование идёт «в текущий контекст».
- **EGLSurface** — цель вывода, привязанная к платформе: окно (`WindowSurface`)
  или offscreen (`PbufferSurface`). Может **отсутствовать** — см. surfaceless.

Жизненный цикл: `eglGetPlatformDisplay` → `eglInitialize` → `eglBindAPI` →
`eglChooseConfig` → `eglCreateContext` → `eglMakeCurrent` → рисуем → teardown.

---

## 3. Платформы и headless: причём тут GBM

EGLDisplay берётся под платформу через `eglGetPlatformDisplay(EGL_PLATFORM_*)`.
Для «без оконной системы» платформа — **GBM**.

**GBM (Generic Buffer Management)** — библиотека, которая на DRM-устройстве
аллоцирует/оборачивает буферы так, чтобы их **понимал и GPU (для рендера через
EGL/GL), и KMS (для вывода на экран)**. Это мост между «GL хочет во что-то
рисовать» и «DRM владеет памятью GPU».

В проекте:

```c
int fd = open("/dev/dri/renderD128", ...);        // render-нода, без прав
struct gbm_device *gbm = gbm_create_device(fd);
EGLDisplay dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm, NULL);
```

Render-нода + GBM = **headless GPU-контекст без DRM master и без окна** — ровно
то, что нужно, чтобы «распаковать» чужой кадр, ничего не выводя.

### Surfaceless

Нам не нужна ни экранная, ни offscreen-поверхность EGL — мы рисуем в **FBO**
(framebuffer object самого OpenGL). Поэтому:

```c
eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx); // surfaceless
```

(расширение `EGL_KHR_surfaceless_context`, поддержано Mesa).

---

## 4. Расширения EGL — где происходит магия импорта

Базовый EGL про окна. Возможность **импортировать чужой dma-buf** дают
расширения:

- **`EGL_KHR_image_base`** — вводит **`EGLImage`**: дескриптор изображения,
  разделяемый между EGL/GL/другими API. Это «контейнер», в который можно
  завернуть внешний буфер.
- **`EGL_EXT_image_dma_buf_import`** (+ `..._modifiers`) — позволяет создать
  `EGLImage` **из dma-buf fd**, передав раскладку (fourcc, fd/offset/pitch на
  каждую плоскость и **modifier** для тайлинга):

  ```c
  EGLImage img = eglCreateImage(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
  ```
- **`GL_OES_EGL_image_external`** (со стороны GL ES) — привязать `EGLImage` к
  специальной **внешней** текстуре `GL_TEXTURE_EXTERNAL_OES`:

  ```c
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, img);
  ```

  Внешняя текстура нужна потому, что источник может быть тайленым/не-RGB (наш
  случай — XR30 + AMD DCC): GPU при сэмплинге сам делает detile и конвертацию.
  В шейдере это `samplerExternalOES` (+ `#extension GL_OES_EGL_image_external`).

---

## 5. Как EGL используется в проекте (`src/egl_capture.c`)

```
ds_egl_create():
  renderD128 → gbm_create_device → eglGetPlatformDisplayEXT(GBM)
  eglInitialize → проверка EGL_EXT_image_dma_buf_import
  eglBindAPI(GLES) → eglChooseConfig → eglCreateContext
  eglMakeCurrent(surfaceless) → компиляция шейдеров (quad + samplerExternalOES)

ds_egl_capture_dmabuf(dmabuf):
  eglCreateImage(EGL_LINUX_DMA_BUF_EXT, {fourcc, fd, offset, pitch, modifier})
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, img)   ← внешняя текстура
  рендер quad'а в RGBA8 FBO                                    ← GPU: detile + convert
  glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE)                      ← линейные пиксели
```

Результат — обычный линейный RGBA8, который уже пишется в PNG (libpng).

---

## 6. Как это соотносится с OpenGL и DRM

Три слоя, каждый на своём месте:

| Слой | Кто | За что отвечает |
|---|---|---|
| **DRM/KMS** | ядро | владеет памятью GPU и дисплеем; отдаёт буфер как **dma-buf** |
| **EGL (+GBM)** | userspace | подключает GL к платформе; **импортирует** dma-buf как `EGLImage` |
| **OpenGL ES** | userspace | собственно **рисует/сэмплит**: detile, конвертация, readback |

Коротко: **DRM даёт буфер → EGL заворачивает его в текстуру → GL достаёт
пиксели.**

---

## Куда дальше

- Откуда берётся сам dma-buf → [drm-kms.md](drm-kms.md).
- Полный путь end-to-end и разбор шагов → [pipeline.md](pipeline.md).
- Почему headless/без прав и как масштабировать до демона →
  [correct-approach.md](correct-approach.md).

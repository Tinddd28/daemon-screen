[English](README.md) · 🌐 **Русский**

# daemon-screen

**Proof of Concept: отрисовка dma-buf фреймбуфера DRM/KMS через EGL.**

Это не законченный продукт, а PoC, который демонстрирует одну конкретную вещь
end-to-end: взять **dma-buf** фреймбуфер, импортировать его в headless-контекст
**EGL/GBM**, дать GPU выполнить detile/конвертацию формата, прочитать пиксели
обратно и сохранить их в PNG. Путь захвата экрана через KMS построен поверх
этого ядра, но смысл проекта — именно во взаимодействии
**dma-buf → EGL → пиксели**.

Изначально исследовалось в рамках дипломной работы по внутреннему устройству
DRM/KMS; идея захвата смоделирована по
[GPU Screen Recorder](https://git.dec05eba.com/gpu-screen-recorder/about/)
([flathub](https://flathub.org/apps/com.dec05eba.gpu_screen_recorder)).

## Конвейер PoC

```
dma-buf fd  ──▶  EGLImage (EGL_LINUX_DMA_BUF_EXT, с модификаторами)
            ──▶  external OES-текстура
            ──▶  рендер в RGBA8 FBO        (detile + конвертация формата на GPU)
            ──▶  glReadPixels
            ──▶  PNG (libpng)
```

Работает headless на render node, поэтому **не нужен ни DRM master, ни оконная
система**. Один и тот же путь обслуживает два входа:

- `ds-selftest` — прогоняет через конвейер **синтетический** dma-buf (буфер GBM,
  залитый известным паттерном). Доказывает работоспособность всего flow **без
  root и без дисплея**.
- `ds-capture` — прогоняет через тот же конвейер **реальные** фреймбуферы KMS
  (сканируемые композитором плоскости) и получает скриншот.

## Сборка / запуск

Дополнительные зависимости PoC: `libepoxy`, `mesa`/`libgbm`, `libpng`
(Arch: `sudo pacman -S libepoxy libpng mesa`).

### Self-test — проверка конвейера dma-buf/EGL (без root, без дисплея)

Прогоняет известный паттерн (верх красный / низ синий) через тот же путь импорта
dma-buf и проверяет считанные обратно пиксели. Пишет `selftest.png` в текущую
директорию (или в путь, переданный первым аргументом).

```
make selftest
./ds-selftest              # -> selftest.png
./ds-selftest out.png      # свой путь
```

### Захват экрана — тот же конвейер на реальных фреймбуферах (нужен root)

```
make capture
sudo ./ds-capture screenshot   # -> screenshot-<N>[-cursor].png на каждую плоскость
```

**Почему root:** `drmModeGetFB2` обнуляет GEM-хендлы фреймбуферов чужих клиентов,
если вызывающий не является DRM master и не имеет `CAP_SYS_ADMIN`. Wayland-
композитор держит master и не отдаёт его, поэтому чтение чужого фреймбуфера
требует привилегий. (В голом TTY без композитора вы становитесь master при
`open()` — потому раньше захват «работал» только там.)

### Legacy-сканер (только перечисление KMS)

```
make        # собирает drm_test из src/main.c + src/mydrm.c + src/kms.c
```

## Структура

| Файл | Роль |
|------|------|
| `src/egl_capture.c` / `include/egl_capture.h` | **ядро PoC** — dma-buf → EGL → RGBA → PNG |
| `src/selftest.c` | драйвер синтетического dma-buf (`ds-selftest`) |
| `src/capture_main.c` | драйвер реального KMS-захвата (`ds-capture`) |
| `src/kms.c` / `include/kms.h` | перечисление плоскостей KMS, экспорт framebuffer → dma-buf |
| `src/mydrm.c`, `src/drmcheck.c` | ранние эксперименты с DRM/KMS (вне пути PoC) |

## Зависимости (базовый инструментарий DRM)

**Debian-подобные:**
```
sudo apt install libdrm-dev gcc make build-essential
# для конвейера PoC: libepoxy-dev libgbm-dev libpng-dev
```

**Про группу `video`:** в активной сессии logind выдаёт доступ к устройствам
через ACL, но `ds-capture` всё равно требует root (см. выше), так что root это
уже покрывает.
```
groups                              # проверить
sudo usermod -aG video "username"   # добавить
```

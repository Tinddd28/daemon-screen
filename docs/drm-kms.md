# DRM и KMS: что это и как с ними работать

Вводный разбор подсистемы Linux, на которой держится весь проект. Как эти понятия
складываются в рабочий конвейер — см. [pipeline.md](pipeline.md).

---

## 1. Что такое DRM

**DRM (Direct Rendering Manager)** — подсистема ядра Linux (`drivers/gpu/drm`),
которая управляет видеокартой. Она закрывает четыре задачи:

1. **Арбитраж доступа** к GPU между процессами (кто сейчас управляет выводом).
2. **Управление памятью** GPU (буферы — через GEM, см. §6).
3. **Настройка вывода на экран** — это и есть **KMS** (§3), «дисплейная»
   половина DRM.
4. **Отправка команд рендеринга** на GPU (render-половина).

Userspace общается с DRM через ioctl'ы, но напрямую их не дёргают — есть
библиотека **libdrm** (`xf86drm.h` — ядро, `xf86drmMode.h` — KMS). Именно её
функции (`drmModeGet*`, `drmIoctl`, `drmSetClientCap`, ...) вы видите в коде.

### Ноды устройства: card vs render

В `/dev/dri/` две разновидности:

| Нода | Пример | Что умеет | Права |
|---|---|---|---|
| **primary (card)** | `card0`, `card1` | mode-setting + рендер + GEM | привилегированная, есть понятие master |
| **render** | `renderD128` | **только** offscreen-рендер/вычисления | безопасна, доступна без прав (`crw-rw-rw-`) |

Отсюда ключевое разделение в проекте: **захват экрана** идёт через card-ноду
(нужен доступ к дисплейным буферам), а **распаковка пикселей на GPU** (EGL) — через
render-ноду, без привилегий. См. [correct-approach.md](correct-approach.md) §1–2.

### DRM master

Настраивать вывод (mode-setting) может **только один клиент одновременно** —
**DRM master**. Им становится Wayland/X-композитор. `drmSetMaster` для чужого
процесса → `EACCES`. Это причина, по которой захват требует привилегий, а не
«перехвата» — подробно в [mistakes.md](mistakes.md) §A.1.

---

## 2. KMS в целом

**KMS (Kernel Mode Setting)** — часть DRM, отвечающая за **дисплей**: какие
мониторы подключены, в каком разрешении/частоте работают, какой буфер выводится
на каждый из них и как слои композятся. Установка режима происходит в ядре (а не
в X-сервере, как исторически), отсюда «kernel» в названии.

---

## 3. Объектная модель KMS

Пять типов объектов и связи между ними:

```
                       ┌── Plane (Cursor)   ── FB ── буфер
   Connector ── CRTC ──┼── Plane (Primary)  ── FB ── буфер
   (монитор)  (scanout)└── Plane (Overlay)  ── FB ── буфер
       │         │
     Mode     тайминги
  (1920x1080@60)
```

- **Connector** — физический выход (HDMI, DisplayPort, eDP). Знает: подключён ли
  монитор (`DRM_MODE_CONNECTED`), список поддерживаемых режимов, EDID, свойства
  (`CRTC_ID`, `HDR_OUTPUT_METADATA`, ...).
- **CRTC** («scanout engine») — читает framebuffer'ы и генерирует видеосигнал с
  таймингами режима. Связывает картинку с конкретным выходом.
- **Encoder** — (legacy-абстракция) преобразователь сигнала CRTC под тип
  коннектора. В атомарном API почти не виден.
- **Plane** — слой, который CRTC композит. Три роли:
  - **Primary** — основной кадр (рабочий стол);
  - **Cursor** — курсор (отдельный маленький буфер, чтобы двигать без
    перерисовки);
  - **Overlay** — доп. слой (например, видео).
- **Framebuffer (FB)** — **не пиксели**, а дескриптор: ссылка на буфер памяти +
  раскладка (`width/height/format/pitch/offset/modifier`). На него ссылается
  plane.

Плюс **Mode** — разрешение и тайминги (`drmModeModeInfo`), и **Property** —
пары ключ-значение на объектах (основа атомарного API).

### Как это перечисляется в коде (`src/kms.c`)

```
drmModeGetResources        → списки connector'ов, CRTC, encoder'ов
drmModeGetConnectorCurrent → свойства коннектора (CRTC_ID, HDR...)
drmModeGetPlaneResources   → список plane'ов
drmModeGetPlane            → plane, его fb_id и crtc_id
drmModeGetFB2              → дескриптор FB (handles/pitches/offsets/format/modifier)
```

---

## 4. Legacy API vs Atomic API

Два поколения интерфейса KMS:

- **Legacy** — императивный: `drmModeSetCrtc`, `drmModeGetEncoder`, отдельные
  вызовы на курсор/плоскость. Проще, но без гарантий атомарности.
- **Atomic** — декларативный: собираешь **все** изменения как набор свойств
  (`drmModeAtomicAddProperty`) и применяешь одним `drmModeAtomicCommit` —
  либо всё, либо ничего (нет промежуточных «мигающих» состояний).

Включаются клиентскими капами:

```c
drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1); // видеть все plane'ы (в т.ч. cursor/overlay)
drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);           // атомарный режим + свойство CRTC_ID у коннектора
```

В проекте atomic-капа нужна **не для коммитов**, а чтобы у коннектора появилось
свойство `CRTC_ID` для маппинга plane→монитор.

---

## 5. Capabilities

- `drmGetCap(fd, DRM_CAP_DUMB_BUFFER, ...)` — поддерживает ли устройство простые
  CPU-буферы (см. §6).
- `drmSetClientCap(...)` — запросить режим работы клиента (universal planes,
  atomic).

---

## 6. Буферы: GEM, dumb buffers, PRIME/dma-buf

### GEM

**GEM (Graphics Execution Manager)** — менеджер объектов памяти GPU. Буфер
адресуется **handle** — но handle **локален для одного открытого fd** и сам по
себе не шарится между процессами.

### Dumb buffers

**Dumb buffer** — простой линейный CPU-доступный буфер
(`DRM_IOCTL_MODE_CREATE_DUMB` → `MAP_DUMB` → `mmap`). Годится для программного
рисования **своего** содержимого на экран. Именно это делает экспериментальная
ветка `src/mydrm.c` — и потому она **не** про захват (нельзя так прочитать чужой
тайленый кадр композитора). См. [mistakes.md](mistakes.md) §A.2.

### PRIME / dma-buf — как шарить буфер

**dma-buf** — механизм ядра для разделения буфера между драйверами/процессами/
устройствами через **файловый дескриптор**. **PRIME** — мост GEM↔dma-buf:

```c
drmPrimeHandleToFD(fd, handle, O_RDONLY, &dmabuf_fd); // экспорт: GEM handle → переносимый fd
drmPrimeFDToHandle(fd, dmabuf_fd, &handle);           // импорт обратно
```

Это ключ ко всему проекту: framebuffer композитора → его GEM-хендлы →
`drmPrimeHandleToFD` → **dma-buf fd**, который можно передать другому процессу
(через `SCM_RIGHTS`) и импортировать в EGL на render-ноде. См.
[egl.md](egl.md) и [pipeline.md](pipeline.md).

> Тонкость раскладки: буфер бывает **тайленым/сжатым** (модификатор, напр. AMD
> DCC). Тогда `pitch/offset` мало — нужен ещё **modifier**, иначе потребитель
> прочитает мусор.

---

## 7. Два сценария взаимодействия в проекте

| | Захват (чтение экрана) | Рисование (эксперимент `mydrm.c`) |
|---|---|---|
| Направление | читаем чужой кадр | пишем свой на экран |
| Буфер | FB композитора → dma-buf | свой dumb buffer |
| Ключевые вызовы | `GetFB2` + `PrimeHandleToFD` | `CreateDumb` + `AddFB` + `SetCrtc` |
| Права | root/`CAP_SYS_ADMIN` | DRM master (TTY) |
| Итог | пиксели через EGL | картинка на мониторе |

Проект целится в **левый** столбец. Правый — учебная ветка, оставлена как
эксперимент.

---

## Куда дальше

- Что делает с полученным dma-buf вторая половина → [egl.md](egl.md).
- Полный конвейер end-to-end → [pipeline.md](pipeline.md).
- Правильная архитектура прав/процессов → [correct-approach.md](correct-approach.md).

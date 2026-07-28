# Документация daemon-screen

Оглавление технических заметок по проекту. Захват экрана здесь — это конвейер из
трёх шагов: **получить буфер (DRM/KMS) → распаковать в пиксели (EGL + OpenGL ES)
→ закодировать (libpng)**. Документы ниже поясняют и сам конвейер, и технологии
под ним, и историю решений.

## Как это работает

- **[pipeline.md](pipeline.md)** — рабочий конвейер end-to-end: что делает EGL и
  как core отдаёт ему буферы, пошагово по коду.
- **[design-choices.md](design-choices.md)** — три шага конвейера и что на каждом
  можно заменить (KMS vs portal; EGL+GLES vs Vulkan vs mmap; libpng vs кодек).

## Технологии (что это и зачем)

- **[drm-kms.md](drm-kms.md)** — что такое DRM и KMS, объектная модель
  (connector/CRTC/plane/framebuffer), буферы, GEM/PRIME/dma-buf.
- **[egl.md](egl.md)** — что такое EGL, его объекты, headless через GBM, импорт
  dma-buf и связь с OpenGL ES.

## Про проект (решения и история)

- **[mistakes.md](mistakes.md)** — что было не так в исходной реализации, включая
  разбор ветки `OpenGL_part` (что было верно и чего не хватило).
- **[correct-approach.md](correct-approach.md)** — правильная модель прав,
  архитектура (привилегированный хелпер + `SCM_RIGHTS`) и дорожная карта.

## Рекомендуемый порядок чтения

1. [drm-kms.md](drm-kms.md) → [egl.md](egl.md) — база: буфер и как из него достать
   пиксели.
2. [pipeline.md](pipeline.md) → [design-choices.md](design-choices.md) — как это
   собрано и какие были развилки.
3. [mistakes.md](mistakes.md) → [correct-approach.md](correct-approach.md) —
   почему исходный код не работал и как довести до боевого компонента.

# Refactored image compression project

Проект разбит на несколько модулей, чтобы каждая часть отвечала за свою задачу.
Во всех исходниках есть короткие комментарии, а для Windows добавлена сборка без `make` — прямо через `Ctrl+Shift+B` в VS Code.

Главное изменение: результаты больше не сохраняются в несжатый BMP-контейнер.

- `jpeg` пишет настоящий JPEG-файл с расширением `.jpg`.
- `svd` пишет бинарный файл с расширением `.svd`, где лежат усечённые SVD-компоненты.
- `scripts/svd_to_bmp.c` собирается в отдельную утилиту `svd_to_bmp.exe`/`svd_to_bmp` и восстанавливает BMP из `.svd`, если нужно посмотреть результат.

## Структура

- `include/common.h`, `src/common.c` — базовые структуры и общие утилиты.
- `include/bmp.h`, `src/bmp.c` — чтение и запись BMP.
- `include/jpeg_writer.h`, `src/jpeg_writer.c` — запись настоящего baseline JPEG.
- `include/metrics.h`, `src/metrics.c` — расчёт PSNR и SSIM.
- `include/svd.h`, `src/svd.c` — SVD-сжатие и запись бинарного `.svd`.
- `include/jpeg_like.h`, `src/jpeg_like.c` — JPEG-подобное сжатие на основе DCT.
- `include/cli.h`, `src/cli.c` — help и печать статистики.
- `include/compare.h`, `src/compare.c` — пакетный режим сравнения и CSV-отчёт.
- `scripts/svd_to_bmp.c` — C-утилита восстановления `.bmp` из `.svd`.
- `src/main.c` — точка входа и выбор режима работы.
- `build.bat` — сборка на Windows через `gcc` без `make`.
- `.vscode/tasks.json` — задача VS Code для `Ctrl+Shift+B`.

## Сборка на Windows через VS Code

### Что нужно установить

Нужен `gcc` в `PATH`.
Самый удобный вариант — MSYS2 с пакетом `gcc`.

После установки компилятора:

1. Открой папку проекта в VS Code.
2. Нажми `Ctrl+Shift+B`.
3. VS Code выполнит `build.bat` и соберёт `image_compressor.exe` и `svd_to_bmp.exe`.

Если хочешь, можно собрать и вручную двойным кликом по `build.bat` или из `cmd`/PowerShell:

```bat
build.bat
```

## Сборка на Linux/macOS

```bash
make
```

## Запуск

### Windows

```powershell
.\image_compressor.exe svd input.bmp out_svd.svd 20
.\svd_to_bmp.exe out_svd.svd out_svd.bmp
.\image_compressor.exe jpeg input.bmp out_jpeg.jpg 60
.\image_compressor.exe compare input.bmp results 5,10,20,40 20,40,60,80
```

### Linux/macOS

```bash
./image_compressor svd input.bmp out_svd.svd 20
./svd_to_bmp out_svd.svd out_svd.bmp
./image_compressor jpeg input.bmp out_jpeg.jpg 60
./image_compressor compare input.bmp results 5,10,20,40 20,40,60,80
```

Если в `svd` передать путь вроде `out_svd.bmp`, программа сама заменит расширение на `.svd`.
Если в `jpeg` передать путь вроде `out_jpeg.bmp`, программа сама заменит расширение на `.jpg`.

## Примеры

```text
image_compressor svd     <input.bmp> <output.svd> <k>
image_compressor jpeg    <input.bmp> <output.jpg> <quality>
image_compressor compare <input.bmp> <out_dir> <k_list> <quality_list>
svd_to_bmp               <input.svd> <output.bmp>
```

`compare` теперь создаёт файлы вида:

```text
results/svd_k20.svd
results/jpeg_q60.jpg
results/summary.csv
```

# 🚀 SputnikAuthV2

Система идентификации спутников по наблюдениям с использованием TLE-каталогов (Celestrak).

---

# Сборка проекта (Windows + MinGW + vcpkg)

## Требования

* CMake ≥ 3.16
* Ninja
* MinGW (Strawberry)
* vcpkg

---

## Установка зависимостей

```bash
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe install curl:x64-mingw-dynamic
```

---

## Сборка проекта

```bash
# из корня проекта
Remove-Item -Recurse -Force build
mkdir build
cd build

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="../vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build .
```

---

## Запуск

```bash
.\SputnikAuthV2.exe./
```

---

# Загрузка каталогов (Celestrak)

В проекте реализована загрузка TLE напрямую с сайта:

```cpp
auto catalog = data.loadCatalogFromCelestrak("stations");
```

Пример групп:

* `stations`
* `starlink`
* `active`
* `weather`

---

# Архитектура

```text
Data → Prepare → Core → Output
```

* **DataModule** — загрузка (файлы, интернет)
* **PrepareModule** — предобработка
* **CoreModule** — алгоритмы идентификации
* **OutputModule** — вывод результатов

---

# Документация (Doxygen)

## Генерация HTML

```bash
doxygen Doxyfile
```

Открыть:

```text
html/index.html
```

---

# Текущее состояние

* ✔ реализована архитектура
* ✔ подключён интернет (libcurl)
* ✔ загрузка и парсинг TLE
* алгоритмы идентификации — в разработке

---

#  Важно

* Для работы curl используется `vcpkg` и triplet `x64-mingw-dynamic`

---

# Проверка
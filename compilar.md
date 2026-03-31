# Compilar NetPortChk en Visual Studio 2026

## Requisitos previos

| Componente | Detalle |
|---|---|
| Visual Studio 2026 | Community, Professional o Enterprise |
| Carga de trabajo | **Desarrollo de escritorio con C++** |
| Componente MFC | MFC para x64 (incluido en la carga de trabajo anterior) |
| SDK de Windows | 10.0 o superior |
| Python o PowerShell | PowerShell 5.1+ (preinstalado en Windows 10/11) |
| HTML Help Workshop | Opcional — necesario para compilar el archivo `.chm` |

---

## Abrir la solución

1. Abra Visual Studio 2026.
2. **Archivo → Abrir → Proyecto o solución…**
3. Seleccione `NetPortChk\NetPortChk.sln`.

---

## Configuración del proyecto

El proyecto está configurado como:

| Parámetro | Valor |
|---|---|
| Plataforma | x64 |
| Juego de caracteres | Unicode |
| Uso de MFC | MFC dinámica (`mfc140u.dll`) |
| Estándar C++ | C++20 |
| Configuraciones | Debug / Release |

No es necesario modificar ninguna propiedad para una compilación estándar.

---

## Compilar

### Desde el IDE

1. Seleccione la configuración deseada en la barra de herramientas:
   - **Debug|x64** — para desarrollo y depuración.
   - **Release|x64** — para distribución.
2. Pulse **Compilar → Compilar solución** (`Ctrl+Mayús+B`).

### Desde la línea de comandos

```
"C:\Program Files\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe" ^
    NetPortChk\NetPortChk.sln ^
    /p:Configuration=Release /p:Platform=x64
```

---

## Evento de pre-compilación — incremento de versión

Antes de cada compilación, el archivo `scripts\increment_build.ps1` incrementa
automáticamente `VER_BUILD` en `src\version.h`.

El comando configurado en el proyecto es:

```
powershell -ExecutionPolicy Bypass -File "$(ProjectDir)scripts\increment_build.ps1" -VersionFile "$(ProjectDir)src\version.h"
```

> Si PowerShell no está disponible o está bloqueado por directiva de grupo,
> la compilación fallará en el paso de pre-build.
> En ese caso, comente o elimine el `<PreBuildEvent>` en el `.vcxproj`.

---

## Evento de post-compilación — archivo de ayuda CHM

Si **HTML Help Workshop** está instalado en la ruta estándar
`%ProgramFiles(x86)%\HTML Help Workshop\`, el evento de post-compilación
compila automáticamente `help\NetPortChk.hhp` y copia el resultado
`NetPortChk.chm` al directorio de salida.

Si HTML Help Workshop no está instalado, el evento muestra un aviso pero
**no interrumpe la compilación**.

Descarga gratuita:
<https://web.archive.org/web/2024/https://www.microsoft.com/en-us/download/details.aspx?id=21138>

---

## Archivos de salida

| Archivo | Descripción |
|---|---|
| `x64\Release\NetPortChk.exe` | Ejecutable de distribución |
| `x64\Release\NetPortChk.chm` | Ayuda compilada (si hhc.exe disponible) |
| `x64\Debug\NetPortChk.exe` | Ejecutable de depuración |

> **MFC dinámica:** para distribuir la aplicación es necesario incluir los
> redistribuibles de Visual C++ 2022–2026, o instalar el paquete
> *Microsoft Visual C++ Redistributable* en el equipo destino.

---

## Estructura del proyecto

```
NetPortChk\
├── NetPortChk.sln
├── NetPortChk.vcxproj
├── compilar.md              ← este archivo
├── scripts\
│   └── increment_build.ps1  ← incrementa VER_BUILD en cada compilación
├── src\
│   ├── version.h            ← versión del producto (editar aquí)
│   ├── resource.h
│   ├── NetPortChk.rc
│   └── *.h / *.cpp
├── res\
│   └── *.ico
└── help\
    ├── NetPortChk.hhp       ← proyecto de ayuda (compilado por hhc.exe)
    └── *.htm
```

---

## Cambiar la versión del producto

Edite `src\version.h` y modifique `VER_MAJOR`, `VER_MINOR` o `VER_PATCH`.
`VER_BUILD` se incrementa automáticamente en cada compilación.

```cpp
#define VER_MAJOR   1
#define VER_MINOR   3
#define VER_PATCH   0
#define VER_BUILD   0   // gestionado por increment_build.ps1
```

# PandOs

# Come compilare

## Cmake
Il progetto utilizza CMake per eseguire la compilazione e il linking dei file.

NOTA:
Le path in cui trovare la libreria di umps3 è stata settata sulle nostre configurazioni,
pertanto se avete una path diversa dovete eseguire da terminale il seguente comando.

ESEMPIO:
```bash
cmake -B build -S . -DUMPS3_PREFIX_DIR=umps3-prefix-dir -DUMPS3_LIB_DIR=umps3-lib-dir 
cmake --build build
```

## Come compilare
Per eseguire la build dei files utilizzare il comando nella directory principale:
```bash
cmake -B build -S .
cmake --build build
```

# Integración en el paquete de la vACC (vatsimspain/Operaciones)

Con estos cambios el plugin se carga automáticamente con los perfiles LECB, LECM y GCCC y puede
dibujar sus ventanas sin que el controlador configure nada.

## 1. Archivos del plugin

Copiar el contenido de `ESRunwayPlugin.zip` (página *Releases*) en:

```
LEXX/Plugins/ESRunwayPlugin/ESRunwayPlugin.dll
LEXX/Plugins/ESRunwayPlugin/LEXX.json
```

Un único DLL y un único `LEXX.json` sirven para los tres FIR.

## 2. Perfiles

Añadir tres líneas al final del bloque `Plugins` de cada perfil, con el siguiente número libre
(separadas por tabuladores, como el resto):

**LECB/LECB.prf** y **LECM/LECM.prf** (el último plugin actual es `Plugin10`):

```
Plugins	Plugin11	\LEXX\Plugins\ESRunwayPlugin\ESRunwayPlugin.dll
Plugins	Plugin11Display0	Standard ES radar screen
Plugins	Plugin11Display1	Ground Radar display
```

**GCCC/GCCC.prf** (el último plugin actual es `Plugin11`):

```
Plugins	Plugin12	\LEXX\Plugins\ESRunwayPlugin\ESRunwayPlugin.dll
Plugins	Plugin12Display0	Standard ES radar screen
Plugins	Plugin12Display1	Ground Radar display
```

- La primera línea carga el plugin al abrir el perfil.
- Las líneas `Display` equivalen a *Allowed to draw on types*: las ventanas del plugin se ven en la
  pantalla de radar estándar y en la de Ground Radar (TWR/GND).

## 3. Actualizaciones

- **Nueva versión del plugin**: sustituir `ESRunwayPlugin.dll` (y `LEXX.json` si cambia).
- **Cambios de AIRAC**: cada aeródromo de `LEXX.json` indica la sección y la enmienda del AIP de la que
  procede (`source`). Las reglas se pueden editar sin recompilar el plugin.

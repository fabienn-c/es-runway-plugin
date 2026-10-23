# Integración en el paquete de la vACC (vatsimspain/Operaciones)

Con estos cambios el plugin se carga automáticamente con los perfiles LECB, LECM y GCCC y puede
dibujar sus ventanas sin que el controlador configure nada. Es lo que contiene el pull request
[vatsimspain/Operaciones#960](https://github.com/vatsimspain/Operaciones/pull/960).

## 1. Archivos del plugin

Copiar el contenido de `ESRunwayPlugin.zip` (página
[Releases](https://github.com/fabienn-c/es-runway-plugin/releases)) en:

```
LEXX/Plugins/ESRunwayPlugin/ESRunwayPlugin.dll
LEXX/Plugins/ESRunwayPlugin/LEXX.json
```

- Un único DLL y un único `LEXX.json` sirven para los tres FIR.
- `LEXX.json` tiene que estar en la misma carpeta que el DLL.

## 2. Perfiles

Añadir tres líneas al final del bloque `Plugins` de cada perfil (`LECB/LECB.prf`, `LECM/LECM.prf`,
`GCCC/GCCC.prf`), separadas por tabuladores como el resto. `N` es el siguiente número libre: el último
`PluginN` que ya exista en ese perfil, más uno.

```
Plugins	PluginN	\LEXX\Plugins\ESRunwayPlugin\ESRunwayPlugin.dll
Plugins	PluginNDisplay0	Standard ES radar screen
Plugins	PluginNDisplay1	Ground Radar display
```

En los perfiles de octubre de 2026 eso es `Plugin11` en LECB y LECM, y `Plugin12` en GCCC (GCCC carga
también ESCenterlines).

- La primera línea carga el plugin al abrir el perfil.
- Las líneas `Display` equivalen a *Allowed to draw on types*: las ventanas del plugin se ven en la
  pantalla de radar estándar y en la de Ground Radar (TWR/GND). Los nombres tienen que coincidir
  exactamente con los de los otros plugins del perfil.

## 3. Actualizaciones

- **Nueva versión del plugin**: sustituir `ESRunwayPlugin.dll` y `LEXX.json` por los del nuevo
  `ESRunwayPlugin.zip`. Los perfiles no cambian.
- **Cambios de AIRAC**: cada aeródromo de `LEXX.json` indica la sección y la enmienda del AIP de la que
  procede (`source`). Si el AIP cambia una configuración preferente, un límite de viento o las
  aproximaciones, se actualiza ese aeródromo en `LEXX.json`; el plugin lo lee al cargarse, sin
  recompilar. El significado de cada campo está en el [README](../README.md#rules-file-lexxjson).
- **Nuevas pistas en el fichero de sector**: los nombres de pista de `LEXX.json` tienen que coincidir
  exactamente con los del fichero de sector (por ejemplo `06L`, no `6L`). Un aeródromo del sector sin
  reglas en `LEXX.json` se queda sin pistas seleccionadas.
- Los cambios en las reglas se pueden proponer en el repositorio del plugin
  ([fabienn-c/es-runway-plugin](https://github.com/fabienn-c/es-runway-plugin)): sus pruebas automáticas
  cargan el `LEXX.json` real y comprueban muchas decisiones.

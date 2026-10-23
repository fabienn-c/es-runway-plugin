# ES Runway Plugin: guía para controladores

El plugin propone y selecciona las pistas en uso de todos los aeródromos del sector cargado en
EuroScope (LECB, LECM o GCCC), según el METAR de VATSIM, las configuraciones preferentes del AIP, el
mejor tipo de aproximación disponible y las reglas de día y noche.

**El plugin nunca cambia nada sin que el controlador lo confirme.** Solo toca las casillas de pista
(DEP/ARR de cada pista), nunca las casillas de aeródromo.

*LT = hora local del aeródromo (en Canarias, hora canaria).*

## Al abrir EuroScope

1. El plugin descarga los METAR (1–2 segundos) y calcula la configuración de cada aeródromo. Sin
   conexión a internet, espera a tener METAR antes de continuar.
2. Aparece una ventana **"Runway configuration ready"** con un único botón **OK**. Hasta que no se pulsa,
   no se toca nada.
3. Al pulsar **OK**, el plugin abre el diálogo *Active airport/runway*, marca las pistas y pulsa OK
   por sí mismo. **No uses el ratón mientras trabaja** (unos segundos; el puntero se mueve solo).
4. Al terminar aparece un resumen: primero los avisos (sin METAR, METAR antiguo, aeródromos AFIS,
   aeródromos sin reglas), después el número de aeródromos configurados y la lista completa.

Si todo ya estaba correcto, no aparece la ventana de OK: solo el resumen.

## Durante la sesión: sugerencias

Con cada METAR nuevo (y en los cambios de día/noche) el plugin vuelve a calcular. Si la configuración
recomendada no coincide con la activa, aparece una ventana con:

- **Active**: la configuración actual (o *"runways not matching any configuration"* si las pistas marcadas
  no corresponden a ninguna configuración, por ejemplo con pistas de más).
- **Suggested**: la recomendada.
- **Reason**: el motivo (por ejemplo, *"RWY 24 not usable, tailwind 7 kt on RWY 24 (limit 5 kt)"*).
- **METAR**: el METAR usado.
- Botones **Accept** (el plugin abre el diálogo y cambia las pistas) y **Decline** (no se cambia nada;
  si la situación sigue igual, el siguiente METAR lo vuelve a sugerir).

Si la configuración activa ya es la recomendada, no aparece nada. Si hay varias ventanas, se muestran de
una en una (*"1 of 3"* en el título).

Qué aeródromos te sugiere depende de tu posición:

| Posición | Sugerencias |
|---|---|
| DEL / GND / TWR | Solo tu aeródromo (las 4 primeras letras del indicativo, p. ej. `LEBL_TWR`) |
| APP / CTR, observador o sin conectar | Todos los aeródromos del sector |

## Reglas que aplica

1. **Configuración preferente del AIP** siempre que el viento lo permita (límites del AIP, rachas
   incluidas). Ejemplos: LEBL oeste de día / norte cruzadas de noche, LEMD norte, LEMG sur, LEPA oeste,
   GCLP norte, GCLA 36.
2. **Sin preferencia en el AIP**: la pista con la mejor aproximación (ILS CAT III > CAT II > CAT I >
   RNP LPV > … > VOR > NDB > visual), si el viento lo permite. Con la misma aproximación en ambas
   cabeceras, la de más viento de cara; si el viento es parecido en las dos, se mantiene la actual.
3. **Si el viento no lo permite en ninguna**: la configuración que mejor se adapte al viento.
4. **Viento en calma o variable**: la configuración preferente o la de mejor aproximación, salvo en los
   aeródromos con pista indicada para calma: GEML 15, GCTS 07, GCXO 30, GCFV 01, GCHI 34.
5. Límites por defecto sin indicación en el AIP: **5 kt de cola y 15 kt cruzado**.

Casos especiales:

- **LEBL y LEMD de noche (23:00–07:00 LT)**: a las 23:00 y a las 07:00 aparece una sugerencia si hay que
  cambiar. La configuración actual se puede mantener mientras el tráfico sea alto; cuando vuelva a la
  normalidad, hay que pasar a la configuración nocturna.
- **LEMD fines de semana**: en configuración sur, los sábados y domingos la configuración nocturna se
  mantiene hasta las 09:00 LT; a las 09:00 se sugiere la diurna.
- **Baja visibilidad en LEBL/LEMD** (techo < 500 ft o visibilidad < 1,9 km): las configuraciones
  preferentes quedan en suspenso y se elige según el viento; en LEBL no se usa la 02/20.
- **Aeródromos AFIS** (LESU, LEBA, LEBG, LEHC, GCGM, y GCHI en fin de semana): **ninguna pista
  seleccionada**, los pilotos deciden la pista. Si alguien marca pistas en ellos, aparece un recordatorio
  *"deselect all runways"* al inicio, con `.rwy check` y en el cambio de GCHI (no con cada METAR).
- **GCHI**: torre de lunes a viernes, AFIS el fin de semana. El sábado a las 00:00 y el lunes a las 00:00
  (hora canaria) aparece un recordatorio del cambio.
- **METAR de más de 2 horas** (aeródromo cerrado): al inicio se deseleccionan sus pistas; durante la sesión
  no se sugiere nada para él.
- **Aeródromos sin reglas** (por ejemplo los extranjeros del sector LECM): al inicio se deseleccionan sus
  pistas.

## Ventanas del plugin

- Se pueden **mover** arrastrando la barra de título.
- Si el texto es largo, aparecen los botones **▲ / ▼** para desplazarse.
- Todo se repite también en la pestaña de chat **RWY** (los resúmenes largos se acortan; la lista completa
  está en `.rwy status` y en el registro).

## Si cambias de ventana mientras trabaja

Mientras marca casillas, el plugin solo actúa si EuroScope es la ventana activa. Si cambias a otro
programa (Alt-Tab), **se detiene** y continúa al volver. Tras 2 minutos fuera, cancela y lo indica.

Si no encuentra el icono del diálogo de pistas en la barra superior, **no hace clic**: te pide que
abras tú el diálogo y continúa desde ahí.

## Comandos

| Comando | Qué hace |
|---|---|
| `.rwy status` | Configuración activa y recomendada de cada aeródromo, con el motivo |
| `.rwy status LEBL` | Lo mismo para un aeródromo |
| `.rwy check` | Vuelve a comprobar todo ahora (y descarga los METAR) |
| `.rwy startup` | Vuelve a mostrar la ventana de OK del inicio |
| `.rwy accept` / `.rwy decline` | Responde a la sugerencia desde la línea de comandos |
| `.rwy airports` | Aeródromos del sector con reglas, y los que se deseleccionan |
| `.rwy find` | Comprueba que encuentra el icono del diálogo de pistas |
| `.rwy capture` | Guarda una imagen de la barra superior (para soporte) |

## Problemas

- El plugin escribe un registro en **`ESRunwayPlugin.log`**, junto al DLL. Adjúntalo al reportar un
  problema. **El registro se borra cada vez que se carga el plugin**: guárdalo antes de volver a abrir
  EuroScope.
- Si las ventanas no aparecen en la pantalla de radar: en *OTHER SET → Plug-ins…*, selecciona el plugin
  y pasa **Standard ES radar screen** (y **Ground Radar display** si usas esa pantalla) a *Allowed to draw
  on types*. Con el perfil de la vACC esto ya viene configurado. Mientras tanto, todo aparece en la
  pestaña **RWY**.
- Si el plugin no carga las reglas, comprueba que **`LEXX.json`** está en la misma carpeta que el DLL.
  Ahí están las reglas de cada aeródromo, con la sección y la enmienda del AIP de las que proceden.

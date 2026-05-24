# Obsolete Texture IDs

This list records generated worn `AssetID`s that are not referenced outside
`Lumen/AssetsEnum.h` as of 2026-05-24.

Scope:
- only paths under `./Assets/worn/`;
- generated enum/path entries do not count as usage;
- mock hanging-texture preview references in `Lumen/stubs.cpp` do not count as product usage;
- currently used worn IDs are omitted from the obsolete list.

## Unused worn icon IDs

- `BG_PATTERN`
- `BUTTON_1`
- `BUTTON`
- `BUTTON_ARROW_LEFT`
- `BUTTON_ARROW_LEFT_OVERLAY_SHADOW`
- `BUTTON_ARROW_RIGHT`
- `BUTTON_ARROW_RIGHT_OVERLAY_SHADOW`
- `CONNECTOR_BODY`
- `CONNECTOR_CROSS_DOT`
- `CONNECTOR_END`
- `CONNECTOR_START`
- `DRAG_IN_LIST_HOVER`
- `SCALE_CONTROL`

## Hanging Texture IDs

Scope: generated IDs whose paths are not under `./Assets/worn/` and that are not
referenced outside `Lumen/AssetsEnum.h`, except by the mock preview in `Lumen/stubs.cpp`.

This list is intended to stay visually connected to `GraphPanel::RenderMockAssetPreview()`.
When IDs are added or removed here, update the mock preview shelf in `Lumen/stubs.cpp`
so the demo graph can show the current hanging texture set before real packets arrive.

- `IC_ADD`
- `IC_ARROW_BOTTOM`
- `IC_ARROW_RATING_DECREASE`
- `IC_ARROW_RATING_INCREASE`
- `IC_ARROW_TOP`
- `IC_ERROR_16_PX`
- `IC_HISTORY_16PX`
- `IC_INFORMATION_16_PX`
- `IC_LOGS_16_PX`
- `IC_RUN`
- `IC_WARNING_16_PX`
- `LOCALSAMPLE_10PX`
- `LOCALSAMPLE_16PX`
- `LOCALSAMPLE_24PX`
- `LOCALSAMPLE_36PX`
- `LOCALSAMPLE_7PX`
- `LOCALSERVER_10PX`
- `LOCALSERVER_16PX`
- `LOCALSERVER_24PX`
- `LOCALSERVER_36PX`
- `LOCALSERVER_7PX`
- `LOCALUSER_10PX`
- `LOCALUSER_16PX`
- `LOCALUSER_24PX`
- `LOCALUSER_36PX`
- `LOCALUSER_7PX`
- `OFFLINE_11PX`
- `OFFLINE_17PX`
- `OFFLINE_26PX`
- `OFFLINE_5PX`
- `OFFLINE_7PX`
- `ONLINE_11PX`
- `ONLINE_17PX`
- `ONLINE_26PX`
- `ONLINE_5PX`
- `ONLINE_7PX`
- `PARTYLEADER_13PX`
- `PARTYLEADER_20PX`
- `PARTYLEADER_30PX`
- `PARTYLEADER_46PX`
- `PARTYLEADER_8PX`

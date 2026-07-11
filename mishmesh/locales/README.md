# Locales

Locale files are UTF-8 JSON. `en_US.json` is the canonical key list and every
other file is named with an `ll_CC` locale tag, for example `sv_SE.json`.

After editing or adding a locale, regenerate the checked-in C++ table:

```sh
python3 tools/generate_locales.py
```

Use this in CI or before committing to verify that generated files are current:

```sh
python3 tools/generate_locales.py --check
```

A locale may omit keys; the firmware falls back to English. Unknown keys and
placeholder mismatches are rejected by the generator. The `_meta.nativeName`
value is shown in the on-device language picker under **Settings → Home**.

The JSON files are build-time sources. They are not parsed or uploaded on the
device; the generator emits a compact header-only table in
`mishmesh/core/LocaleStrings.h`.

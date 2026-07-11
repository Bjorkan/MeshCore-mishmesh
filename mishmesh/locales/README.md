# Locales

Each language is stored in exactly one UTF-8 TOML file in this directory.
`en_US.toml` is the canonical locale; additional languages use an `ll_CC.toml`
locale tag, for example `sv_SE.toml`.

Locale metadata lives at the top of the file:

```toml
[meta]
name = "Swedish"
native_name = "Svenska"
```

Group translations by the screen or component that owns them. Nested TOML
tables become dotted translation keys. For example:

```toml
[messages.settings]
quick_replies = "Quick replies"
```

produces the key `messages.settings.quick_replies`.

Keep all translations for a language in its single TOML file. Do not create
per-screen locale fragments or additional locale files.

After editing or adding a locale, regenerate the checked-in C++ tables:

```sh
python3 tools/generate_locales.py
```

Use this in CI or before committing to verify that generated files are current:

```sh
python3 tools/generate_locales.py --check
```

A locale may omit keys; the firmware falls back to English. Unknown keys and
placeholder mismatches are rejected by the generator. The `meta.native_name`
value is shown in the on-device language picker under **Settings → Home**.

The TOML files are build-time sources. They are not parsed or uploaded on the
device; the generator emits compact header-only lookup tables under
`mishmesh/core/`.

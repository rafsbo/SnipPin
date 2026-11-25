# Translating SnipPin

SnipPin ships in the 24 official languages of the European Union. English is built into the program; every other language is a plain text file in [`res/lang/`](../res/lang).

The initial translations were produced without native-speaker review. **Corrections from native speakers are very welcome**, even a single word.

| Code | Language | Code | Language | Code | Language |
|---|---|---|---|---|---|
| `bg` | Български | `fi` | Suomi | `nl` | Nederlands |
| `cs` | Čeština | `fr` | Français | `pl` | Polski |
| `da` | Dansk | `ga` | Gaeilge | `pt` | Português |
| `de` | Deutsch | `hr` | Hrvatski | `ro` | Română |
| `el` | Ελληνικά | `hu` | Magyar | `sk` | Slovenčina |
| `en` | English | `it` | Italiano | `sl` | Slovenščina |
| `es` | Español | `lt` | Lietuvių | `sv` | Svenska |
| `et` | Eesti | `lv` | Latviešu | `mt` | Malti |

## How the language is chosen

*Settings → Language* offers **Automatic (system language)**, the default, plus every available language. *Automatic* follows the Windows display language and falls back to English when that language is not available. The choice applies immediately, without a restart, and is saved as `Language=` in `settings.ini`.

## File format

Each file is UTF-8 text, one `key = value` per line:

```
# Comment lines start with # or ;
language.code = it
language.name = Italiano
language.name_en = Italian

menu.always_on_top = Sempre in &primo piano
notify.recall_launching = Apertura di «%s»…
```

Rules (all checked by the unit tests, so a mistake cannot slip into a release):

1. **Keep every key.** Translate only the text after `=`. The full list of keys, with the English text, is in [`res/lang/en.lang`](../res/lang/en.lang).
2. **Placeholders**: `%s` (a text) and `%d` (a number) must stay, in the same order. At runtime a string with wrong placeholders is ignored and English is shown instead.
3. **`&` marks the keyboard accelerator** in menus: `&Opacity` is shown as <u>O</u>pacity. If the English text has one `&`, the translation must have exactly one. If it has none, use none. Write `&&` for a literal ampersand.
4. **Escapes**: `\n` is a line break (used in the About box), `\t` a tab, `\\` a backslash.
5. Keep **SnipPin**, **Windows**, **Ctrl**, **UIAccess** and the GitHub URL unchanged.
6. Keep texts short. Menus and the settings dialog have limited width, and long labels are cut with "…".

## Improving an existing translation

1. Edit `res/lang/<code>.lang` on GitHub (the pencil icon) or in a fork.
2. Open a pull request. CI checks the file automatically.

To see the result **without compiling**, create a folder named `lang` next to `SnipPin.exe` and put your edited `<code>.lang` there. Files in that folder override the built-in translation with the same code. Then choose the language in *Settings → Language*.

## Adding a new language

Languages outside the EU are welcome too.

1. Copy `res/lang/en.lang` to `res/lang/<code>.lang`, using the two-letter ISO 639-1 code.
2. Set `language.code`, `language.name` (the name in the language itself) and `language.name_en`, then translate every value.
3. Add one line to [`res/Languages.rc`](../res/Languages.rc):
   ```
   XX SNIPPINLANG "lang/xx.lang"
   ```
4. For *Automatic* detection, add the language to `CodeFromLangId()` in `src/platform/Localization.cpp`. Without it, the language can still be chosen by hand.
5. Run the tests (see below).

## For developers: adding or changing a UI string

All UI strings are declared once, with their key and English text, in [`src/i18n/StringTable.h`](../src/i18n/StringTable.h). Use them with `i18n::Tr(StringId::...)`, or `i18n::TrFormat(StringId::..., {args})` for strings with placeholders. Never use `printf`-style functions with translated text.

After changing the table:

```bash
cmake --preset tests-host && cmake --build --preset tests-host
```

```bash
./build/tests-host/snippin_tests --dump-english > res/lang/en.lang
```

```bash
ctest --preset tests-host
```

The tests list every catalogue that is missing the new key. Add the key to each language file. If you can't translate it, copy the English text and mention it in the pull request so native speakers can follow up. A missing key fails CI, but at runtime it would simply show English.

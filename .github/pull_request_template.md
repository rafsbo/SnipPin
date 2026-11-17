## Summary

<!-- What does this change and why? -->

## Testing

- [ ] Unit tests pass (`ctest`)
- [ ] Checked by hand on: <!-- e.g. Windows 7 SP1 x86, Windows 11 x64 -->
- [ ] Relevant items of `docs/TESTING.md` verified

## Checklist

- [ ] Code, comments and UI text are in English
- [ ] No new runtime dependency; post-Windows-7 APIs are resolved through `ApiShim`
- [ ] Changes to other applications' windows go through `WindowTracker`
- [ ] `CHANGELOG.md` updated (for user-visible changes)

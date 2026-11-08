# Curses Access release notes

## 0.0.3 (2026-11-08)

- New **Clear Screen Before Commands** setting in Program Configuration
  (default On). Each program starts on a blank screen, so the previous
  command's output no longer shows. It uses the terminal's own `clear`
  sequence from terminfo.

## 0.0.2 (2026-11-07)

- **The menu can't be closed by accident.** At the menu, ^C, ^\, ^Z and
  ^S/^Q are read as ordinary keys and do nothing. SIGINT, SIGQUIT, SIGTSTP
  and SIGTERM are ignored. SIGHUP (the terminal went away) restores the
  terminal and exits.
- **Programs started from the menu run as their own foreground job.** ^C
  and ^\ work in them as usual. ^Z can no longer leave the menu waiting
  forever: a stopped program is resumed at once.
- Every function has a comment, the spelling "color" is used throughout,
  and the tests carry full license headers.
- New `.gitignore`, so a Git checkout holds only the sources. Run
  `./autogen.sh` first to build from Git.

## 0.0.1 (2026-11-06)

First release.

- Lettered menus A–Z with sub-menus up to six levels deep. The layout
  follows the terminal size, down to 64 × 20.
- Menu Maintenance: Create / Modify editor, Program Configuration, Color
  Themes, Usage Report, and Passwords & Recovery.
- Exit, Maintenance and per-entry passwords, stored as Argon2id hashes
  (libsodium) or scrypt hashes (OpenSSL 3).
- A recovery code, shown once, that removes forgotten passwords.
- An encrypted usage log that only the Maintenance password or the
  recovery code can read.
- Six built-in color themes, plus themes you save yourself.
- A screen blanker, System Info, and a rainbow start-up logo that you can
  replace with your own.

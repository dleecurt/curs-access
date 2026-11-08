# Curses Access

Curses Access (`curs-access`) is a lettered menu launcher for the Linux
terminal. It is written in C with ncurses and modelled on the DOS menu
systems of the early 1990s.

You get a main menu of up to 26 entries, **A** to **Z**. Each entry either
runs a program or opens a sub-menu, and sub-menus go up to six levels deep.
Press a letter to run an entry. When the program ends you're back at the menu.

All screens resize to fit the terminal they run in. The smallest supported
size is 64 × 20.

## Features

- **Lettered menus** with a selection bar, a greeting that follows the time
  of day, and the date and time in the bottom border.
- **Menu Maintenance (F1)**:
  - Create / Modify screens for each menu: insert, delete, move, custom
    titles, a menu tree view, per-entry options and sub-menus.
  - Program Configuration.
  - Color Themes.
  - Usage Report.
  - Passwords & Recovery.
- **Passwords** for Exit, for Menu Maintenance and for individual entries.
  They are stored only as slow one-way hashes (Argon2id with libsodium,
  scrypt with OpenSSL).
- **Recovery code**, shown once on first start. If you forget the Exit or
  Maintenance password, press **F4** at the password prompt, or start with
  `curs-access --recover`. The passwords are removed and the usage log can
  still be read.
- **Encrypted usage log**: when Usage Tracking is on, each program run is
  logged. Each entry is sealed to a public key, so writing an entry needs no
  password. Only someone with the Maintenance password or the recovery code
  can read the report.
- **Color themes**: six built-in themes (Classic, Blue Screen, Monochrome,
  Amber, Green Screen, Midnight). Every color can be changed in Program
  Configuration, and you can save your own themes.
- **Clean screen for each program**: with Clear Screen Before Commands
  (Program Configuration, on by default), a program never starts with the
  previous command's output still on the screen.
- **Can't be closed by accident**: ^C, ^\, ^Z, ^S/^Q and a plain `kill`
  (SIGTERM) do nothing at the menu. Programs started from the menu still
  get ^C and ^\ as usual, but ^Z doesn't suspend them, because the menu
  resumes them at once. Only SIGHUP (the terminal went away) and SIGKILL
  end the program.
- **Screen blanker** after a set number of idle minutes.
- **System Info (F3)**: host, kernel, CPU, memory, disk and terminal.
- **Start-up logo** with a rainbow effect. Put your own logo in
  `~/.config/curs-access/logo.txt` (see `doc/logo-example.txt`).

## Building

You need a C compiler, ncursesw, and either libsodium (preferred) or
OpenSSL 3.

On Debian:

    sudo apt install build-essential pkg-config libncurses-dev libsodium-dev

From a Git checkout, generate the build system first. This needs autoconf,
automake and pkg-config:

    sudo apt install autoconf automake
    ./autogen.sh

Then, from a Git checkout or a release tarball:

    ./configure
    make
    make check
    sudo make install

`make install` puts `curs-access` in `/usr/local/bin` and the manual page
in section 1. Run `curs-access` in a terminal, or `man curs-access`.

`configure --with-crypto=sodium|openssl|auto` picks the encryption library.
The default, `auto`, uses libsodium when it is found and OpenSSL 3 otherwise;
the summary at the end of `configure` says which one it chose. The two
libraries write different hash formats. If you switch builds, run
`curs-access --reset-security`.

## Keys

| Key | Action |
|-----|--------|
| A–Z | Run the entry or open the sub-menu |
| ↑ ↓ Home End PgUp PgDn, Enter | Move the bar and choose |
| Esc | Previous menu, cancel, or leave a screen |
| F1 | Menu Maintenance |
| F3 | System Info |
| F5 | Show the logo |
| F10 | Exit (from a sub-menu: back to the main menu) |
| F4 | At a password prompt: use the recovery code |

If the terminal or window manager takes the function keys for itself, press
**Esc** and then a digit: Esc 1 … Esc 9 and Esc 0 act as F1 … F10.

In the Create / Modify screen:

| Key | Action |
|-----|--------|
| F1 | Insert |
| F2 | Delete |
| F3 | Move |
| F5 | Custom Title |
| F7 | Tree |
| F8 | Options (pause after the program, entry password) |
| F9 | Sub-Menu |
| Esc | Back |
| F10 | Back to the Maintenance menu |

Typing on a cell replaces its text, and Enter edits the text already there.

## Files

Everything is kept in your home directory. Files are created with mode
0600 and directories with 0700.

| File | Contents |
|------|----------|
| `~/.config/curs-access/curs-access.conf` | Program Configuration and colors |
| `~/.config/curs-access/menus.conf` | The menus (tab-separated, one entry per line) |
| `~/.config/curs-access/security.conf` | Password hashes, recovery hash, usage-log keys |
| `~/.config/curs-access/themes/*.theme` | Your saved themes |
| `~/.config/curs-access/logo.txt` | Optional custom logo |
| `~/.local/state/curs-access/usage.log` | Encrypted usage log |
| `~/.local/state/curs-access/debug.log` | Debug log (only with `--debug`) |

`$XDG_CONFIG_HOME` and `$XDG_STATE_HOME` are respected.

## A note on security

The passwords work like the original program's: they keep people at a
shared terminal out of maintenance and stop them leaving the menu. They are
not a sandbox. Anyone who can run a shell as the same user can still edit the
files above, for example by deleting `security.conf`. Doing that does **not**
let them read the old usage log, because its secret key is stored only in
encrypted form.

If you lose both the Maintenance password and the recovery code, run
`curs-access --reset-security`. It starts over with no passwords and a new
recovery code, and it deletes the old usage log.

## Source layout

| File | Purpose |
|------|---------|
| `src/main.c` | Options and start-up order |
| `src/ui.c`, `ui.h` | The screen kit: frame, clock, boxes, key bar, dialogs, field editor, viewer |
| `src/theme.c`, `theme.h` | The sixteen colors, color slots, built-in and saved themes |
| `src/mainmenu.c` | The lettered menu screen and the main menu loop |
| `src/maint.c` | Menu Maintenance: Create / Modify editor, tree, Usage Report, Passwords & Recovery |
| `src/cfgscreen.c` | Program Configuration and Color Themes screens |
| `src/splash.c` | Start-up logo, System Info, recovery code screen |
| `src/launch.c` | Running a menu entry as a foreground job |
| `src/access.c` | Exit / Maintenance password checks and the recovery code prompt |
| `src/signals.c`, `signals.h` | Ignoring ^C, ^\, ^Z and SIGTERM; exiting on SIGHUP |
| `src/menu.c`, `menu.h` | The menu tree and `menus.conf` |
| `src/prefs.c`, `prefs.h` | Program Configuration file |
| `src/security.c`, `security.h` | Password hashes, recovery code, usage log key |
| `src/usage.c`, `usage.h` | Encrypted usage log |
| `src/crypto.c`, `crypto.h` | libsodium and OpenSSL 3 back ends |
| `src/util.c`, `util.h` | Strings, paths, base64/base32, debug log |
| `tests/` | `make check` tests (run in a temporary directory) |
| `doc/DESIGN.md` | The design system the screens follow |
| `doc/curs-access.1` | Manual page |

The header files describe each module's functions. `doc/DESIGN.md`
explains the layout, color and key rules, for anyone adding a screen. Release
notes are in `NEWS.md`.

## License

Copyright (C) 2026 Lee Curtis. Released under the GNU General Public License,
version 3 or later; see `COPYING`.

Curses Access is an independent program. It contains no code, artwork or
text from any earlier commercial menu system.

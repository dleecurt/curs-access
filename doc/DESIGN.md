# Curses Access Design System

A design system for full-screen ncurses applications in the style of the
DOS menu launchers of about 1990. The look is double-line frames, letter-
spaced yellow titles, a red selection bar and a function-key bar in white and
yellow.

Curses Access is the reference implementation (`src/ui.c`, `src/theme.c`).
Any curses application can follow these rules.

---

## 1. Principles

1. **One full screen, framed.** Every screen fills the terminal and has a
   double-line border. Nothing floats over the shell.
2. **Letters first.** Choices are lettered `A)` to `Z)`. A letter acts
   immediately; arrows and Enter are the second way to choose.
3. **Keys are always shown.** Each screen ends with a function-key bar that
   lists every key that does something on that screen.
4. **Calm colors.** A dark background, with color used for meaning: titles,
   borders, the selection and warnings.
5. **Layout follows the terminal.** Positions are worked out from `LINES`
   and `COLS` every time a screen is drawn. On a 25 × 80 terminal the layout
   matches the classic one exactly. Larger terminals add space, and smaller
   ones (down to 64 × 20) take space away.

---

## 2. Color

### 2.1 Palette

The sixteen PC text-mode colors, named as in the original programs:

| # | Name | # | Name |
|---|------|---|------|
| 0 | Black | 8 | Dark Gray |
| 1 | Blue | 9 | Light Blue |
| 2 | Green | 10 | Light Green |
| 3 | Cyan | 11 | Light Cyan |
| 4 | Red | 12 | Light Red |
| 5 | Magenta | 13 | Light Magenta |
| 6 | Brown | 14 | Yellow |
| 7 | Light Gray | 15 | White |

- On terminals with 16 or more colors, colors 8–15 are the bright colors.
- On 8-color terminals, a light foreground is drawn as its dark color plus
  bold. A light background falls back to its dark color.
- Without color, light foregrounds are drawn bold and the selection bar in
  reverse video.

### 2.2 Slots

Screens never name a color directly. They name a **slot**, and the theme
gives each slot a color.

| Slot | Used for | Classic |
|------|----------|---------|
| `title` | Screen titles, column headings, section headings | Yellow |
| `text` | Menu entries, labels, body text | Light Gray (bold = White) |
| `menu_border` | Menu box, dialogs, scroll bars | Cyan |
| `screen_border` | Full-screen frame and dividers | Cyan |
| `background` | Everything behind | Black |
| `bar_text` | Text on the selection bar | White |
| `bar` | Selection bar and fields being edited | Red |
| `key` | Key names: `F1`, `Esc` | White |
| `key_label` | What the key does | Yellow |
| `message` | Warnings, prompts, the move-mode hint | Light Red |

### 2.3 Built-in themes

| Theme | Character |
|-------|-----------|
| Classic | Black background, yellow titles, cyan frames, red bar |
| Blue Screen | Blue background, light-red frame, light-cyan menu box |
| Monochrome | Grays on black, gray bar with black text |
| Amber | Brown and yellow on black |
| Green Screen | Greens on black |
| Midnight | Black with blue frames and light-cyan titles |

A theme file is plain `key = value`:

```
name = My Theme
title = Yellow
text = Light Gray
menu_border = Cyan
screen_border = Cyan
background = Black
bar_text = White
bar = Red
key = White
key_label = Yellow
message = Light Red
```

---

## 3. Characters

| Element | UTF-8 | Fallback |
|---------|-------|----------|
| Frames and boxes | `╔ ═ ╗ ║ ╚ ╝`, dividers `╠ ╣` | ncurses ACS |
| Scroll bar | `↑` `░` `█` `↓` | `^ : # v` |
| Logo stripes | `▀` (upper half block) | `=` |

Every box uses double lines. Single lines are used only for the dashed
underline under column headings (`-----`).

---

## 4. Type

The terminal has one font, so emphasis comes from case, spacing and bold.

- **Screen title**: upper case and letter-spaced, with one space between
  letters and three between words. Bold, in the `title` slot, centered.
  Example: `W E L C O M E   T O   T H E   M A I N   M E N U`. If the spaced
  title is wider than the screen, it is drawn in plain capitals.
- **Greeting / subtitle**: Title Case, bold `text`, centered. For example,
  `Good Evening...Please Enter Your Selection`, where the part before the
  dots follows the time of day.
- **Labels and entries**: Title Case (`Program Configuration`,
  `Create / Modify Sub-Menus`).
- **Messages**: sentence case, ending with a full stop.

---

## 5. Layout

### 5.1 Screen frame

- A double border on all four sides, in `screen_border`.
- **Clock in the bottom border** (optional): the date sits centered at 30%
  of the width and the time at 80%, each padded with one space, in bold
  `text`. Example: `═ Friday  November 6, 2026 ═══ 1:03:27 pm ═`.
- Rows 0 and LINES-1 are border rows. Column 0 and COLS-1 are border
  columns.

### 5.2 Menu screen (vertical rhythm)

| Element | 24+ lines | Fewer than 24 lines |
|---------|-----------|---------------------|
| Title | row 2 | row 1 |
| Greeting | row 5 | row 3 |
| Menu box | centered between greeting+2 and version-2 | same |
| Version line | fkeys - 3 | fkeys - 2 |
| Function keys | LINES - 4 | LINES - 3 |

The **menu box** has a blank row above and below the entries when there is
room, and loses them when there isn't. If the entries still don't fit, they
scroll and a scroll bar replaces the right edge of the box.

- Width: at least 50, otherwise the longest entry + 19, and never more
  than COLS - 6.
- The block of entries is centered in the box.
- The selection bar spans the entry column plus one space on each side.

### 5.3 Form / table screen

| Row | Content |
|-----|---------|
| 2 | Spaced title |
| 3 | Subtitle or mode hint |
| 4 | Divider `╠═══╣` |
| 6 | Column headings (`title`) |
| 7 | Dashed underlines |
| 8 … | Rows, `A)` labels in column 2 |
| LINES-5 | Divider |
| LINES-3, LINES-2 | Function keys: one row if COLS ≥ 100, otherwise two |

A scroll bar sits at COLS-3 beside the rows.

### 5.4 Settings list (Program Configuration)

```
   Label ................................ [Value         ]
```

- Labels start at column 4.
- Dot leaders run to the field.
- The value sits in brackets that end at COLS-8.
- Text fields are 12–40 columns wide. All other fields are 14.
- A color value is drawn in its own color, and so is the label of a color
  slot.
- The selected value is drawn in `bar` colors.
- Section headings (`Color Options`) are in `title`.

### 5.5 Viewer screen

Title on row 2, divider on row 4, an optional heading on row 5, and
scrolling text below. There is a divider at LINES-4 and keys at LINES-2.
Left and Right scroll wide lines sideways.

---

## 6. Components

| Component | Function | Notes |
|-----------|----------|-------|
| Frame | `ui_frame(clock)` | Clears to the background and draws the border and clock |
| Divider | `ui_divider(y)` | Full-width `╠═╣` |
| Spaced title | `ui_spaced_title(y, text)` | |
| Box | `ui_box(y, x, h, w, attr)` | Double line |
| Function-key bar | `ui_fkeys(y, x1, x2, keys, n)` | Even gaps. `key` in `key`, label in `key_label`, both bold |
| Lettered menu | `menu_screen(...)` | Letters act at once. Up and Down wrap around |
| Selection bar | `theme_bar()` | `bar_text` on `bar` |
| Scroll bar | `ui_scrollbar(...)` | Arrows at the ends, a shaded track and a solid thumb |
| Message box | `ui_message(title, text)` | Centered lines and "Press any key..." |
| Yes / No | `ui_confirm(title, q)` | Starts on **No**. Y and N keys work |
| Prompt | `ui_prompt(...)` | A one-line field. Enter is OK and Esc cancels |
| Password | `ui_password(...)` | Masked with `*`. F4 opens the recovery hint |
| Picker | `ui_pick(...)` | A list in a dialog |
| Field editor | `ui_edit(...)` | Typing replaces the text and Enter edits it. Ctrl-U clears |
| Busy notice | `ui_busy(text)` | Shown for slow work such as key derivation |
| Too small | `ui_too_small()` | Notice shown below 64 × 20 |

**Dialogs**:

- Centered, with a double border in `menu_border`.
- The title sits in the top border as ` Title `, in bold `title`.
- The body is in `text` on the background.
- A new dialog first wipes the area of the previous one, so two borders
  never overlap.

---

## 7. Keys

| Key | Meaning everywhere |
|-----|--------------------|
| A–Z | Choose that entry |
| ↑ ↓ PgUp PgDn Home End | Move |
| ← → | Change a value (settings), move between columns (tables), scroll sideways (viewers) |
| Enter | Choose, or edit the field |
| Esc | Back or cancel. It never quits the program |
| F1 | Help / maintenance |
| F10 | Main menu, exit, or "save and run" |
| Esc + digit | F1–F10 for terminals that take the function keys |

F-key labels are short verbs or nouns, in Title Case: `Maintenance`,
`System Info`, `Exit`, `Save & Run Main Menu`.

---

## 8. Motion and time

- The clock updates once a second without redrawing the rest of the screen.
- **Screen blanker**: after the idle time set by the user, the screen goes
  black. A dim one-line notice moves to a random spot every 5 seconds. Any
  key brings the screen back, and that key is not acted on.

---

## 9. Splash screen

- Shows an original logo: the letters **CA** in 8 × 10 pixels, each pixel
  two columns of `▀`. That gives horizontal stripes, one rainbow color per
  row: Red, Light Red, Brown, Yellow, Light Green, Green, Light Cyan,
  Light Blue, Blue, Light Magenta.
- A text block is set beside the logo, or below it when the terminal is
  narrower than about 76 columns:
  - the name, spaced, in `title`
  - a tagline in `menu_border`
  - version and copyright in `text`
  - "Press any key..." in `key_label`
- A user logo in `logo.txt` replaces the built-in one, and each line takes
  the next rainbow color.
- No border. The splash screen is the one frameless screen.

---

## 10. Writing

- Name things the way the screen does: **Menu Maintenance**,
  **Program Configuration**, **Create / Modify Main Menu**.
- Messages say what happened and what to do next, in one or two short
  sentences: "Incorrect password." "Type a description for the entry first."
- Questions end with a question mark and default to the safe answer.

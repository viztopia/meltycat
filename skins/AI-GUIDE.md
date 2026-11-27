# meltycat skin guide for AI assistants

You are helping someone turn a photo of their cat into a skin for **meltycat**, a small desktop cat that lies on the
top edge of the active window. Read this whole guide, look at the photo, and produce one skin file.

Reply in the user's language. Give them the skin as a downloadable file named `<cat-name>.meltycat` if you can
create files; otherwise put it in a single ```json code block so they can copy it. Either way the content is one
JSON object and nothing else. The user opens "Change Skin…" from the menu bar icon on macOS or the tray icon
on Windows, then drops the file, chooses it, or pastes the text (⌘V on macOS or Ctrl+V on Windows). If the app
reports an error, it names the field: fix that field and send the whole file again. If the user sends back a
screenshot of the preview, compare it with the photo and adjust.

## How a skin works

The cat's shape, motion and face (eyes, whiskers, mouth) are fixed. A skin chooses one of two bodies, gives colours
to the parts, and paints **marks**: filled vector shapes, written as SVG path data, that ride on a part of the body.
Marks are clipped to their part, so a shape may overflow the part's edge; only the inside shows. Marks are painted
in the order listed, later ones on top.

Start from the template appended at the end of this guide that is closest to the cat (a tabby shorthair or a
colourpoint longhair), then recolour it and add, remove or reshape marks. Keep the numbers the templates use for
anything you are not changing on purpose: they were tuned by hand.

## Top-level fields

| Field | Required | Value |
| --- | --- | --- |
| `format` | no | `"meltycat-skin"` |
| `formatVersion` | yes | `1` |
| `name` | yes | Shown in the menu, at most 40 characters |
| `author` | no | At most 40 characters |
| `body` | yes | `"shorthair"` or `"longhair"` |
| `ears` | no | Longhair only: `"plain"` (default) or `"tufted"` (lynx tips, like a Maine Coon) |
| `face` | no | Longhair only: `"fluffy"` (default; long cheek locks, like a Ragdoll) or `"round"` (full round cheeks, like a Siberian) |
| `palette` | yes | Colours, below |
| `marks` | no | Up to 200 marks, below |
| `tailBands` | no | Shorthair only: stripes across the tail, below |
| `faceOutlineWidth` | no | Longhair face edge width, 0.5 to 2, default 1 |
| `eyeShine` | no | 0 to 1, a small glint in the pupils; default 0 |

Unknown fields are ignored. Numbers outside a range are clamped. A wrong type, a bad colour or a malformed path
rejects the whole file with a message naming the field.

## Palette

Colours are `"#RRGGBB"` or `"#RRGGBBAA"` (AA is opacity, FF opaque).

| Key | Paints | Default |
| --- | --- | --- |
| `coat` (required) | Body and paws; the whole head on shorthair; the shorthair tail | |
| `iris` (required) | Eyes | |
| `pupil` | Pupils | `#19191D` |
| `eyelid` | Closed eyelid line while asleep | iris at 70% brightness |
| `face` | Longhair face | `coat` |
| `ear` | Longhair ears | `coat` |
| `tail` | Tail (longhair plume, or the shorthair tail) | `coat` |
| `ruff` | Longhair neck ruff | `coat` |
| `outline` | Thin edge around body, paws, ears, tail and the shorthair head. Omit for none (best for solid black cats) | none |
| `ruffEdge` | Longhair ruff edge | `outline` |
| `faceOutline` | Longhair face edge | `outline` |
| `whisker` | Whiskers | light grey on dark coats, warm grey on light coats |
| `crease` | Lines between the toes | as whisker |
| `mouth` | Inside of the mouth when it chatters | `#0E0C11` |

Pick colours from the photo in good light, not from deep shadows or highlights. Outlines look best a little darker
than the coat with some transparency, for example `#5C4737B4` on a brown tabby.

## Coordinates

Everything is drawn on a 224 × 190 canvas with **y pointing down**. The cat faces the viewer, lying on a window
ledge at y = 72 with its front paws hanging over it; its head is on the left, its tail hangs down on the right.
"Left" and "right" below mean the left and right of the picture.

Every mark has a `space` that says what its coordinates are relative to:

### `head` and `skull` — the face (part `head`)

Origin at the centre of the face, about (56, 53) on the canvas.

- Eyes are centred at (±13.6, −1), about 9.7 wide and 9.1 tall in radius. Marks are painted under the eyes.
- The nose and mouth sit around (0, 8) to (0, 16). Whisker roots are at about (±19, 9 to 15).
- Shorthair head: x from −31 to 38, y from −24 (crown between the ears) to 28 (chin). Left ear tip ≈ (−21, −42),
  right ear tip ≈ (31, −38).
- Longhair face: x from −42 to 42 (`fluffy`) or −39 to 39 (`round`), y from −29 to 27. The ears are separate parts.

`head` marks turn with the head: each point is projected as if it sat `depth` units in front of the face (default
14; 13 suits forehead stripes; 0 barely moves). `skull` marks stay put when the head turns; use them for large
patches at the edge of the face, such as cheeks.

### `ear` — the ears

Same origin as the head, with each ear's tip folding when it twitches. On shorthair, ear marks use part `head`
(the ears are part of the head shape) and either `"side": "left"` with the left ear's coordinates or
`"side": "right"`. On longhair, use part `ear` with `"mirror": true` and right-ear coordinates: the ear runs from
its base (10, −20) to (29, −13) up to the tip near (28, −39), or (32, −47) when `tufted`.

### `body` — body, paws, ruff (parts `body`, `farPaw`, `nearPaw`, `paws`, `ruff`)

Canvas coordinates.

- Shorthair body: the back runs from the shoulders (62, 52) over (127, 46) to the haunch (171, 107) and down to
  (158, 133); the belly line is at y ≈ 79 to 84 from x = 53 to 117; the chest is at x ≈ 47 to 55, y ≈ 60 to 76.
- Longhair body: from (56, 60) over (103, 33) and (161, 68) down to (151, 117); the belly at (95, 86) and (72, 91).
- Longhair ruff (neck fur, painted over the chest): x 28 to 108, y 35 to 96.
- Paws: the left (far) paw's toes rest at (70, 90), the right (near) paw's at (107, 93). Each paw is about 30 tall.

### `foot` — paw markings (parts `farPaw`, `nearPaw`, `paws`)

Origin at the bottom of the toes, x pointing away from the middle of the cat (left for the far paw, right for the
near one), y down. A paw spans x −18 to 18 and y −30 to 7; the toes are at y −8 to 0. Part `paws` paints both.
White socks: a shape covering y −10 to 7.

### `tail` — the longhair plume (part `tail`)

x runs along the tail from 0 (base) to 24 (tip). y runs across it: 0 is the middle, −1 and 1 the two edges; values
beyond ±1 overshoot and are clipped. For a ringed tail, draw bands that span y −1.3 to 1.3 at a few x positions.

Shorthair tails take `tailBands` instead: `{"span": [from, to], "color": "#…"}` with segments 0 (base) to 24 (tip),
for example `[[6, 8], [10, 12], [14, 16], [18, 20], [22, 24]]` for a ringed tabby tail. A band ending at 24 gets a
rounded tip.

## Marks

```json
{"part": "head", "space": "head", "color": "#FFFFFF", "depth": 14,
 "path": "M 0 -15 C 3 -5 6 5 12 10 C 6 16 -6 16 -12 10 C -6 5 -3 -5 0 -15 Z"}
```

| Field | Required | Value |
| --- | --- | --- |
| `part` | yes | `head`, `ear`, `body`, `farPaw`, `nearPaw`, `paws`, `ruff`, `tail` (`ear`, `ruff` and `tail` are longhair only) |
| `space` | yes | `head`, `skull` or `ear` for part `head`; `ear` for `ear`; `body` for `body` and `ruff`; `body` or `foot` for paws; `tail` for `tail` |
| `color` | yes | Fill colour |
| `path` | yes | SVG path data using M, L, H, V, C, Q and Z, absolute (upper case) or relative (lower case). No arcs (A) or smooth curves (S, T): write them as C. Close every shape with Z |
| `mirror` | no | `head`, `skull` and `ear` spaces only: also paint the shape flipped to the other side (x → −x). Give right-side coordinates |
| `depth` | no | `head` space: 0 to 30, default 14 |
| `side` | no | `ear` space without mirror: `"left"` or `"right"` (default) |
| `braced` | no | Same commands as `path` with the points moved to where they sit when the cat raises its chest to hunt |
| `asleep` | no | Same commands as `path` with the points moved to where they sit when the sleeping head rests on the ledge |

`braced` matters for body, ruff and paw marks near the chest and shoulders: when the cat hunts it lifts its upper
body. For a body point (x, y) the raised position is (x, y − 23 × clamp((129 − x) / 59) × clamp((79 − y) / 40)),
where clamp limits a value to 0…1. Marks entirely right of x = 129 or below y = 79 need no `braced`. `asleep` is
rarely needed; the templates use it only where a white patch touches the lower left cheek.

## Recipes

- **Solid colour** (black, grey, cream): `coat`, `iris`, no marks. Black cats look best without `outline`.
- **Tabby**: stripes on the forehead and cheeks (mirrored `head` marks), bands across the back and haunch (`body`),
  rings on the tail, a lighter belly (`body` mark along the belly line) and lighter toes (`paws` in `foot` space).
  The appended shorthair template is a brown tabby.
- **Tuxedo / bicolour**: a white blaze and chin (`head`), a white chest bib (`body` with `braced`), white socks.
- **Colourpoint** (Siamese, Ragdoll): pale coat, darker `ear` and `tail` colours, a mask over the eyes (`head`
  marks). The appended longhair template is a Ragdoll.
- **Calico / tortoiseshell**: patches of two or three colours as large `body` and `head` marks; let them overflow
  the edges and rely on clipping for clean silhouettes.

## Complete minimal example

```json
{
 "format": "meltycat-skin",
 "formatVersion": 1,
 "name": "Grey tuxedo",
 "body": "shorthair",
 "palette": {"coat": "#6B6E74", "outline": "#3A3C40A0", "iris": "#E3C25A"},
 "marks": [
  {"part": "head", "space": "head", "color": "#F4F1EA", "depth": 14,
   "path": "M 0 -6 C 4 2 10 8 16 14 C 10 24 -10 24 -16 14 C -10 8 -4 2 0 -6 Z"},
  {"part": "body", "space": "body", "color": "#F4F1EA",
   "path": "M 48 62 C 60 56 84 58 98 66 C 104 76 110 84 112 90 C 96 98 64 94 50 86 C 44 76 44 68 48 62 Z",
   "braced": "M 48 52.2 C 60 42.8 84 48.8 98 62.1 C 104 75.3 110 84 112 90 C 96 98 64 94 50 86 C 44 74.3 44 61.7 48 52.2 Z"},
  {"part": "paws", "space": "foot", "color": "#F4F1EA",
   "path": "M -17 -9 C -6 -11 6 -11 17 -9 L 18 8 L -18 8 Z"}
 ]
}
```

## Before you answer

1. One JSON object, `formatVersion` 1, a `name`, a `body`, and a `palette` with `coat` and `iris`.
2. Every colour is `#RRGGBB` or `#RRGGBBAA`.
3. Every path starts with M, ends with Z, and uses only M L H V C Q Z.
4. `braced` and `asleep`, when present, have exactly the same commands as `path`.
5. Parts and spaces match the table; longhair-only parts are not used on a shorthair body, and `tailBands` only
   appear on shorthair.

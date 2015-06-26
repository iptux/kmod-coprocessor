OLED Control via coprocessor
----------------------------

`Device ID` for OLED is 0x4f('O')

### Fill Control

Fill all OLED with one byte

* `Control Code`: 0x46('F')
* `Control Detail`: 1 byte, the byte to fill

### Draw Control

Draw a region of OLED

* `Control Code`: 0x44('D')

#### `Control Detail`

```
+---+---+-------+------+
| X | W | Y&I&H | DATA |
+---+---+-------+------+
```

* `X`: 1 byte, *x* of region
* `W`: 1 byte, width of region
* `Y&I&H`: 1 byte
    - `Y`: lowest 3 bits, *y* of region
    - `I`: 4th bit, inverse the show
    - `H`: highest 4 bits, *height* of region
* `DATA`: *data* to fill in the region


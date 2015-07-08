Battery Status via coprocessor
------------------------------

`Device ID` for Battery is 0x42('B')

### Capacity Read

`Control Code`: 0x43('C')

#### Response

```
+----------+
| Capacity |
+----------+
```

* `Capacity`: 1 byte, in percent

### Get Battery Status

`Control Code`: 0x53('S')

#### Response

```
+--------+
| Status |
+--------+
```

* `Status`: 1 byte
    - 0x00: unknown
    - 0x01: charging
    - 0x02: discharging
    - 0x03: not changing
    - 0x04: charging full
    - 0x05: battery not present


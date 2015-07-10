GPIO Control via coprocessor
----------------------------

`Device ID` for GPIO is 0x47('G')

### GPIO Get

`Control Code`: 0x72('r')

#### Request

```
+-------------+
| GPIO Number |
+-------------+
```

`GPIO Number`: 1 byte

#### Response

```
+------------+
| GPIO Value |
+------------+
```

`GPIO Value`: 0 for low, 1 for high

### GPIO Set

`Control Code`: 0x68('h') for set to high, 0x6c('l') for set to low

#### Request

```
+-------------+
| GPIO Number |
+-------------+
```

`GPIO Number`: 1 byte

### GPIO Value Change Report

Send from coprocessor to primary processor,
report a value change event of a GPIO.

`Control Code`: 0x75('u') for change from low to high,
  0x64('d') for change from high to low

```
+-------------+
| GPIO Number |
+-------------+
```

`GPIO Number`: 1 byte

### GPIO Get Direction

`Control Code`: 0x65('e')

#### Request

```
+-------------+
| GPIO Number |
+-------------+
```

`GPIO Number`: 1 byte

#### Response

```
+-----------+
| Direction |
+-----------+
```

`GPIO Value`: 0 for out, 1 for in

### GPIO Set Direction

`Control Code`: 0x69('i') for set to input, 0x6f('o') for set to output

#### Request

```
+-------------+
| GPIO Number |
+-------------+
```

`GPIO Number`: 1 byte


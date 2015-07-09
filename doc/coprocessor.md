Use MCU as Coprocessor
======================

```
+-----------+               +------+
|  Primary  |  Serial Line  | MCU  |
| Processor |<------------->|      |
+-----------+               +------+
```


Protocol
--------

```
+--------+----------+------------------+-----------------+--------------+
| Length | Identity | Message Checksum | Header Checksum | Message Body |
+--------+----------+------------------+-----------------+--------------+
```

### `Length` Field

* 1 byte
* length of `Message Body`
* maxium value 250

### `Identity` Field

* 1 byte
* valid values
    - 0x70('p'): ping request, no `Message Body`
    - 0x61('a'): ping ack, no `Message Body`
    - 0x71('q'): control request
    - 0x72('r'): control response
* all other value should be ignored

### `Message Checksum` Field

* 1 byte
* checksum of `Message Body` field
* if `Message Body` is absent, this field should be 0xff

### `Header Checksum` Field

* 1 byte
* checksum of `Length`, `Identity` and `Message Checksum` field

### Checksum Algorithm

```
function checksum(bytes[])
	sum = 0
	for byte in bytes
		sum = sum + byte
	return sum & 0xff
```

### XOR Encryption

Before sending to `Serial Line`,
every byte should be XOR with `0xd8`.

After receiving from `Serial Line`,
every byte should be XOR with `0xd8` before futher processing.

### Control Request

```
+-----------+--------------+----------------+
| Device ID | Control Code | Control Detail |
+-----------+--------------+----------------+
```

* `Device ID`: 1 byte
* `Control Code`: 1 byte
* `Control Detail`: depends on specific device

`Device ID` value `0xf0 ~ 0xff` is reserved for special usage.

### Control Response

After the *Control Action* is finished,
a `Control Message` with `Control Detail` field replaced by `Response Detail` field
should be send back as a *control response*.

```
+-----------+--------------+-----------------+
| Device ID | Control Code | Response Detail |
+-----------+--------------+-----------------+
```

* `Device ID`: 1 byte, same as `Device ID` in `Control Request`
* `Control Code`: 1 byte, same as `Control Code` in `Control Request`
* `Response Detail`: depends on specific device

### Control Error Reponse

If an error occured, a *Control Error Response* should be send.

```
+----------+------------+
| Error ID | Error Code |
+----------+------------+
```

* `Error ID`: 1 byte, 0xf0
* `Error Code`: 1 byte, `0xf0 ~ 0xff` is reserved
    - 0xf0: invalid `Device ID`
    - 0xf1: invalid `Control Code`


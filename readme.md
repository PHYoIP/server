# PHYoIP Server

> [!IMPORTANT]
> Work in progress! "Usage:" and "Examples:" are more like a specification atm.

If a serial port is specified, the server acts as a gateway server (= server with integrated gateway client).

Usage:
```
phyoip-server PORT [SPCFG DEV BAUD [CONFIG]]
phyoip-server --help
```

Examples:
```sh
phyoip-server 1234
phyoip-server 1234 A,0D0A /dev/ttyS4 115200
phyoip-server 1234 B,1500 COM7 19200
phyoip-server 1234 A,0A,24 COM3 9600 8E2
```

### Serial Protocol Config (SPCFG)
Comma separated list of parameters. The first parameter specifies the protocol type.

#### `A` ASCII Protocol
ASCII protocols use one or two stop symbols, and optionally one start symbol: `A,STOP[,START]`.

|       Symbol       | Description                                     |
|:------------------:|:------------------------------------------------|
| `xx`,<br/>`xxxx` | `x` has to be a hexadecimal digit               |
|        `LF`        | Line Feed, 0A<sub>h</sub>, 10<sub>d</sub>       |
|        `CR`        | Carriage Return, 0D<sub>h</sub>, 13<sub>d</sub> |
|       `CRLF`       | `CR` and `LF`, 0D0A<sub>h</sub>                 |
|       `NULL`       | Null terminator, 00<sub>h</sub>, 0<sub>d</sub>  |

#### `B` Binary Protocol
`B,TO[,LEN]`
- `TO` timout as microseconds
- `LEN` byte offset of the length specifier of the message

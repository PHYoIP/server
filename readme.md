# PHYoIP Server

> [!IMPORTANT]
> Work in progress! "Usage:" and "Examples:" are more like a specification atm.

If a serial port is specified, the server acts as a gateway server (= server with integrated gateway client).

Usage:
```
phyoip-server PORT [DEV BAUD [CONFIG]]
phyoip-server --version
phyoip-server --help
```

Examples:
```sh
phyoip-server 1234
phyoip-server 1234 /dev/ttyS4 115200
phyoip-server 1234 COM7 19200
phyoip-server 1234 COM3 9600 8E2
```

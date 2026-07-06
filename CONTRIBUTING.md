# Contributing

Tests are written in C with a Go test harness that runs on the host machine and communicates with the Ultimate hardware over TCP/IP. See [tests](tests) for the test code.

Development of `libuci` library requires:

* Ultimate hardware
* [cc65](https://cc65.github.io/)
* [oscar64](https://github.com/drmortalwombat/oscar64)
* Go

Run all tests with one command:

```bash
CC=cl65 make test -j
CC=oscar64 make test -j
```

To set the Ultimate hardware address:

```bash
export C64U_ADDRESS=192.168.1.150  # default "c64u"
```

To list all available test names:

```bash
make list-tests
```

To run individual tests:

```bash
make test TEST_RUN=TestDOS/ListDir
make test TEST_RUN=TestNet/TcpEcho
make test TEST_RUN=TestCore    # run entire suite
```

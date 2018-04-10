# Pi Exchange

[![Build Status](https://travis-ci.org/SEIAROTg/pi-exchange.svg?branch=master)](https://travis-ci.org/SEIAROTg/pi-exchange)
[![Coverage Status](https://coveralls.io/repos/github/SEIAROTg/pi-exchange/badge.svg?branch=master)](https://coveralls.io/github/SEIAROTg/pi-exchange?branch=master)

Playground of optimization experiments, targeting a simple exchange server on Raspberry Pi 3B.


### Build

```sh
$ git clone https://github.com/SEIAROTg/pi-exchange.git
$ cd pi-exchange
$ mkdir -p build/release
$ cd build/release
$ cmake -DCMAKE_BUILD_TYPE=Release ../..
$ make
```

### Run

```sh
# listen at 127.0.0.1:3000
$ ./exchange 3000 127.0.0.1
```

### Test

```sh
$ make tests
$ ./tests
```

### Benchmark

Build and run `exchange` first

```sh
$ make benchmark
# generate 5,000,000 requests with expected order book size 100,000
$ ./benchmark generator file 100000 5000000
# feed 5,000,000 requests to server
$ ./benchmark file 127.0.0.1:3000 0 5000000
```

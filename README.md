# io_uring echo server

code for test with io_uring 


## Build

```
$ make
```

## Test

server

```
./bin/io_uring2

```

client

```
python3 echo-client.py
```

## Reference

- [Why you should use io_uring for network I/O](https://developers.redhat.com/articles/2023/04/12/why-you-should-use-iouring-network-io#:~:text=io_uring%20is%20an%20asynchronous%20I,O%20requests%20to%20the%20kernel.)
- [Donald Hunter-io_uring_playground](https://github.com/donaldh/io_uring_playground) 

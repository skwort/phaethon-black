# Data Serialisation
Protocol buffers are used to serialise data for transfer between devices.

## Setup
To compile for Python, ensure you have `protoc` installed. Then run the
following command:
```
protoc --python_out=./pi/ ./shared/phaethon.proto
```

For use in Javascript, `protobuf.js` is available.

For Zephyr platforms, include the adsb.proto file in the `src` tree and
enable `CONFIG_NANOPB`.
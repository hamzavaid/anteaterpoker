# Anteater Poker Installation

## System Requirements

- EECS Linux host or compatible Unix-like environment
- GCC
- `make`
- GTK 3 development libraries
- `tar` or `gtar`

## User Package

Unpack the user package:

```sh
gtar xvzf Poker_V1.0.tar.gz
cd Poker_V1.0
```

Run the server:

```sh
./bin/poker_server --port 10010 --table "ZotHouse" &
```

If you'd like to add bots, run the server with the `--bots` option alongside the number of bots you want to add:

```sh
./bin/poker_server --port 10010 --table "ZotHouse" --bots 2 &
```

Run a client:

```sh
./bin/poker_client --host server_name_or_ip --port 10010 --name Player &
```

Replace server_name_or_ip with the actual host name or IP address of the server, for example `localhost`.

## Source Package

Unpack and build from source:

```sh
gtar xvzf Poker_V1.0_src.tar.gz
cd Poker_V1.0_src
make
```

Run tests:

```sh
make test
make test-gui
```

Create submission archives:

```sh
make tar
```

Clean generated files:

```sh
make clean
```

Uninstall by deleting the unpacked `poker` directory.

## More Information

User documentation is provided in:

```text
doc/Poker_UserManual.pdf
```

Developer and design documentation is provided in:

```text
doc/Poker_SoftwareSpec.pdf
```

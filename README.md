# raspi evse everest

# Building

Prerequesits:

```
sudo apt install -y python3-pip python3-venv git rsync wget cmake doxygen   graphviz build-essential clang-tidy cppcheck openjdk-21-jdk npm docker.io   docker-compose libboost-all-dev nodejs libssl-dev libsqlite3-dev   clang-format curl rfkill libpcap-dev libevent-dev pkg-config libcap-dev   libsystemd-dev
```

Build Steps:

```
mkdir build
cd build/
cmake ../EVerest-2026.02.1/
make -j4
make install
make iso15118_pip_install_dist
```

# Running

in the __build/__ folder run the following commands

## Run EV

```
source venv/bin/activate
./run-scripts/run-raspi-ev-DC.sh
```

## RUN EVSE

```
./run-scripts/run-raspi-evse-DC.sh
```

# Troubleshooting

1. Couldn't create socket error:

```
[ERRO] ev_slac:EvSlac  void module::main::ev_slacImpl::run() :: Couldn't open device eth1 for SLAC communication. Reason: Couldn't create the socket: Operation not permitted
```

Allow EV slac binary to open the interface without root.

EV:
```
pi@plcpi5:~/git/raspi-evse-everest/build sudo setcap cap_net_raw=eip dist/libexec/everest/modules/EvSlac/EvSlac
```

EVSE:
```
pi@plcpi5:~/git/raspi-evse-everest/build sudo setcap cap_net_raw=eip dist/libexec/everest/modules/EvseSlac/EvseSlac
```

2. No SLAC

Make sure the QCA is configured as either CCo 'Always' (EVSE) or CCo 'Never' (EV) by flashing the correctly configured PIB file:

```
plctool -p org.pib
cp org.pib evse.pib
cp org.pib ev.pib

# evse:
./git/open-plc-utils/slac/evse.sh evse.pib
plctool -P evse.pib local

# ev
./git/open-plc-utils/slac/pev.sh ev.pib
plctool -P ev.pib local
```

Check Configuration:

```
pi@plcpi5:~ $ plctool -I
        PIB 0-0 8836 bytes
        MAC 00:01:87:10:EF:85
        DAK 97:87:14:B1:3A:3F:67:7B:B2:E0:B6:3D:1D:74:0B:2C
        NMK 78:8D:DC:22:85:44:95:44:D8:C1:83:17:5B:D3:9E:23
        NID 9D:C5:10:CF:67:BB:04
        Security level 0
        NET Qualcomm Atheros Enabled Network
        MFG Delta Electronics Sat  5 Sep 20:29:52 CST 2026
        USR Qualcomm Atheros Enabled PEV
        CCo Never
        MDU N/A
```


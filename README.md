# DAQ
Repo with all configs and codes used for the MFE27 DAQ system

## Projects

| Folder | Description | Former branch |
|---|---|---|
| [`Accel/`](Accel/) | Accelerometer board firmware (STM32G491KC) | `MFE27-Accel` |
| [`CAN-Node/`](CAN-Node/) | Sensor CAN node firmware (STM32G491RE) + SIL tests | `MFE27-CAN-node` |
| [`RideHeight/`](RideHeight/) | Mini ride-height sensor firmware (STM32G491KC) + SIL tests | `MFE27-RideHeight` |
| [`DBC-Files/`](DBC-Files/) | CAN database (DBC) files | `MFE27-DBC-FILES` |
| [`MOTEC-Config/`](MOTEC-Config/) | MoTeC C185 and display-creator configs | `MOTEC-config` |

Each folder is a self-contained project; open/import the folder itself (e.g. in STM32CubeIDE) rather than the repo root.
CI for `CAN-Node` lives in `.github/workflows/can-node.yml` and only runs on changes under `CAN-Node/`.

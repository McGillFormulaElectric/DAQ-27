# DAQ
Repo with all configs and codes used for the MFE27 DAQ system

## Projects

| Folder | STM32 Project Name | Description | Former branch |
|---|---|---|---|
| [`Accel/`](Accel/) | Accelerometer board firmware (STM32G491KC) | `MFE27-Accel` |
| [`CAN-Node/`](CAN-Node/) | Sensor CAN node firmware (STM32G491RE) + SIL tests | `MFE27-CAN-node` |
| [`RideHeight/`](RideHeight/) | Mini ride-height sensor firmware (STM32G491KC) + SIL tests | `MFE27-RideHeight` |
| [`DBC-Files/`](DBC-Files/) | CAN database (DBC) files | `MFE27-DBC-FILES` |
| [`MOTEC-Config/`](MOTEC-Config/) | MoTeC C185 and display-creator configs | `MOTEC-config` |

Each folder is a self-contained project; open/import the folder itself (e.g. in STM32CubeIDE) rather than the repo root.
CI (`.github/workflows/firmware-build.yml`) builds the Debug configuration of `Accel`, `CAN-Node` and `RideHeight` with arm-none-eabi-gcc on every push/PR, using the settings from each project's `.cproject`. The built `.elf` files are uploaded as artifacts.

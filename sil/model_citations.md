# SIL model rule citations

The model tests use the version-locked `ads7066-reference.pdf` baseline recorded by `sil_instructions.md`.

| Model rule | Datasheet authority | Automated evidence |
|---|---|---|
| Reset `SYSTEM_STATUS=0x81`, `OPMODE_CFG=0x04` | Register-map reset values | `MODEL core` |
| WR/RD/SET/CLR opcodes and register masks | SPI command protocol and register-map field definitions | `MODEL core`, `SIL-ADC-001/002` |
| Command decode on CS rising; RD value in following frame | SPI protocol timing and register-read pipeline | `MODEL core`, `SIL-ADC-002` |
| SYSTEM_STATUS W1C behavior; CAL self-clear | SYSTEM_STATUS and GENERAL_CFG register definitions | `MODEL core`, `SIL-MAN-001/002/003` |
| CRC-8 polynomial `0x07`, initial `0xFF`, 32-clock frames | CRC sections; TI application note SBAA456 | `MODEL CRC` |
| Bad input CRC rejection and `CRCERR_IN`; valid SYSTEM_STATUS/GENERAL_CFG exceptions | CRC error-recovery rules | `MODEL CRC` |
| Valid register reads while `CRCERR_IN` is set; output CRC over the three-byte payload | CRC input/output rules | `MODEL CRC` |
| 20-bit OSR result plus four-bit channel ID | DATA_CFG/OSR output formats | `SIL-AUTO-003/004` |
| `OPMODE_CFG=0x0D` gives 96 us cycles; OSR=8 conservative completion is 814.08 us | Table 7-3 and Figure 7-3 | `SIL-AUTO-001/004` |

Reserved or unsupported addresses are logged and ignored by the model. Only registers exercised by the production firmware and catalogue are implemented; adding another production register access requires adding its mask and a cited model-conformance case first.

## FDCAN model authority

The three-buffer host model is bounded to the transmit behavior used by
`CANDriver`; it is not a complete Bosch M_CAN simulation. Its cited authority
is ST RM0440 Rev. 8, section 44.3.17 (Tx FIFO/queue operation), section 44.4.28
(`TXBRP`, Tx buffer request pending), and section 44.4.31 (`TXBTO`, Tx buffer
transmission occurred). RM0440 is not currently archived and SHA-256 locked in
the evidence directory, so `MODEL-FDCAN-EVIDENCE` remains BLOCKED; this citation
does not claim version-locked equivalence. The fixed three-element queue models
the vendored G4 HAL layout at `Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_fdcan.c`
(`SRAMCAN_TFQ_NBR (3U)`, near line 217), while `Core/Src/fdcan.c` selects
`FDCAN_TX_FIFO_OPERATION` (near line 57); it deliberately omits dedicated Tx
buffers. Both local sources are hashed into the run manifest. `MODEL-FDCAN`
tests the model without calling production code:
scripted HAL failure without register mutation, three pending buffer requests,
full-queue rejection, completion clearing
`TXBRP`, acknowledged completion setting `TXBTO`, forced test-only terminal
completion leaving `TXBTO` clear, re-request clearing stale `TXBTO`, and
free-slot selection across put-index wrap.

The driver-agnostic `VirtualCanBus`/`VirtualCanNode` adds a bounded behavioral
peer for SIL. ACK is represented as an attempt outcome, never a CAN data
message. An active peer validates and acknowledges a classic standard data
frame; listen-only and offline modes suppress ACK. With automatic retry, an ACK
or scripted protocol error leaves the request pending (`TXBRP` set and `TXBTO`
clear) while the configured finite SIL retry budget controls only how many
attempts are advanced. Exhausting that budget does not model cancellation or
successful completion. A peer may have received an otherwise valid frame even
when ACK is deliberately withheld; stuff, form, bit, and CRC outcomes do not
enter the received-data log. The configurable completion latency is a host
scheduler abstraction used to expose pending state, not a physically delayed
ACK slot.

The adapter maps bounded protocol outcomes to `LastErrorCode`. TEC/REC changes,
error-passive transitions, bus-off thresholds, bit-accurate arbitration, and
ISO-conformant retransmission timing are intentionally outside this model and
remain HIL/evidence follow-ups. The fake-only forced-terminal helper retires a
request for Tx FIFO register conformance tests; it is not the no-ACK behavior.

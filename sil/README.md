# MFE26 host SIL

This directory compiles the real SPI/ADS7066, CAN driver, `BoardManager`, and C wrapper production sources against deterministic host fakes. It remains a defect-finding SIL harness rather than a release-green campaign while its reported XFAIL/BLOCKED items remain.

The CAN harness uses a portable `VirtualCanBus`/`VirtualCanNode` model shared by
the focused driver and application fakes. Its queued, attempted, and
peer-received frame logs are distinct. Physical ACK and protocol errors are
attempt events, not DBC frames. Tests may select active, listen-only, or offline
peer modes, script stuff/form/ACK/bit/CRC outcomes, bound the number of retries
advanced, and use virtual frame-completion latency. The latter is a scheduler
abstraction rather than a delayed physical ACK. Error-confinement counters,
error-passive/bus-off thresholds, and bit-accurate arbitration remain outside
the bounded model.

Run on Windows PowerShell with:

```powershell
.\sil\run_sil.ps1
```

The ordinary invocation is the development/host-CI gate: `FAIL`, `XPASS`, process failures, and reproduced `DRIVER-IMPACTING` errors fail it, while reproduced `MINOR` errors remain visible advisory warnings. Use `-Strict` for the release gate. Strict mode fails for every surviving XFAIL and every traceability status other than `AUTOMATED`, including MINOR warnings, so it remains red until all errors, missing inputs, and unavailable-tool gates are closed.

If CMake is installed, configure `sil` and run CTest. The direct PowerShell runner exists for development machines without CMake.

Current coverage is reported using stable SIL catalogue IDs. Requirement-known failures are retained in raw logs as `XFAIL <SIL-ID> <DEF-ID>`, while console output labels driver-impacting reproductions as `ERROR` and minor reproductions as `WARNING`. A normal exit code of zero means no unexpected or driver-impacting functional error occurred; it is not a release-green result while any XFAIL or open traceability entry remains.

Strict mode reconciles each unique emitted `XFAIL <SIL-ID> <DEF-ID>` pair with the defect identifiers published in the matching `traceability.csv` row and rejects missing, unexpected, or duplicate pairs. Emitted `BLOCKED <SIL-ID>` records must likewise map to one catalogue row.

The direct runner uses the available MSYS2 GCC. Compiler warnings remain visible and are archived in per-target build logs but do not fail an ordinary host build; compiler errors remain blocking. The harness does not use `-Werror` or suppress the known host diagnostics. No production source is changed by this harness.

Every run ends with an easy-to-read summary that separates unexpected failures from reproduced errors (`XFAIL`). Reproduced errors are grouped as blocking `DRIVER-IMPACTING ERRORS` or nonblocking `MINOR ADVISORY WARNINGS` using the explicit metadata in `defect_details.csv`; the labels describe observed driver/system behavior and are not a formal safety classification. JUnit records driver-impacting XFAILs as failures and minor XFAILs as skipped advisories. Every catalogue row also supplies the cause, consequence, expected correction, and exact source/function shown in the report. Summary generation rejects missing or invalid metadata instead of guessing from defect names. Blocked checks are intentionally omitted from this human-readable summary; they remain available in detailed logs, JUnit, and traceability and still affect strict enforcement. The same report is saved as `build/sil-summary.txt`, and each invocation snapshots its artifacts into a unique `build/runs/<UTC>-<commit>/` directory.

Static-analysis compiler/analyzer errors and cppcheck error/warning findings are blocking. Style, performance, portability, and ordinary compiler-warning diagnostics are advisory. ASan, UBSan, and leak detection run with fault-halting behavior, so real sanitizer findings block CI; informational runtime messages alone do not.

GCC/gcov coverage is collected on every direct run. `build/coverage-summary.json` reports each explicitly named raw-model or ADC-driver compiler artifact from one test executable; metrics from separate builds are never maximized or combined. Raw-model artifacts are checked against 98% line, 100% function, and 95% branch-taken thresholds, while ADC-driver artifacts are checked against 95%/100%/90%. `build/coverage-production-summary.json` contains the ADC-driver subset. `build/run-manifest.json` archives the commit, input hashes, compiler identity, and discovered/missing tools.

JUnit testcases retain their emitting log (or runner record) and line number, so
identical PASS text from different targets is not deduplicated. Run archives are
kept under `build/runs`; the newest ten are retained by default and older run
directories are pruned only after verifying they are direct children of that
archive root, match the generated run-ID form, contain `traceability.csv`, and
contain a generated SIL test
executable or log. This recognizes legacy harness archives that predate
`source-status.txt`; nonmatching/manual directories are never pruned.
Only authoritative named reports (`junit.xml`, the manifest, both coverage
summaries, and `sil-summary.txt`) plus SIL test executables/logs and gcov logs
are copied; unrelated JSON/XML diagnostics in `build/` are excluded.
Pass a different `-RetentionCount` directly to
`archive_run.ps1` when a controlled CI retention policy requires it.

`verify_dbc.ps1` independently parses the hash-locked controlling DBC and filters exact transmitters `FrontNode`/`RearCAN` with the load-cell CAN-HAT exclusions. The runner defaults to the repository-relative `evidence/controlling.dbc`; pass `-DbcPath` for a CI-provided version-locked input. See `evidence/README.md` for canonical evidence names.

The ADS model now exercises CS-rising-edge command decode, relevant register masks, reserved-access logging, CRC framing/error recovery, and CRCERR write exceptions; citations are recorded in `model_citations.md`. Remaining acceptance blockers include first-call MemorySanitizer evidence; sensor population/variants/wiring; RE22 ordering and zero; ID 810 mapping and above-range policy; the internally impossible rear brake-line DBC range; production defects reported as XFAIL; and sanitizer/coverage/mutation/cross-build gates unavailable on this host.

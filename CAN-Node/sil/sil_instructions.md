# SIL Instructions

## Production driver immutability

Under no condition may SIL development, test maintenance, coverage work, static-analysis cleanup, sanitizer remediation, CI work, or evidence collection modify the main production driver or application firmware.

The following production paths are read-only for all SIL work:

- `Core/**`
- `Drivers/**`

SIL work may modify only the harness and its supporting material, including files under `sil/**` and SIL-specific CI configuration. Tests must compile and exercise the production sources exactly as they exist; they must not patch, replace, conditionally alter, or maintain a SIL-specific version of production logic.

If SIL, static analysis, sanitizers, coverage, or CI identify a production defect:

1. Record the defect and its production source location.
2. Add or retain an honest failing, XFAIL, characterization, or blocked check as appropriate.
3. Report the required correction to the driver owner.
4. Do not implement the correction in `Core/**` or `Drivers/**` as part of SIL work.

Passing the SIL by weakening an oracle, changing expected output to match defective behavior, suppressing a production finding, or modifying the production driver is prohibited.

## Result and quality-gate policy

`defect_details.csv` is authoritative for reproduced-error severity. In an ordinary SIL run, a `DRIVER-IMPACTING` XFAIL is a blocking error and a `MINOR` XFAIL is a visible, nonblocking advisory warning. `FAIL` and `XPASS` are always blocking. Strict/release mode remains red for every surviving XFAIL and every open traceability row, including MINOR items and independently blocked checks.

Compiler warnings must remain visible and be archived, but are advisory; compiler errors are blocking. Do not add `-Werror` or warning-suppression options to ordinary host SIL targets. Static-analysis error/warning findings from the configured blocking checks remain blocking, while style, performance, and portability suggestions are advisory.

Real AddressSanitizer, UndefinedBehaviorSanitizer, or LeakSanitizer findings are blocking. Informational sanitizer startup/runtime messages are advisory only. Never downgrade a sanitizer fault by matching or filtering its output; the sanitizer or test process exit status remains authoritative.

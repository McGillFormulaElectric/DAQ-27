# Version-locked SIL evidence

Place approved evidence in this directory using the canonical names below. The
SIL runner defaults to `controlling.dbc`; `-DbcPath` may override it with another
repository-relative or CI-provided path. Files are not fabricated or copied from
personal folders by the harness.

- `controlling.dbc`
- `MFE26_Sensor_DBC_Mapping.csv`
- `KTY84.pdf`
- `BRAKE_ROTOR_TEMP_INFKL-800.pdf`
- `ROT_POT_RE22.pdf`
- `AAS-920-672B-Thermometrics-GE1711.pdf`
- `ADS7066-RevC-SBAS928C.pdf` - TI SBAS928C Rev. C, SHA-256 `26E7ABC2644592C329615F2989AAB3B76ECB78DF47354D71D60022B76AD389EF`
- `SBAA456-ADS7066-CRC.pdf` - TI SBAA456, SHA-256 `B3E4E3311737D9FD40B678482BBE0A7E150C2EA9908F1C62A94EEF94254E1076`
- `ads7066_evidence.csv` - source URLs, retrieval date, byte lengths, and hashes for both ADS7066 authorities

The ADS7066 requirements baseline is `../ads7066_requirements.csv`. Each row cites one of the evidence IDs above, an exact section/table/figure locator, and the full controlling hash. The only HIL dispositions are analog accuracy and electrical power/GPIO behavior; their digitally observable consequences remain injectable in SIL.

Every present file is SHA-256 recorded in `build/run-manifest.json`; every absent
canonical input is recorded as missing. Approved large PDFs may instead be
provided by CI through a documented shared artifact location and passed to the
relevant gate explicitly.

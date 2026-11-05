# BACCAble – Contents and Development History

Special thanks to [netzmark](https://github.com/netzmark/BACCAble) and [gaucho1978](https://github.com/gaucho1978/BACCAble) for their work on BACCAble.

## Development branch origin — Netzmark's v3.7.4 merge

This development branch originated from **Netzmark's merge of the two BACCAble development lines**. Netzmark took **Gaucho's v3.7.4 firmware** as the basis and integrated his own modifications into it. **Our subsequent development started from that merged source**, rather than directly from an earlier Gaucho master release.

- **Merge comparison:** [Netzmark BACCAble — Gaucho v3.7.4 integration](https://github.com/netzmark/BACCAble/compare/5854eda3261cc004d507b100a5627f222d83eef1...aea756beaea2b72eca2c44e21a80a6afe2f60449)
- **Starting point for our changes:** the resulting Netzmark-merged BACCAble firmware.

The selections listed below document the functionality retained or excluded in our subsequent integration work; they should not be interpreted as a complete description of the upstream merge itself.

### Excluded from our integration

- **Netzmark's IBS hack:** Did not work reliably in our tests.
- **Netzmark's HSE:** Purpose and behavior were not evaluated.
- **Gaucho's Read Faults implementation:** At that stage, it was experimental and conflicted with Netzmark's large-display code.
- **Gaucho's PDC disable:** Netzmark's implementation was preferred.
- **Gaucho's park mirror:** Netzmark's implementation was preferred.
- **Gaucho's 4WD improvement:** Not tested on the RWD vehicle; at that stage, it conflicted with Netzmark's large-display code.

### Included in our integration

**From Netzmark:**

- Extended LCD parameter display, including the custom `printf` improvements and `uds_parameters_large.c`.
- PDC disable functionality.
- Park mirror delay.
- Menu-speed improvements (commit `828d421`; speed benefit not independently confirmed).
- Fix for `ACC_autostart2` failure during ISC (commit `a3ca082`).

**From Gaucho:**

- MAX hold.
- Sniffer (tested with a notebook and an Android phone).
- ELM327 (tested and responsive).
- Updated `user_config.h` configuration support.

**From Shadin71:**

- Parameter-setting refinements, including extended parameter pairs and rearrangement.
- Longer menu labels.
- Aligned status values for longer menu labels (`On`, `Off`, `Normal`, `Forced`, etc.).

## Version history

### v3.9.1.2

- Reordered menus.
- Restored radio/head-unit text when exiting BACCAble (mute/unmute handling).
- Added **DNA Memory** for the CANable devices: restoration works after ignition Off/On and BACCAble Off/On, without requiring a CANable sleep or low-power state.
- Reworked the **Read Faults** menu and diagnostics presentation, including BCM/ECM/ABS statuses and fault counts; long OK handling was adjusted to reduce accidental activation.
- Added **Reverse Mute** to the Main menu settings.

### v3.9.2.3

- Integrated the fix for incorrect parameter display from Gaucho's **v3.9.2** firmware.
- Reworked menu indexing to simplify future menu reordering.

### v3.9.2.4

- Integrated the newly developed functionality and reorganized files closer to Gaucho's original source-code structure.
- Consolidated diagnostic, DNA Memory and supporting code into existing firmware modules where practical.
- Completed all C1/C2/BH builds and the final in-car regression: **7 PASS, 2 SKIP, 0 FAIL** (Main/Features menu and Front Brake were not retested in that final run).

### v3.9.2.5 — VERIFIED (current reference)

- User-confirmed vehicle operation of DTC Log with simplified numbering, persistence and protected clearing.
- Reduced C1 SRAM footprint through optional LED compilation and Flash-resident immutable CAN templates.
- Conditionally hide the LED Setup menu item when the LED feature is not compiled in.
- C1 linker MAP: 3536 B free, an improvement of 3496 B over the earlier DTC Log build.
- Current release details, validation, memory measurements and release-label changes: [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md), section 10.

## Further technical details

For an overview of the major functional changes and validation results, see [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md).

> **Note:** The inclusion/exclusion lists above describe **historical integration decisions after the Netzmark v3.7.4 merge**. They are not necessarily a description of the complete merge, the current v3.9.2.5 feature set, or Gaucho's present upstream implementation.

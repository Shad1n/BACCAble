# BACCAble – Contents and Development History

Special thanks to [gaucho1978](https://github.com/gaucho1978/BACCAble) and [netzmark](https://github.com/netzmark/BACCAble) for their work on BACCAble.

## Original integration baseline

Based on Gaucho's **BACCAble master v3.3.2** firmware (commits dated July 25, 2026).

### Excluded from the original integration

- **Netzmark's IBS hack:** Did not work reliably in our tests.
- **Netzmark's HSE:** Purpose and behavior were not evaluated.
- **Gaucho's Read Faults implementation:** At that stage, it was experimental and conflicted with Netzmark's large-display code.
- **Gaucho's PDC disable:** Netzmark's implementation was preferred.
- **Gaucho's park mirror:** Netzmark's implementation was preferred.
- **Gaucho's 4WD improvement:** Not tested on the RWD vehicle; at that stage, it conflicted with Netzmark's large-display code.

### Included in the original integration

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

**From Shadin:**

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

- Based on Gaucho's master **v3.9.2**.
- Reworked menu indexing to simplify future menu reordering.

### v3.9.2.4 — Stage 18 VERIFIED

- Integrated the newly developed functionality and reorganized files closer to Gaucho's original source-code structure.
- Consolidated diagnostic, DNA Memory and supporting code into existing firmware modules where practical.
- Completed all C1/C2/BH builds and the final in-car regression: **7 PASS, 2 SKIP, 0 FAIL** (Main/Features menu and Front Brake were not retested in that final run).

## Further technical details

For an overview of the major functional changes and validation results, see [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md).

> **Note:** The inclusion/exclusion lists above describe the **original integration baseline**. They are historical decisions, not necessarily the feature set or upstream implementation status of v3.9.2.4.

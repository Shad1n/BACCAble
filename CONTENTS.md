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

### v3.9.2.5 — VERIFIED (archived reference)

- User-confirmed vehicle operation of DTC Log with simplified numbering, persistence and protected clearing.
- Reduced C1 SRAM footprint through optional LED compilation and Flash-resident immutable CAN templates.
- Conditionally hide the LED Setup menu item when the LED feature is not compiled in.
- C1 linker MAP: 3536 B free, an improvement of 3496 B over the earlier DTC Log build.
- Historical release details, validation and C1 RAM measurements: [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md), section 10. Consistency Maintenance NoLegacy and its 16/16 reported in-car PASS are documented in section 11.


### v3.9.2.6 — VERIFIED (archived rollback reference)

- Promoted the **Flash Consistency – No LED UI** source as the new development reference at the user's explicit request (2026-11-05). This is an acceptance decision, not a claim of new STM32CubeIDE builds or vehicle tests for this exact final source.
- Preserved the v3.9.2.5 NoLegacy improvements (Flash parameter ordering, standard Reverse Mute 0/1, C1/C2/BH and 18/30 display compatibility, UDS bounds, and previously reported **16/16 PASS** in-car regression).
- Removed redundant and uninitialized UDS shown-parameter Flash writes; existing Flash addresses and record format remain unchanged.
- Retained the original Setup and Params Setup `Save&Exit` navigation without newly introduced red-LED error indication or a failed-save menu lock. The low-level Flash verification remains in place.
- Clarified `getFreeRAM()` as a diagnostic stack-gap estimate, not the linker-MAP free SRAM value.
- Updated the default firmware version to `v3.9.2.6` (no `-dev` suffix). Documentation remains consolidated in [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md), especially section 12.

The next Smart Start/Stop / DisplayPersistent PREBUILD initially exposed a vehicle sleep regression; that **unguarded** variant is rejected. The corrected ignition-guarded solution is included in v3.9.2.7 (see below).

### v3.9.2.7 — VERIFIED (current reference, 2026-11-11)

- Based on the in-car validated **Smart Start/Stop + Display Ignition Guard** source. The user confirmed normal vehicle sleep with BACCAble connected, BACCAble switched-USB power-off, persistent Audio Text during a Start/Stop engine autostop, normal display shutdown on ignition OFF, and once-per-real-ignition-cycle Start/Stop automatic disable.
- Retained the original Gaucho one-shot Start/Stop and Stage G ignition-return infrastructure. Removed the legacy 39-second RPM-triggered text hide, but stopped periodic C1→BH dashboard / UDS requests when ignition/RFHUB RUN CAN states are OFF or stale, preventing the earlier vehicle sleep regression.
- Kept `DISABLE_LOW_CONSUME` unchanged for the USB-5V-powered Canable cards. No new firmware modules or storage layouts. No `-dev` suffix.
- Functional changes were user-tested in the vehicle on 2026-11-11; there is **no separately reported full STM32CubeIDE build or in-car flash test** of this version-string-only v3.9.2.7 release archive. Full release and validation scope are recorded in [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md), sections 13–16.

## Files included in this archive

- `firmware/ledsStripController/` — complete C1/C2/BH STM32CubeIDE source, project configurations, Gaucho's original `LICENSE.md` and `MAKEFILE.md`, and the user's `user_config.h`.
- `README.md` — original Gaucho project README, retained intact.
- `CONTENTS.md` — concise integration / version history and file inventory.
- `DEVELOPMENT_SUMMARY.md` — consolidated technical notes, historical intermediate DTC/RAM work, release records and actual test status. No separate PREBUILD-note or version-specific Markdown files remain.

## Further technical details

For the technical changes and validation records, see [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md).

> **Note:** The inclusion/exclusion lists above describe **historical integration decisions after the Netzmark v3.7.4 merge**. They are not necessarily a description of the complete merge, the current v3.9.2.7 feature set, or Gaucho's present upstream implementation.

**Current reference:** v3.9.2.7 VERIFIED. Historical v3.9.2.6 remains the rollback reference; the unguarded DisplayPersistent PREBUILD is not a release.

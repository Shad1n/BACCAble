# BACCAble – Contents and Development History

**Current development version:** v3.9.4.2 PREBUILD (USB storage consistency)
**Current verified release:** v3.9.4.1 VERIFIED (FAT12 FailSafe; user-reported hardware testing)
**Previous verified reference:** v3.9.3.2 VERIFIED
**Upstream:** [Gaucho BACCAble](https://github.com/gaucho1978/BACCAble) and [Netzmark BACCAble](https://github.com/netzmark/BACCAble)  
**Custom modifications:** `@Shadin`

## Project overview

This branch builds on Netzmark's merge of Gaucho v3.7.4 with Netzmark's changes, then incorporates selected later Gaucho improvements and `@Shadin` development. The existing Gaucho project layout is preserved. Features are evaluated by working behavior, regardless of their origin.

The firmware supports C1, C2 and BH Canable targets, petrol and diesel configurations, and both 18-character and extended 30-byte (28 visible characters) display paths. The supplied personal `user_config.h` remains part of the project. **Pump Force remains disabled.**

## Historical integration decisions

The following are historical decisions, not a complete inventory of either upstream repository.

**Retained from Netzmark:** extended display and UDS parameter handling (`uds_parameters_large.c`), PDC / Front Park Mute, park mirror delay, selected menu improvements and the ACC Autostart2 ISC fix.

**Retained from Gaucho:** the original firmware organization and compatible vehicle features, Sniffer, ELM327 and configuration support. Later Gaucho changes were reviewed for functional compatibility.

**Added or refined by `@Shadin`:** ECM/BCM/ABS diagnostics, DTC logging, DNA Memory, Reverse Mute, menu and display consistency, RAM optimizations, ignition-guarded display operation and first-boot FAT initialization.

**Not integrated or deliberately left unchanged:** Netzmark IBS hack (unreliable in tests), unevaluated HSE changes, Gaucho's alternative PDC/Park Mute implementation (Netzmark's works), optional brake-pedal/Max Hold additions, and the reviewed 4WD improvement (not needed for this RWD configuration). These decisions do not imply that every historical upstream feature is absent from the present firmware.

## Current functionality

### Diagnostics and USB files

- **Read faults** and **Clear faults** support ECM, BCM and ABS, including multiple DTCs and status information.
- Manual Read/Clear actions are written to `DTC_LOG.TXT` using `1.`, `2.`, ... event numbering, with a blank line between entries and no timestamps or `END` markers.
- **Clear DTC logs** requires long OK; `VERSION.TXT` is retained.
- On C2/BH, an erased FAT12 volume is initialized at startup. Existing compatible volumes are preserved; unknown or damaged volumes are not automatically formatted. TXT writes require verified reserved-cluster protection. C1 does not use USB mass storage.

### Settings and vehicle features

- **DNA Memory** restores the selected driving mode, including Race transitions.
- **Reverse Mute** uses Boolean Flash storage (`0` OFF, `1` ON); the `user_config.h` default follows the defined/commented `#define` convention.
- Setup and Params Setup selections persist in Flash; unnecessary writes and inconsistent parameter handling were corrected.
- **Smart Disable Start/Stop** acts once after the first engine start in a genuine ignition cycle. Subsequent manual Start/Stop choices are respected until the next ignition cycle.
- Existing Netzmark Front Park Mute, ACC-related behavior, HU Text Restore and other previously accepted functions are retained.

### Menus, display and power

- The 18/30 display paths, UDS parameter navigation and menu labels are retained and corrected where necessary.
- Setup and Params Setup intentionally reach **Save&Exit** at both ends of ±10-page navigation.
- The LED Controller menu appears only if LED support is compiled in; LED-disabled builds omit unused buffers.
- The former RPM-triggered 39-second Audio Text clearing was removed. Dashboard/UDS periodic transmissions require fresh ignition and RFHUB RUN information, allowing Start/Stop display persistence without keeping the vehicle awake after ignition OFF.
- The USB-powered Canable configuration retains `DISABLE_LOW_CONSUME`; UCAN-specific power handling is not reused blindly.

## Development history

### v3.9.1.2

Menu ordering, HU text restoration, early DNA Memory, diagnostic menu refinements and Reverse Mute integration.

### v3.9.2.3

Integrated Gaucho v3.9.2 parameter-display correction and simplified menu indexing.

### v3.9.2.4

Consolidated ECM/BCM/ABS diagnostics, DNA Memory, Flash configuration persistence and 18/30 display support into the existing source structure.

### v3.9.2.5

Added simplified DTC logging and protected clearing; reduced RAM usage, made the LED menu conditional, standardized Reverse Mute and corrected navigation, buffers and Flash values.

### v3.9.2.6

Reduced redundant Params Setup Flash writes, clarified `getFreeRAM()` reporting and retained the original Save&Exit behavior without LED-only error feedback.

### v3.9.2.7

Added once-per-ignition Smart Start/Stop and ignition-guarded dashboard/UDS updates. Removed the 39-second text clearing while preserving normal vehicle sleep.

### v3.9.2.8 (with fault, no longer applied. Corrected in v3.9.3.1)

Restored Gaucho's first-boot FAT12 initialization for erased C2/BH USB storage, with protection for existing data and reserved Flash regions.

### v3.9.3.0 – 4WD menu label fix

Applied Gaucho commit `3629e6a` menu-label correction: when engine RPM falls below 400 and 4WD Disable resets, the ` En` label is restored on the fixed 4WD menu entry (index 8), rather than overwriting the currently selected menu entry. No change to 4WD control behavior. The fix is retained in v3.9.3.1.

### v3.9.3.1 – Gaucho Simple FAT

Restored Gaucho-style FAT mount-or-format handling on C2/BH without the extra blank-Flash scan. Corrected FatFs `GET_SECTOR_COUNT` and `GET_BLOCK_SIZE` output types; `VERSION.TXT` is created or updated before legacy-file cleanup. Existing valid FAT volumes are reused. The last 6 KiB of Flash remains protected, so the advertised USB volume capacity must not be treated as fully writable. The user confirmed successful C1/C2/BH builds and flashing, and FAT plus `VERSION.TXT` creation on erased C2 and BH boards. DTC logging was not separately vehicle-tested with this release.

### v3.9.3.2 – Netzmark menu speed

Integrated Netzmark's faster menu timing: C1 serial reply timing 250 to 150 ms (linked C2/BH timing 200 to 100 ms) and BH dashboard frame interval 40 to 15 ms. The existing frame order and ignition-guarded display behavior are retained. User-reported C1/C2/BH builds and vehicle checks passed for menu response, text replacement, long UDS names, Setup / Params Setup, communication stability and vehicle sleep.

### v3.9.4.0 – Gaucho USB IRQ storm protection

Integrated Gaucho commit `495d72a` (upstream V.3.9.4): count consecutive USB interrupts without a SysTick and disable USB until MCU reset if the 254-interrupt threshold is reached. USB initialization and Sniffer/ELM327 USB bring-up honor the lockout. No other firmware behavior was intentionally changed. Upstream release is accepted as tested by Gaucho; local C1/C2/BH builds passed; vehicle verification remains pending. The first three version components follow Gaucho, while the fourth is the `@Shadin` subversion.

### v3.9.4.1 – FAT12 reserved-area protection

- Preserve the 128-sector USB geometry and physical write guard; mark clusters 114–125 as bad and verify by readback before enabling TXT writes.
- Preserve compatible existing volumes; initialize erased volumes only. Unknown or occupied layouts disable TXT writes, including DTC append and deletion, without disabling ECU diagnostics.
- Raw USB read range is unchanged. Local STM32CubeIDE and hardware verification pending.

### v3.9.4.2 – USB Storage Consistency (VERIFIED)

- Keep the 128-sector FAT12 geometry and the verified FAT12 reserved-cluster protection.
- Report the USB LUN as write-protected; set the MODE SENSE 6/10 WP bits from the storage callback, and reject any write callback with a negative status. FatFs internal TXT and DTC operations remain available.
- Preserve physical reads for sectors 0–115; return zero-filled data for sectors 116–127, including requests spanning the boundary, without exposing reserved Flash bytes.
- Replace stale `timerTest` builder paths with per-configuration project-local build paths and remove its stale project reference. All C1/C2/BH configurations are retained.
- C1/C2/BH STM32CubeIDE builds passed on 2026-11-24 after the MODE SENSE WP-bit addition; corresponding ELF/MAP outputs are present. Hardware verification remains pending.

## Files in this archive

- `firmware/ledsStripController/` – STM32CubeIDE project, C1/C2/BH configurations, source files and personal `user_config.h`.
- `README.md` – original Gaucho project instructions.
- `CONTENTS.md` – this consolidated project and development summary.
- Original project `LICENSE.md` and `MAKEFILE.md` are retained under the firmware directory.

## Development principles

- Preserve verified functionality and existing Gaucho/Netzmark structure; add no parallel subsystem without a concrete need.
- Maintain C1/C2/BH, petrol/diesel, 18/30 display and personal `user_config.h` compatibility.
- Mark custom source changes with `@Shadin`; keep Pump Force disabled.
- Do not reopen closed topics without a new requirement.
- Keep firmware version strings free of `-dev`; track verification separately.
- Record future release changes in this file rather than creating additional version-specific Markdown documents.


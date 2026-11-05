> **Current reference (2026-11-05): v3.9.2.6 VERIFIED (user-accepted).** This document preserves the v3.9.2.4 Stage 18 and v3.9.2.5 validation history. The v3.9.2.6 Flash Consistency source was accepted as VERIFIED by the user; no separate STM32CubeIDE build or in-car test of the final follow-up was reported.

# BACCAble – Development Summary (v3.9.2.6 VERIFIED)

**Technical overview for Gaucho**  
**Maintainer:** Shadin  
**Reference:** Gaucho master, incorporating Netzmark-based improvements  
**Current status:** v3.9.2.6 VERIFIED (explicitly accepted by user; final follow-up not independently rebuilt or vehicle-tested)  
**Historical baselines:** v3.9.2.4 Stage 18 – Build PASS / In-car VERIFIED; v3.9.2.5 Consistency Maintenance NoLegacy – user-reported Build PASS and 16/16 in-car PASS

## 1. Overview

This development builds on Gaucho's original BACCAble firmware and incorporates selected improvements from Netzmark's fork, together with additional diagnostic, configuration, and user-interface functionality.

The primary goals were to improve fault-code diagnostics, introduce persistent DNA drive-mode memory, simplify menu handling, and preserve existing vehicle functions.

The code has subsequently been reorganized to follow Gaucho's original file structure as closely as practical. Most intermediate development modules have been consolidated into existing source files to make review and future integration easier.

The changes are not presented as an immediately applicable upstream patch. The current firmware is a working reference implementation.

## 2. Diagnostic improvements – ECM, BCM and ABS

### ECM / BCM fault diagnostics

The diagnostic implementation was improved to support more reliable fault-code reading, storage, display and clearing.

Key improvements:

- Independent ECM and BCM DTC storage, supporting up to 20 records per module.
- Corrected ECU identification and status reporting.
- Improved handling of diagnostic responses, empty results and timeouts.
- DTC browsing using UP/DOWN, with short OK returning to the main menu.
- Improved Read Faults / Clear Faults state handling, including refreshing previously displayed results.

### ABS fault diagnostics

ABS fault-code reading and clearing were integrated into the existing C1/C2 architecture.

This includes:

- ABS diagnostic request and response processing.
- Transfer of diagnostic results from C2 to C1 over UART.
- Response framing and validation.
- Transaction identification to distinguish diagnostic responses.
- Timeout handling and user-facing status messages.

The current implementation uses a transaction-aware UART response protocol. It differs from the ABS implementation observed in a later Gaucho source archive, particularly in response layout and the number of DTCs transmitted per response.

These protocols should not be mixed without corresponding changes on both C1 and C2.

**Validation:** ECM, BCM and ABS Read Faults / Clear Faults have been tested successfully in the vehicle, including earlier tests with actual fault records.

## 3. DNA Memory

A persistent DNA drive-mode memory function was implemented to restore the previously selected driving mode after vehicle restart.

The implementation includes:

- Learning and storing the selected Natural, Dynamic or Advanced/Race mode.
- Controlled restoration after startup.
- Handling the required intermediate mode transitions when restoring Race.
- Protection against inappropriate mode changes during active diagnostic sessions.
- Persistent storage using a Flash Journal mechanism.

The implementation also considers startup timing and the interval between a manual mode change and vehicle shutdown.

**Validation:** DNA Memory has been tested in the vehicle across multiple development stages, including mode restoration after power cycles.

## 4. Configuration storage and reliability

The firmware configuration-storage mechanism was refactored to improve maintainability and preserve user settings.

The work includes:

- Consolidation of configuration-related code.
- Preservation of Save & Exit behavior.
- Correct restoration of saved settings after restart.
- Separation of configuration persistence from DNA Memory persistence.

**Validation:** Configuration changes, Save & Exit and post-restart persistence passed dedicated vehicle regression tests.

## 5. Menu and display improvements

The menu system was reorganized to make future development easier and reduce manual maintenance.

Notable changes:

- Centralized menu definitions and automatic item counting.
- Improved Main / Features menu structure.
- Consistent Exit handling.
- Standardized diagnostic menu labels.
- Improved button handling and navigation.
- Corrected several display formatting issues.

The existing support for both small and large factory displays was retained.

The `LARGE_DISPLAY` compile-time configuration remains available, supporting the extended display path while preserving the 18-character display variant.

The extended display implementation was intentionally retained despite its known performance difference compared with the original shorter display path.

## 6. Vehicle-related functions

Several existing functions were retained, corrected or integrated during development.

**HU Text Restore:** Improved restoration of the original head-unit text when BACCAble is switched off.

**Reverse Mute:** Integrated reverse-gear audio muting into the user configuration and setup structure. A known limitation remains when changing audio sources during reverse operation.

**Front Park Mute:** The working Netzmark implementation was retained rather than replaced.

**Front Brake / Launch display:** Menu and text presentation corrections were made, including formatting compatible with the 18-character display.

**Pump Force:** Experimental code was preserved but the function remains disabled by default (`function_pump_force_enabled = 0`).

Existing optional functionality, including Sniffer and ELM327, was retained.

## 7. Source-code restructuring

A significant part of the later development focused on reducing differences from Gaucho's original source layout.

This included:

- Consolidating temporary ABS diagnostic modules into existing C1/C2/UART files.
- Integrating DNA StageG and Flash Journal implementation code into existing firmware modules.
- Reducing dedicated headers and simplifying shared declarations.
- Centralizing ABS protocol constants.
- Resolving header ordering and circular dependency problems discovered during compilation.
- Preserving the original Gaucho/Netzmark code and comments where practical.

Custom development changes are identified using `@Shadin` comments.

The intention is to make it easier to understand individual improvements without introducing an unnecessarily different project architecture.

## 8. Validation status

The historical reference documented in this section is **BACCAble v3.9.2.4 (Stage 18)**.

All C1, C2 and BH STM32CubeIDE build configurations passed compilation.

The final in-car regression produced:

| Test | Result |
|---|---|
| Firmware startup | PASS |
| Main / Features menu | SKIP |
| Read Faults | PASS |
| Clear Faults | PASS |
| DNA Memory | PASS |
| Configuration persistence | PASS |
| Front Brake | SKIP |
| HU Text Restore | PASS |
| Display handling | PASS |

**Final result: 7 PASS / 2 SKIP / 0 FAIL.**

The two skipped areas were not retested during this final regression. They should not be interpreted as newly verified by Stage 18.

Earlier regression stages also covered individual diagnostic, menu and configuration functions.

## 9. Integration notes

This firmware is intended as a technical reference rather than a drop-in replacement for Gaucho's current master.

Some features have evolved independently in the upstream repository, particularly ABS diagnostics. Integration should therefore be based on functional comparison rather than wholesale replacement.

The most relevant areas for future upstream consideration are:

1. ECM / BCM DTC handling.
2. ABS diagnostic reliability and response handling.
3. DNA Memory and persistent Flash Journal.
4. Configuration-store improvements.
5. Menu structure, automatic indexing and display corrections.

The archived v3.9.2.4 and v3.9.2.5 implementations provide the historical baselines for examining these changes individually; v3.9.2.6 is the current development reference.

---

## Summary

BACCAble v3.9.2.4 combines improved multi-ECU diagnostics, persistent DNA Memory, reliable configuration storage, refined user-interface behavior and a more maintainable source layout. The result has been compiled successfully for all three firmware targets and validated in the vehicle for its principal functions.

---

## 10. v3.9.2.5 VERIFIED release – historical reference (2026-11-05)

Release decision: 2026-11-05. This was the development reference until the subsequent v3.9.2.6 release.
The previous v3.9.2.4 Stage 18 VERIFIED version remains an archived, unchanged baseline.

## Scope of v3.9.2.5

- Retains the v3.9.2.4 verified functionality and C1/C2/BH configurations.
- Manual Read faults / Clear faults events are appended to `DTC_LOG.TXT` on the existing USB storage; PC access remains read-only.
- Entries are numbered `1.`, `2.`, etc. and separated by a blank line; no timestamp or END marker.
- `Clear DTC logs` requires long OK, clears the log and restarts numbering at 1.
- Features menu final four items: Read faults, Clear faults, Clear DTC logs, Reset statistics.
- `VERSION.TXT` is preserved; obsolete `HELLO.TXT` is retired.
- RAM optimization: immutable immobilizer/exhaust CAN templates are stored in Flash; when `LED_STRIP_CONTROLLER_ENABLED` is not defined, LED buffers and the nonfunctional LED Setup menu entry are omitted. Defining the macro restores the LED implementation and its menu.
- The user's `user_config.h` is retained, as are 18/30-character display support and Pump Force disabled by default.
- Source changes remain integrated into existing Gaucho modules and annotated `@Shadin`.

## Validation and memory

- C1/C2/BH compilation was reported successful by the user during development.
- The user confirmed on 2026-11-05 that the current optimized firmware works correctly in the vehicle and requested VERIFIED status.
- C1 linker MAP (provided by user) reports `.data` 2512 B, `.bss` 8800 B, heap+stack reservation 1536 B, leaving **3536 B** of the 16384 B SRAM. Compared with the earlier DTC Log build's 40 B, this is **3496 B** additional linker-side headroom.
- Linker-side free RAM is not a measurement of peak runtime stack usage.
- This release is a source-level version bump of the verified code; the packaged source has not been independently rebuilt or reflashed after changing the version string. Rebuild C1/C2/BH to display `BACCAble v3.9.2.5` and update `VERSION.TXT`.

## Release change from last optimized build

Only the default `BUILD_VERSION` string and release documentation changed. No CAN behavior, menus, buffers, storage logic, or other functional firmware code was altered for this release label.

The v3.9.2.5 release details are maintained here rather than in a separate version-specific Markdown file.


## 11. v3.9.2.5 consistency maintenance – VERIFIED (2026-11-05)

The original **v3.9.2.5 VERIFIED source and archive remain archived**. The
Consistency Maintenance NoLegacy candidate was compiled and subsequently tested
in the vehicle. Both builds and the 16/16 car-regression results are reported
PASS by the user (2 preparation, 7 mandatory, 7 short regression; 0 FAIL, 0 SKIP).
`BUILD_VERSION` has no development suffix by project convention.
The changes follow the existing Gaucho/Netzmark structure and introduce no new
C modules, storage formats or menu navigation concepts.

- Restored numeric initialization order for Flash parameters 32 (ELM327), 33 (DNA
  Memory), 34 (Reverse Mute). Reverse Mute saves and reads the standard
  0/1 Boolean values; anything else is invalid and falls back to the
  `REVERSE_MUTE_ENABLED` default. No old 2=OFF compatibility or migration
  is retained, as requested by the sole user of this custom feature. As with
  other Gaucho Boolean features, an active `#define REVERSE_MUTE_ENABLED`
  means default ON; commenting it out means default OFF. No numeric macro
  value is required. This applies to C1 and BH defaults alike.
- Restored Gaucho's `user_config.h` defaults for parameters 30–32 (Front Park
  Mute, Sniffer, ELM327) when no valid saved value exists. Persisted 0/1 wins.
- Restored valid `2` for Exhaust Drive Style on Flash parameter 28, matching
  Gaucho's master behavior.
- Reduced configuration parameter array to the existing `CFG_TOTAL` count,
  without changing the 34-word transactional Flash layout, its CRC or addresses.
- Corrected the 18/30 display menu copies: transmit the **same UART frame size**,
  copy at most one menu row, pad unused tail bytes with zero.
- Sized the UDS Setup temporary name by the actual `uds_params_array` name
  field and guaranteed termination before stripping formatting patterns.
- Replaced unchecked unsigned reverse/forward UDS page searches with bounded
  circular searches. All-hidden pages return a bounded index.
- The statistics Flash save now writes only the two defined statistics at
  their original addresses, leaving the seven former uninitialized slots in
  their erased state.
- ACC virtual pad now processes only DLC=3 frames, matching its fixed
  three-byte CAN transmit buffer.
- Reused the already-defined `MENU_*` enum values rather than hardcoding IDs
  in the main/Features filtering function.
- Added clarifying `@Shadin` notes to the retained personal `user_config.h`,
  without changing feature defaults or `LARGE_DISPLAY` selection.

**Explicitly unchanged:** setup fast 10-page navigation **must** end at
`Save&Exit` in both directions; the LED compile-time menu guard, DTC Log,
DNA Memory, 18/30 display support, C1/C2/BH separation, and disabled Pump Force
remain as in v3.9.2.5 VERIFIED. Four-wheel-drive modifications remain excluded.

**Validation status:** C1/C2/BH STM32CubeIDE builds reported PASS by the user.
Full 16/16 vehicle checklist reported PASS, including Reverse Mute persistence,
Setup and Params Setup ±10 Save&Exit, UDS navigation, DNA Memory (D),
Read faults, DTC_LOG.TXT, Exhaust Drive Style 2, Front Park Mute/ACC,
Clear DTC logs safety and LED menu guard. This verification applies to the
**NoLegacy source before the following Flash-safety follow-up**, not to later changes.

---

## 12. v3.9.2.6 Flash Consistency – No LED UI: VERIFIED by user acceptance (2026-11-05)

Based on the user-verified v3.9.2.5 Consistency Maintenance NoLegacy version, this
Flash Consistency – No LED UI source is **designated v3.9.2.6 VERIFIED by the
user on 2026-11-05**. No additional C files, menu structures or persistent-storage
formats were introduced. This is a user acceptance decision; see the precise
validation distinction below.

- **UDS shown-parameter Flash save (`saveShownParamsOnflash`)**: removed the
  always-true `weShallWriteFlash` flag and uninitialized 30-halfword writes.
  Pack into a zero-initialized buffer sized from the existing 240-bit array;
  program only `ceil(active_menu_bits/16)` halfwords: 3/4 for the
  18-character gasoline/diesel menus (48/56 bits) and 4/4 for the
  30-character gasoline/diesel menus (51/54 bits). Preserve the original 4-byte Flash slot stride
  and the first 240 bits of the preexisting layout. Reader now accesses only
  the 15 halfwords required to recover 240 bits, not the redundant 30.
  Unprogrammed slots remain erased (`0xFFFF`) and read as enabled if a new
  menu parameter becomes relevant later. Bounds are clamped to the array size.
- **Setup and Params Setup `Save&Exit`**: the extra menu-level red LED
  indication and failed-save menu lock were withdrawn after review: the C1 unit
  is concealed in normal vehicle use, so its LED is not an observable user
  error notification. Both menus keep their original Save&Exit behavior.
  The existing Flash routines still report errors internally; the two-copy
  transactional C1 configuration store still validates each completed record
  with CRC and retains the earlier valid page on failed commits. The
  preexisting immobilizer shortcut keeps its nonzero-status error check.
  Sniffer/ELM327 hardware/USB operations remain after the configuration save
  call (normal success path unaffected). No new dashboard error-message
  infrastructure is introduced for exceptional Flash failures. The existing
  low-level LED diagnostics in Flash routines are left untouched.
- **`getFreeRAM()`**: clarified that the existing value is the gap from
  linker `_end` to the current MSP; it is NOT the linker MAP free SRAM and
  does not subtract dynamic heap allocation or future stack requirements.
  The function and both display variants remain functionally unchanged.
- **Version convention**: default `BUILD_VERSION` is `v3.9.2.6`, without
  `-dev`, even for work-in-progress artifacts. Verification status is tracked
  in the archive name and these release notes, not in the firmware version.
- **Intentionally unchanged:** both ±10 navigation paths lead to `Save&Exit`
  in Setup and Params Setup; C1/C2/BH targets, user_config.h, 18/30 displays,
  DNA Memory, DTC Log and its safety checks, and Pump Force disabled.

**Acceptance and validation record:** On 2026-11-05 the user explicitly requested
VERIFIED status for this v3.9.2.6 release. The preceding v3.9.2.5 NoLegacy
source had reported C1/C2/BH BUILD PASS and 16/16 in-car PASS (0 FAIL / 0 SKIP).
No separate CubeIDE build, linker MAP or in-car regression for the later Flash
Consistency – No LED UI modifications was reported; do not attribute those earlier
results to the newly modified source. The default version-string change itself
has not been separately compiled/flashed. The original behavior of Setup and
Params Setup `Save&Exit`, including both ±10-page endpoints, is preserved.

**Release contents:** This version increments only the default `BUILD_VERSION`
from the final Flash Consistency – No LED UI candidate and updates the existing
`CONTENTS.md` and `DEVELOPMENT_SUMMARY.md`. No new firmware modules, configuration
fields, or standalone version-document files were added. All later releases
continue the no-`-dev` version naming convention.

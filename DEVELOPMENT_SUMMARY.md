> **Current reference (2026-11-11): v3.9.2.7 VERIFIED (user-accepted).** Based on the Smart Start/Stop + Display Ignition Guard source, confirmed by the user in the vehicle for the targeted behavior. The v3.9.2.7 release changes only its default version string and documentation; a separate STM32CubeIDE build or vehicle test of this version-bumped archive has not been reported. Earlier v3.9.2.4–v3.9.2.6 baselines and their distinct validation histories remain documented below.

# BACCAble – Development Summary (v3.9.2.7 VERIFIED)

**Technical overview for Gaucho**  
**Maintainer:** Shadin  
**Reference:** Gaucho master, incorporating Netzmark-based improvements  
**Current status:** v3.9.2.7 VERIFIED — user-confirmed in-car operation of the Smart Start/Stop + Display Ignition Guard source; the v3.9.2.7 version-only build is not separately verified  
**Historical baselines:** v3.9.2.4 Stage 18 – Build PASS / In-car VERIFIED; v3.9.2.5 Consistency Maintenance NoLegacy – user-reported Build PASS and 16/16 in-car PASS; v3.9.2.6 Flash Consistency – user-accepted VERIFIED

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

**Reverse Mute:** Integrated reverse-gear audio muting into the user configuration and Setup; the earlier audio-source-change issue was resolved and closed.

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

The archived v3.9.2.4, v3.9.2.5 and v3.9.2.6 implementations provide historical baselines for examining these changes individually; v3.9.2.7 is the current development reference.

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

### Consolidated DTC Log / RAM-development notes (historical)

The four separate v3.9.2.4 PREBUILD note files have been retired from the release archive; their still-useful technical history is retained here. **Their intermediate format, memory estimates and PREBUILD warnings are historical, not specifications of v3.9.2.7.**

- The first DTC Log candidate used timestamps from CAN `0x73A`, an `END` marker and long numeric IDs. These were deliberately replaced by the final sequential `1.`, `2.` entries, a blank-line completion separator, **no timestamp** and **no END** marker. Recovery handles interrupted trailing records and recognizes earlier `END` records; the menu retains long-OK protection for `Clear DTC logs`. Read/Clear journaling is manual only; a Clear event records **request initiation**, not independent proof that the ECU cleared its DTCs. USB host MSC access is read-only, and storage work is coordinated with host mounting.
- An early C1 link failed with SRAM overflow (336 B). Intermediate optimizations reconstructed the ECU tag from per-ECU DTC counts in the UART transfer (the 4-byte ECU + 3-byte DTC record layout was preserved), and moved immutable sound/CAN data into Flash. The actual later C1 MAP, rather than early estimates, established **3536 B** linker-side free SRAM.
- The final optional LED implementation preserves the existing LED behavior when compiled with `LED_STRIP_CONTROLLER_ENABLED`; when absent, the sizeable PWM/LED buffers and the LED menu option are omitted. `can_tx` reads constant templates without changing the CAN payloads, while the original Gaucho file structure is retained.
- DTC capacity remains bounded (up to 20 retained records per ECU per read snapshot); excess records are marked truncated. Two event slots and bounded UART framing were used in the original memory work. The final functional behavior, not the preliminary candidate notes, is authoritative.


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

---

## 13. Smart Disable Start/Stop – once per real ignition cycle (integrated in v3.9.2.7, 2026-11-11)

**Development reference:** Began as a PREBUILD follow-up to the unchanged v3.9.2.6 VERIFIED release. The final ignition-guarded source was subsequently validated in the vehicle and promoted to v3.9.2.7 VERIFIED; see section 16.

**User requirement:** If Smart Disable Start/Stop is enabled in Setup, BACCAble
may automatically disable the vehicle's Start/Stop system **once after the
first proper engine start of a real ignition cycle**. The driver can manually
turn Start/Stop back on and leave it on through subsequent automatic engine
stops/restarts until the next proper ignition OFF/ON cycle (for example, the
following morning).

**Minimal source changes, no new state or storage:**

- `processingMessage0x000000FC.c`: removed the assignment that rearmed
  `startAndStopEnabled` on every drop below 400 RPM; the existing pending CAN
  request is still cancelled when the engine stops. This avoids rearming on
  normal Start/Stop auto-stops.
- `processingMessage0x00000384.c`: rearm the existing `startAndStopEnabled`
  one-shot flag from the already-implemented *confirmed ignition return* event
  (RFHUB non-RUN at least 1000 ms, followed by RUN with fresh 0x384/0x1EF/0x0FC
  observations and engine rotation). The detection is already used by DNA
  Stage G; its logic and restore behavior are untouched. Short, unconfirmed
  non-RUN transitions are not treated as a new ignition cycle.
- Gaucho's existing 5-second running-engine gate, 10-second power-up gate,
  Start/Stop state observation, CAN button simulation, Setup enable/disable
  setting, and saved Flash configuration are unchanged. `user_config.h` is
  retained verbatim (its `DISABLE_THE_FUNCTION_SMART_DISABLE_START_STOP` macro
  remains defined, so enabling through Setup or changing this default is
  necessary to exercise the feature).
- Updated the existing one-shot latch declaration comments to match this
  behavior; no new data, functions, or header declarations.
- C1-only change; no changes to C2/BH firmware logic, 18/30-character display
  behavior, UDS/DTC, DNA Memory, Pump Force or the ±10 `Save&Exit` behavior.
- `BUILD_VERSION` remains `v3.9.2.6` without `-dev`; candidate status is indicated
  by the package name and these release notes.

**Result:** On 2026-11-11 the user reported that Smart Start/Stop now works as intended in the combined ignition-guarded source. The full first-start / manual re-enable / auto-restart / next ignition-cycle sequence is the expected behavior; no additional full configuration build log was supplied for this source. See section 16.

## 14. Dashboard text during Start/Stop auto-stops (integrated in v3.9.2.7, 2026-11-11)

**Development reference:** This originated in the section 13 PREBUILD candidate. The first DisplayPersistent attempt was later found to keep the vehicle awake; only the subsequent ignition-guarded correction (section 15) is part of v3.9.2.7 VERIFIED.

**Observation:** With the BACCAble Audio Text/menu line visible, the vehicle's
Start/Stop auto-stop brings the engine below 400 RPM. Gaucho's original C1
logic then starts a delayed hide. After 39,000 ms it sets the BACCAble menu
invisible and transmits spaces with `clearDashboardBaccableMenu()`. The driver
observed this as the line disappearing after approximately 45 seconds.

**Minimal correction:**

- Removed only the RPM-triggered dashboard auto-hide timer and the 39-second
  dashboard-clearing branch from `processingMessage0x000000FC.c` and
  `functions_C1baccable.c`.
- Removed the now-unused `shutdownDashboardMenuRequestTime` and
  `baccabledashboardMenuWasVisible` globals/declarations. No replacement timer,
  ignition-state machine, settings, or additional firmware module was added.
- Preserved manual BACCAble menu visibility control and its explicit
  `clearDashboardBaccableMenu()` calls, the existing C1 CAN inactivity power
  management, and the HU's own ignition-OFF behavior. The user's normal
  ignition-off display disappearance makes this additional timed clearing
  unnecessary.
- Preserved the separate, unverified section 13 one-shot Smart Disable
  Start/Stop change. C2/BH, both display sizes, personalized `user_config.h`,
  DNA Memory, DTC log, Pump Force disabled, Setup/Params Setup Save&Exit and
  existing Flash storage remain untouched. The version stays `v3.9.2.6`
  without `-dev`.

**Result in the final guarded source:** The user confirmed that the BACCAble text stays visible during Start/Stop engine autostop; removing ignition turns the vehicle display off normally. The 39-second RPM-based clear was therefore unnecessary for display operation, but removal without an ignition-aware TX gate caused the vehicle sleep regression described below. Only the gated implementation was validated in the vehicle.

---

## 15. Dashboard ignition-aware TX gate (validated in v3.9.2.7 source, 2026-11-11)

**Regression report:** The previous SmartStartStop + DisplayPersistent test
firmware kept the vehicle electronics/USB awake after the ignition was
turned off; the vehicle slept immediately when BACCAble was disconnected.
This behavior was not present in the v3.9.2.6 VERIFIED baseline. Do not
use the prior unguarded DisplayPersistent candidate in the vehicle.

**Cause in the source:** Removing Gaucho's 39-second RPM-triggered menu
auto-hide removed the `baccableDashboardMenuVisible = 0` side effect. That
allowed C1's periodic dashboard requests (500ms) and Show Params UDS
requests (250ms) to continue even after the engine stopped, as long as
the USB-powered Canable hardware remained powered.

**Conservative correction:** In the existing C1 dashboard refresh/UDS
block, require fresh 0x384 and 0x1EF observations no older than 1500ms,
0x384 ignition RUN (=4), and RFHUB RUN (=0x08). This uses the existing
Stage G RX-only variables, with no new timers, modules, Flash settings or
CAN messages. The previous 39-second RPM-based auto-hide stays removed.
The BACCAble menu visibility preference is preserved while the ignition
is OFF, so normal display operation resumes when valid RUN messages
return. During a genuine Start/Stop auto-stop, ignition remains RUN, so
the display and UDS refresh can continue. A lost/stale status fails
closed and stops these periodic transmissions.

**Hardware:** `DISABLE_LOW_CONSUME` remains enabled in the user's Canable
USB-5V configuration. UCAN-only GPIO power management is not changed.
The once-per-ignition Smart Start/Stop change is unchanged.

**In-car validation (reported by user on 2026-11-11):** The corrected Ignition Guard candidate restores normal vehicle sleep with BACCAble connected; BACCAble shuts down afterwards as the switched USB power goes away. The BACCAble text remains visible during Start/Stop engine autostop, ignition OFF switches the vehicle display off, and the once-per-ignition Smart Start/Stop behavior works correctly. The earlier periodic sleep/wake symptom disappeared. These are user-reported actual vehicle observations, not a claim of full CAN-bus capture, independent STM32CubeIDE build logs, or a new test of the final version-only package.

---

## 16. v3.9.2.7 VERIFIED release – Smart Start/Stop + Display Ignition Guard (2026-11-11)

**Release decision:** The user explicitly requested v3.9.2.7 VERIFIED after testing the combined **SmartStartStop + DisplayIgnitionGuard** source in the vehicle and reporting the targeted functions correct. The previous v3.9.2.6 VERIFIED release remains an archived rollback reference. The earlier **unguarded DisplayPersistent** candidate is rejected due to the vehicle-sleep regression; do not use or reintroduce its unrestricted periodic dashboard/UDS transmissions.

**Included functionality:**

- Smart Disable Start/Stop triggers only once after the first proper engine start of each confirmed ignition cycle, using the existing Gaucho one-shot flag and the existing Stage G ignition-return event. Driver selection is respected after a manual Start/Stop re-enable, including later automatic engine restarts.
- The 39,000-ms RPM-based BACCAble Audio Text auto-hide is removed. Start/Stop auto-stop with ignition RUN does not blank the BACCAble line.
- C1's periodic C1→BH dashboard and Show Params UDS transmissions are gated by fresh CAN `0x384` ignition RUN and `0x1EF` RFHUB RUN observations (max age 1500 ms). OFF or stale information stops these transmissions and permits vehicle/USB sleep; menu visibility preference survives for the next valid RUN state.
- This does not change `DISABLE_LOW_CONSUME`: the user's Canable cards use switched USB 5 V rather than UCAN-style constant OBD power. No unrelated GPIO low-power framework, CAN transmission system or configuration format was added.
- Preserve C1/C2/BH configurations, 18/30-character displays, the user's personal `user_config.h`, DNA Memory, DTC Log and its protected clearing, the Setup / Params Setup ±10 `Save&Exit` endpoints, the original HU Text Restore, and Pump Force disabled. Changes remain in existing source files with `@Shadin` annotations.

**Reported vehicle regression – PASS for the targeted functions:**

| Test (2026-11-11) | User-reported result |
|---|---|
| Vehicle electronics fall asleep normally with BACCAble still connected | PASS |
| Switched USB power / BACCAble eventually turn off | PASS |
| Previous periodic wake/speaker-click cycle no longer occurs | PASS |
| BACCAble display line persists during Start/Stop engine autostop | PASS |
| Vehicle display turns off when ignition is removed | PASS |
| Smart Disable Start/Stop works once per real ignition cycle | PASS |

**Verification scope:** These are real user-reported in-car observations of the final ignition-guarded functional source. No separate complete CubeIDE C1/C2/BH build logs, MAP reports or flashed binaries for the final **v3.9.2.7** version-string update were supplied; do not state otherwise. The local release checks verify source identity, ZIP integrity and patch applicability, not hardware build or vehicle testing of a new binary.

**Release delta:** Relative to the validated Ignition Guard source, only the default `BUILD_VERSION` is changed from `v3.9.2.6` to `v3.9.2.7` (no `-dev`) and the existing release documentation is updated. Four obsolete stand-alone DTC Log/RAM PREBUILD Markdown notes were consolidated into this file and removed; the Gaucho `README.md`, `LICENSE.md` and `MAKEFILE.md` are retained. No new firmware module or separate version-specific documentation file was created.

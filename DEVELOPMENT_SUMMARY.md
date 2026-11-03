# BACCAble v3.9.2.4 – Development Summary

**Technical overview for Gaucho**  
**Maintainer:** Shadin  
**Reference:** Gaucho master, incorporating Netzmark-based improvements  
**Status:** Stage 18 – Build PASS / In-car VERIFIED

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

The current reference is **BACCAble v3.9.2.4 (Stage 18)**.

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

The complete v3.9.2.4 implementation provides a working baseline for examining these changes individually.

---

## Summary

BACCAble v3.9.2.4 combines improved multi-ECU diagnostics, persistent DNA Memory, reliable configuration storage, refined user-interface behavior and a more maintainable source layout. The result has been compiled successfully for all three firmware targets and validated in the vehicle for its principal functions.

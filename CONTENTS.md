# BACCAble – Contents and Development History

**Current reference:** v3.9.2.8 VERIFIED  
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
- On C2/BH, a new or completely erased USB Flash area is initialized with FAT12 on first boot. Existing valid volumes are preserved; nonempty or unreadable areas are not automatically formatted. C1 does not use USB mass storage.

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

### v3.9.2.4 – Stage 18

Consolidated ECM/BCM/ABS diagnostics, DNA Memory, Flash configuration persistence and 18/30 display support into the existing source structure.

### v3.9.2.5

Added simplified DTC logging and protected clearing; reduced RAM usage, made the LED menu conditional, standardized Reverse Mute and corrected navigation, buffers and Flash values.

### v3.9.2.6

Reduced redundant Params Setup Flash writes, clarified `getFreeRAM()` reporting and retained the original Save&Exit behavior without LED-only error feedback.

### v3.9.2.7

Added once-per-ignition Smart Start/Stop and ignition-guarded dashboard/UDS updates. Removed the 39-second text clearing while preserving normal vehicle sleep.

### v3.9.2.8 – current VERIFIED reference

Restored Gaucho's first-boot FAT12 initialization for erased C2/BH USB storage, with protection for existing data and reserved Flash regions.

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

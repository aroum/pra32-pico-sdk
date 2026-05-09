# PRA32-U Table Generators

This directory contains Ruby scripts used to generate lookup tables for the PRA32-U synthesizer. These tables are memory-optimized using `static const` to ensure they reside in Flash memory (XIP) on the RP2040, saving valuable RAM.

## Requirements

- Ruby (any modern version)

## Scripts

### 1. `pra32-u-generate-osc-table.rb`
Generates oscillator frequency, tuning, and waveform tables (saw, triangle, square, sine).
- **Output**: `pra32-u-osc-table.h`

### 2. `pra32-u-generate-filter-table.rb`
Generates low-pass filter coefficients and gain tables for various resonance levels.
- **Output**: `pra32-u-filter-table.h`

### 3. `pra32-u-generate-eg-table.rb`
Generates envelope generator (EG) coefficient tables for attack, decay, and release phases.
- **Output**: `pra32-u-eg-table.h`

### 4. `pra32-u-generate-lfo-table.rb`
Generates rate and fade coefficient tables for LFO and chorus effects.
- **Output**: `pra32-u-lfo-table.h`

### 5. `pra32-u-generate-constants-rb-from-h.rb`
Helper script that extracts constants from `include/pra32-u-constants.h` and creates `pra32-u-constants.rb` for use by the other generators.

## How to Run

To regenerate all tables, run the following commands from this directory:

```bash
ruby pra32-u-generate-constants-rb-from-h.rb
ruby pra32-u-generate-osc-table.rb
ruby pra32-u-generate-filter-table.rb
ruby pra32-u-generate-eg-table.rb
ruby pra32-u-generate-lfo-table.rb
```

After generation, move the resulting `.h` files to the `include/` directory at the root of the project.

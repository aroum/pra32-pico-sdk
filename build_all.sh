#!/bin/bash

# Configuration
PROJECT_DIR="."

# 0. Board Detection / Override
if [ -n "$BOARD_ARG" ]; then
    BOARD_NAME="$BOARD_ARG"
    if [ "$BOARD_NAME" == "nizkoteno" ]; then BOARD_ID=2;
    elif [ "$BOARD_NAME" == "omsk" ]; then BOARD_ID=3;
    elif [ "$BOARD_NAME" == "pra32" ]; then BOARD_ID=1;
    else echo "Unknown board: $BOARD_ARG"; exit 1; fi
else
    BOARD_LINE=$(grep "^#define CURRENT_BOARD BOARD_" include/config.h)
    if [[ $BOARD_LINE == *"BOARD_NIZKOTENO"* ]]; then
        BOARD_NAME="nizkoteno"; BOARD_ID=2;
    elif [[ $BOARD_LINE == *"BOARD_OMSK"* ]]; then
        BOARD_NAME="omsk"; BOARD_ID=3;
    else
        BOARD_NAME="pra32"; BOARD_ID=1;
    fi
fi

TARGET_NAME="$BOARD_NAME"
BUILD_DIR="${PROJECT_DIR}/build_${BOARD_NAME}"

# RP2040 configuration
PICO_PLATFORM="rp2040"
PICO_BOARD="pico"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default flags
CLEAN=false
SIZE=false
FLASH=false
BOARD_ARG=""

# Help function
show_help() {
    echo "Usage: ./build_all.sh [options]"
    echo "Board detected: $BOARD_NAME (via config.h)"
    echo "Options:"
    echo "  -b, --board    Specify board (pra32, nizkoteno, omsk)
  -c, --clean    Remove build directory and re-run CMake"
    echo "  -s, --size     Show detailed memory usage report"
    echo "  -f, --flash    Build and flash to RP2040 via picotool"
    echo "  -h, --help     Show this help message"
    echo ""
    echo "Example: ./build_all.sh -csf (Clean, Show Size, and Flash)"
}

# Parse arguments
while getopts "b:csfh" opt; do
    case "${opt}" in
        b) BOARD_ARG="${OPTARG}" ;;
        c) CLEAN=true ;;
        s) SIZE=true ;;
        f) FLASH=true ;;
        h) show_help; exit 0 ;;
        *) show_help; exit 1 ;;
    esac
done

# 1. Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}=== Cleaning build directory for $BOARD_NAME ===${NC}"
    rm -rf "$BUILD_DIR"
fi

# 2. Setup Build Directory and CMake
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}=== Initializing CMake for $BOARD_NAME ===${NC}"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR" || exit
    cmake -DPICO_PLATFORM=$PICO_PLATFORM -DPICO_BOARD=$PICO_BOARD -DCURRENT_BOARD=$BOARD_ID ..
    cd ..
fi

# 3. Always Build
echo -e "${YELLOW}=== Building $TARGET_NAME ===${NC}"
cd "$BUILD_DIR" || exit

# Determine number of cores for faster build
if [[ "$OSTYPE" == "darwin"* ]]; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=$(nproc 2>/dev/null || echo 1)
fi

make -j"$JOBS"
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
cd ..

echo -e "${GREEN}=== Build Successful! ===${NC}"
echo -e "Binary: $BUILD_DIR/$TARGET_NAME.uf2"

# 4. Size Report
if [ "$SIZE" = true ]; then
    ELF_FILE="$BUILD_DIR/$TARGET_NAME.elf"
    if [ ! -f "$ELF_FILE" ]; then
        echo -e "${RED}Error: ELF file not found at $ELF_FILE${NC}"
    else
        echo -e "\n${GREEN}=== Detailed Memory Analysis (RP2040) ===${NC}"

        # Get raw data from arm-none-eabi-size
        MAP_DATA=$(arm-none-eabi-size -A "$ELF_FILE")
        
        # Helper function to get section size
        get_size() {
            local val=$(echo "$MAP_DATA" | grep -E "^\.$1" | awk '{print $2}' | head -n 1)
            echo ${val:-0}
        }

        # Extract values (bytes)
        TEXT=$(get_size "text")
        RODATA=$(get_size "rodata")
        DATA=$(get_size "data")
        BSS=$(get_size "bss")
        STACK=$(get_size "stack_dummy")
        BINARY_INFO=$(get_size "binary_info")
        
        # Calculations
        TOTAL_FLASH=$((TEXT + RODATA + DATA + BINARY_INFO))
        TOTAL_RAM=$((DATA + BSS + STACK))
        
        # Hardware limits
        MAX_FLASH=$((16 * 1024 * 1024)) # 16MB
        MAX_RAM=$((264 * 1024))         # 264KB
        
        # Formatting helper
        format_num() {
            python3 -c "print(f'{int($1):,}')" 2>/dev/null || echo $1
        }

        FLASH_PCT_RAW=$(echo "scale=4; $TOTAL_FLASH * 100 / $MAX_FLASH" | bc)
        FLASH_PCT=$(printf "%.2f" $FLASH_PCT_RAW)

        echo -e "${YELLOW}1. Flash (Non-volatile Memory)${NC}"
        printf "   .text:   %12s bytes\n" "$(format_num $TEXT)"
        printf "   .rodata: %12s bytes\n" "$(format_num $RODATA)"
        printf "   .data:   %12s bytes\n" "$(format_num $DATA)"
        echo -e "   -------------------------------------------"
        echo -e "   Total Flash Usage: ${GREEN}$((TOTAL_FLASH / 1024)) KB${NC} (~$FLASH_PCT% of 16MB)"

        echo -e "\n${YELLOW}2. RAM (Operating Memory)${NC}"
        printf "   .data:   %12s bytes\n" "$(format_num $DATA)"
        printf "   .bss:    %12s bytes\n" "$(format_num $BSS)"
        printf "   .heap:   %12s bytes\n" "2,048" # Approx
        printf "   .stack:  %12s bytes\n" "$(format_num $STACK)"
        echo -e "   -------------------------------------------"
        echo -e "   Total RAM Usage:   ${GREEN}$((TOTAL_RAM / 1024)) KB${NC} out of 264 KB"
        echo -e "   Remaining RAM:     $(((MAX_RAM - TOTAL_RAM) / 1024)) KB"
        echo ""
    fi
fi

# 5. Flashing
if [ "$FLASH" = true ]; then
    UF2_FILE="$BUILD_DIR/$TARGET_NAME.uf2"
    echo -e "${YELLOW}=== Flashing Process ===${NC}"

    if ! picotool info > /dev/null 2>&1; then
        echo -e "${RED}Device not found. Please connect in BOOTSEL mode.${NC}"
    else
        echo "Uploading $UF2_FILE..."
        picotool load "$UF2_FILE" -x
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}Flash Successful!${NC}"
        else
            echo -e "${RED}Flash failed!${NC}"
        fi
    fi
fi

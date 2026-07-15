#!/bin/bash

# Script to generate progressively harder Sokoban levels using sokohard in parallel

SOKOHARD_BIN="sokohard/out/sokohard"
LEVELS_DIR="levels"
NUM_LEVELS=42
NUM_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)  # Auto-detect CPU cores

# Check if sokohard binary exists
if [ ! -x "$SOKOHARD_BIN" ]; then
    echo "Error: sokohard binary not found at $SOKOHARD_BIN"
    exit 1
fi

# Check for GNU parallel
if ! command -v parallel &> /dev/null; then
    echo "GNU parallel not found. Using a simple parallel implementation."
    USE_GNU_PARALLEL=0
else
    USE_GNU_PARALLEL=1
fi

# Create levels directory if it doesn't exist
mkdir -p "$LEVELS_DIR"

# Remove old generated levels
rm -f "$LEVELS_DIR"/*.sok

# Difficulty progression parameters
MIN_HEIGHT=2  # Reduced minimum height
MAX_HEIGHT=3  # Very small max height 
MIN_WIDTH=4   # Width is double the height
MAX_WIDTH=6   # Width is double the height
MAX_BOXES=4   # Maximum number of boxes

echo "Generating $NUM_LEVELS progressively difficult levels using $NUM_CORES parallel jobs..."

# Define the generation function
generate_level() {
    local i=$1
    local tmp_dir=$(mktemp -d)
    
    # Calculate difficulty parameters based on level number
    # We'll use linear progression for simplicity
    local height=$(( MIN_HEIGHT + (i * (MAX_HEIGHT - MIN_HEIGHT)) / NUM_LEVELS ))
    local width=$(( MIN_WIDTH + (i * (MAX_WIDTH - MIN_WIDTH)) / NUM_LEVELS ))
    
    # Calculate boxes based on level quartile (1-4 boxes distributed as 1:1-25%, 2:25-50%, 3:50-75%, 4:75-100%)
    local level_percent=$(( i * 100 / NUM_LEVELS ))
    if (( level_percent < 25 )); then
        local boxes=1
    elif (( level_percent < 50 )); then
        local boxes=2
    elif (( level_percent < 75 )); then
        local boxes=3
    else
        local boxes=4
    fi
    
    # Generate seed for reproducibility
    local seed=$((1000 + i))  # Different seed for each level
    
    # Use box changes for even-numbered levels, player moves for odd-numbered
    local metric_flag=""
    if ((i % 2 == 0)); then
        metric_flag="--box-changes"
    fi
    
    # Pad level number for consistent sorting
    local padded_num=$(printf "L%02d" $i)
    local tmp_output="${tmp_dir}/out.sok"
    local output_file="$LEVELS_DIR/$padded_num.sok"
    
    echo "Generating level $i/$NUM_LEVELS: width=$width, height=$height, boxes=$boxes"
    
    # Run sokohard to generate the level
    "$SOKOHARD_BIN" -w $width -h $height -b $boxes -o "${tmp_dir}/out" -s $seed $metric_flag
    
    # Check if the level was generated successfully
    if [ -f "$tmp_output" ]; then
        mv "$tmp_output" "$output_file"
        echo "Successfully created $output_file"
    else
        echo "Warning: Failed to generate level $i"
    fi
    
    # Clean up temporary directory
    rm -rf "$tmp_dir"
}

# Export variables and function for parallel usage
export -f generate_level
export SOKOHARD_BIN LEVELS_DIR NUM_LEVELS MIN_HEIGHT MAX_HEIGHT MIN_WIDTH MAX_WIDTH MAX_BOXES

if [ "$USE_GNU_PARALLEL" -eq 1 ]; then
    # Use GNU parallel
    parallel -j "$NUM_CORES" generate_level ::: $(seq 1 $NUM_LEVELS)
else
    # Simple parallel implementation using background processes and wait
    for ((i=1; i<=NUM_LEVELS; i++)); do
        # Run in background with process group control
        (
            generate_level $i
        ) &
        
        # Limit the number of parallel processes
        if (( i % NUM_CORES == 0 )); then
            wait
        fi
    done
    
    # Wait for any remaining background processes
    wait
fi

echo "Generation complete. Generated levels in $LEVELS_DIR/"

# Run the embed_levels tool to update the game with the new levels
if [ -x "./embed_levels" ]; then
    echo "Updating embedded levels..."
    ./embed_levels
    echo "Embedded levels updated. Run 'make' to rebuild the game."
else
    echo "Note: embed_levels tool not found. Run 'make update' to include the new levels in the game."
fi

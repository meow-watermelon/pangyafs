#!/bin/bash

################################################################################
# PangYa Filesystem Stress-Test Utility
# 
# Purpose: Validates filesystem stability and structural integrity by simulating
# heavy I/O and edge-case workloads. Each step is verified by checking the 
# command output to ensure results are as expected.
#
# Requirements:
#  - mkfs.pangya, chfs.pangya, df.pangya, mkdir.pangya, cp.pangya,
#    stat.pangya, ls.pangya, rm.pangya
#  - All tools should be built via 'make' in the pangyafs repository
################################################################################

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# mkfs.pangya hardcodes the filename to "pangyafs.img"
TEST_IMAGE="${SCRIPT_DIR}/pangyafs.img"
TEST_FILE="${SCRIPT_DIR}/test_content.txt"

# Filesystem parameters
FILESYSTEM_SIZE_KB=1024
INODE_COUNT=200

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test state tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

################################################################################
# Utility Functions
################################################################################

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
    ((TOTAL_TESTS++))
}

log_output() {
    echo -e "${CYAN}[OUTPUT]${NC}"
    while IFS= read -r line; do
        echo "  $line"
    done
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASSED_TESTS++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((FAILED_TESTS++))
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

cleanup() {
    log_info "Cleaning up test artifacts..."
    rm -f "$TEST_IMAGE" "$TEST_FILE"
}

trap cleanup EXIT

################################################################################
# Test Steps
################################################################################

# Step 1: Create filesystem
test_mkfs() {
    log_test "Step 1: Creating filesystem image (${FILESYSTEM_SIZE_KB}KB, ${INODE_COUNT} inodes)..."
    
    local mkfs_output
    if ! mkfs_output=$(./mkfs.pangya -s "$FILESYSTEM_SIZE_KB" -i "$INODE_COUNT" 2>&1); then
        log_fail "mkfs.pangya command failed"
        echo "$mkfs_output" | log_output
        return 1
    fi
    
    echo "$mkfs_output" | log_output
    
    if [ ! -f "$TEST_IMAGE" ]; then
        log_fail "Filesystem image not created at $TEST_IMAGE"
        return 1
    fi
    
    # Verify output contains expected initialization info
    if ! echo "$mkfs_output" | grep -q "INITIALIZE DISK IMAGE"; then
        log_fail "mkfs.pangya output format unexpected"
        return 1
    fi
    
    log_pass "Filesystem created: $TEST_IMAGE"
    return 0
}

# Step 2: Verify read-only and read-write modes
test_chfs_modes() {
    log_test "Step 2: Verifying chfs.pangya read-only and read-write modes..."

    local df_output
    local command_output
    local seed_dir="/chfs-seed"
    local seed_file="/chfs-seed/seed.txt"
    local readonly_test_dir="/chfs-ro-test"
    local readonly_test_file="/chfs-ro.txt"
    local readwrite_test_dir="/chfs-rw-test"

    printf '%s\n' "PangYa chfs read-only test data" > "$TEST_FILE"

    if ! command_output=$(./mkdir.pangya -f "$TEST_IMAGE" "$seed_dir" 2>&1); then
        log_fail "Failed to create directory for read-only rm test"
        echo "$command_output" | log_output
        return 1
    fi

    if ! command_output=$(./cp.pangya -f "$TEST_IMAGE" "$TEST_FILE" "$seed_file" 2>&1); then
        log_fail "Failed to create file for read-only rm test"
        echo "$command_output" | log_output
        return 1
    fi

    if ! command_output=$(./chfs.pangya -f "$TEST_IMAGE" -r 2>&1); then
        log_fail "chfs.pangya -r command failed"
        echo "$command_output" | log_output
        return 1
    fi

    if ! df_output=$(./df.pangya -f "$TEST_IMAGE" 2>&1); then
        log_fail "df.pangya failed after enabling read-only mode"
        return 1
    fi

    echo "$df_output" | log_output

    if ! echo "$df_output" | grep -q "filesystem status: read-only"; then
        log_fail "Filesystem was not marked read-only after chfs.pangya -r"
        return 1
    fi

    log_info "  Command (expected to fail): ./mkdir.pangya -f $TEST_IMAGE $readonly_test_dir"
    if command_output=$(./mkdir.pangya -f "$TEST_IMAGE" "$readonly_test_dir" 2>&1); then
        log_fail "mkdir.pangya unexpectedly succeeded while filesystem was read-only"
        echo "$command_output" | log_output
        return 1
    fi
    echo "$command_output" | log_output
    if ! echo "$command_output" | grep -qi "read-only"; then
        log_fail "mkdir.pangya failed for a reason other than read-only mode"
        return 1
    fi

    log_info "  Command (expected to fail): ./cp.pangya -f $TEST_IMAGE $TEST_FILE $readonly_test_file"
    if command_output=$(./cp.pangya -f "$TEST_IMAGE" "$TEST_FILE" "$readonly_test_file" 2>&1); then
        log_fail "cp.pangya unexpectedly succeeded while filesystem was read-only"
        echo "$command_output" | log_output
        return 1
    fi
    echo "$command_output" | log_output
    if ! echo "$command_output" | grep -qi "read-only"; then
        log_fail "cp.pangya failed for a reason other than read-only mode"
        return 1
    fi

    log_info "  Command (expected to fail): ./rm.pangya -f $TEST_IMAGE $seed_file"
    if command_output=$(./rm.pangya -f "$TEST_IMAGE" "$seed_file" 2>&1); then
        log_fail "rm.pangya unexpectedly succeeded while filesystem was read-only"
        echo "$command_output" | log_output
        return 1
    fi
    echo "$command_output" | log_output
    if ! echo "$command_output" | grep -qi "read-only"; then
        log_fail "rm.pangya failed for a reason other than read-only mode"
        return 1
    fi

    if ./ls.pangya -f "$TEST_IMAGE" "/" 2>&1 | grep -q "chfs-ro-test"; then
        log_fail "Read-only mkdir.pangya attempt changed the filesystem"
        return 1
    fi

    if ./ls.pangya -f "$TEST_IMAGE" "/" 2>&1 | grep -q "chfs-ro.txt"; then
        log_fail "Read-only cp.pangya attempt changed the filesystem"
        return 1
    fi

    if ! ./ls.pangya -f "$TEST_IMAGE" "$seed_dir" 2>&1 | grep -q "seed.txt"; then
        log_fail "Read-only rm.pangya attempt changed the filesystem"
        return 1
    fi

    if ! command_output=$(./chfs.pangya -f "$TEST_IMAGE" -w 2>&1); then
        log_fail "chfs.pangya -w command failed"
        echo "$command_output" | log_output
        return 1
    fi

    if ! df_output=$(./df.pangya -f "$TEST_IMAGE" 2>&1); then
        log_fail "df.pangya failed after enabling read-write mode"
        return 1
    fi

    echo "$df_output" | log_output

    if ! echo "$df_output" | grep -q "filesystem status: read-write"; then
        log_fail "Filesystem was not marked read-write after chfs.pangya -w"
        return 1
    fi

    if ! command_output=$(./mkdir.pangya -f "$TEST_IMAGE" "$readwrite_test_dir" 2>&1); then
        log_fail "Write failed after enabling read-write mode"
        echo "$command_output" | log_output
        return 1
    fi

    if ! ./ls.pangya -f "$TEST_IMAGE" "/" 2>&1 | grep -q "chfs-rw-test"; then
        log_fail "Read-write test directory was not created"
        return 1
    fi

    if ! command_output=$(./rm.pangya -f "$TEST_IMAGE" "$readwrite_test_dir" 2>&1); then
        log_fail "Failed to remove read-write test directory"
        echo "$command_output" | log_output
        return 1
    fi

    if ! command_output=$(./rm.pangya -f "$TEST_IMAGE" "$seed_file" 2>&1); then
        log_fail "Failed to remove seeded file after enabling read-write mode"
        echo "$command_output" | log_output
        return 1
    fi

    if ! command_output=$(./rm.pangya -f "$TEST_IMAGE" "$seed_dir" 2>&1); then
        log_fail "Failed to remove seeded directory after enabling read-write mode"
        echo "$command_output" | log_output
        return 1
    fi

    log_pass "Read-only mode blocks mkdir, cp, and rm; read-write mode allows writes"
    return 0
}

# Step 3: Check initial filesystem status
test_df_initial() {
    log_test "Step 3: Checking initial filesystem status..."
    
    local df_output
    if ! df_output=$(./df.pangya -f "$TEST_IMAGE" 2>&1); then
        log_fail "df.pangya command failed"
        return 1
    fi
    
    echo "$df_output" | log_output
    
    # Verify output contains expected information
    if ! echo "$df_output" | grep -q "SUPERBLOCK INFORMATION"; then
        log_fail "df.pangya output format unexpected"
        return 1
    fi
    
    if ! echo "$df_output" | grep -q "INODE USAGE INFORMATION"; then
        log_fail "df.pangya did not output inode usage"
        return 1
    fi
    
    if ! echo "$df_output" | grep -q "BLOCK USAGE INFORMATION"; then
        log_fail "df.pangya did not output block usage"
        return 1
    fi
    
    log_pass "Initial filesystem status verified"
    return 0
}

# Step 4: Create directories
test_mkdir() {
    log_test "Step 4: Creating test directories..."
    
    # Create dir1
    if ! ./mkdir.pangya -f "$TEST_IMAGE" "/dir1" 2>&1 > /dev/null; then
        log_fail "Failed to create /dir1"
        return 1
    fi
    
    # Create dir2
    if ! ./mkdir.pangya -f "$TEST_IMAGE" "/dir2" 2>&1 > /dev/null; then
        log_fail "Failed to create /dir2"
        return 1
    fi
    
    # Create dir3
    if ! ./mkdir.pangya -f "$TEST_IMAGE" "/dir3" 2>&1 > /dev/null; then
        log_fail "Failed to create /dir3"
        return 1
    fi
    
    # Verify directories exist via ls
    local ls_output
    if ! ls_output=$(./ls.pangya -f "$TEST_IMAGE" "/" 2>&1); then
        log_fail "ls.pangya command failed"
        return 1
    fi
    
    echo "$ls_output" | log_output
    
    if echo "$ls_output" | grep -q "dir1" && echo "$ls_output" | grep -q "dir2" && echo "$ls_output" | grep -q "dir3"; then
        log_pass "Directories created and verified: /dir1, /dir2, /dir3"
        return 0
    else
        log_fail "Directories not found in ls output"
        return 1
    fi
}

# Step 5: Create and copy test files
test_cp() {
    log_test "Step 5: Creating and copying test files..."
    
    # Create test content file
    cat > "$TEST_FILE" << 'EOF'
This is a PangYa Filesystem stress-test file!
It contains multiple lines of text to validate file operations.
Line 3: Testing filesystem integrity and stability.
Line 4: Ensuring proper data storage and retrieval.
Line 5: Validating inode allocation and block management.
EOF

    local test_file_size
    test_file_size=$(wc -c < "$TEST_FILE")
    log_info "  Created test file ($test_file_size bytes)"
    
    # Copy file to dir1
    if ! ./cp.pangya -f "$TEST_IMAGE" "$TEST_FILE" "/dir1/file1.txt" 2>&1 > /dev/null; then
        log_fail "Failed to copy file to /dir1/file1.txt"
        return 1
    fi
    
    # Copy file to dir2
    if ! ./cp.pangya -f "$TEST_IMAGE" "$TEST_FILE" "/dir2/file2.txt" 2>&1 > /dev/null; then
        log_fail "Failed to copy file to /dir2/file2.txt"
        return 1
    fi
    
    # Copy file to dir3
    if ! ./cp.pangya -f "$TEST_IMAGE" "$TEST_FILE" "/dir3/file3.txt" 2>&1 > /dev/null; then
        log_fail "Failed to copy file to /dir3/file3.txt"
        return 1
    fi
    
    # Verify files exist via ls
    local ls_output
    if ! ls_output=$(./ls.pangya -f "$TEST_IMAGE" "/dir1" 2>&1); then
        log_fail "ls.pangya /dir1 command failed"
        return 1
    fi
    
    echo "$ls_output" | log_output
    
    if echo "$ls_output" | grep -q "file1.txt"; then
        log_pass "Files copied and verified"
        return 0
    else
        log_fail "Files not found in directory listing"
        return 1
    fi
}

# Step 6: List directories and inspect file metadata
test_stat_and_ls() {
    log_test "Step 6: Listing directories and inspecting file metadata..."
    
    # List root directory
    log_info "  Listing root directory (/)"
    local ls_output
    if ! ls_output=$(./ls.pangya -f "$TEST_IMAGE" "/" 2>&1); then
        log_fail "ls.pangya / command failed"
        return 1
    fi
    
    echo "$ls_output" | log_output
    
    if ! echo "$ls_output" | grep -q "dir1"; then
        log_fail "Root directory listing missing dir1"
        return 1
    fi
    
    # Stat a file
    log_info "  Stat on /dir1/file1.txt"
    local stat_output
    if ! stat_output=$(./stat.pangya -f "$TEST_IMAGE" "/dir1/file1.txt" 2>&1); then
        log_fail "stat.pangya /dir1/file1.txt command failed"
        return 1
    fi
    
    echo "$stat_output" | log_output
    
    if ! echo "$stat_output" | grep -q "File: /dir1/file1.txt"; then
        log_fail "stat output does not show correct file path"
        return 1
    fi
    
    if ! echo "$stat_output" | grep -q "Type:.*REG"; then
        log_fail "stat output does not show file as REG (regular file)"
        return 1
    fi
    
    log_pass "Directory listing and file metadata verified"
    return 0
}

# Step 7: Check space usage after writes
test_df_after_write() {
    log_test "Step 7: Checking filesystem space usage after file operations..."
    
    local df_output
    if ! df_output=$(./df.pangya -f "$TEST_IMAGE" 2>&1); then
        log_fail "df.pangya command failed"
        return 1
    fi
    
    echo "$df_output" | log_output
    
    # Extract block usage: Match line with 5+ digits at start (1024), space, then 2+ digits (block count)
    # This pattern uniquely identifies the data line, not the header
    local bused
    bused=$(echo "$df_output" | awk '/BLOCK USAGE/,/^$/ {if (/^[[:space:]]*[0-9]{4,}[[:space:]]+[0-9]+/) {print $2; exit}}')
    
    local iused
    iused=$(echo "$df_output" | awk '/INODE USAGE/,/^$/ {if (/^[[:space:]]*[0-9]{3,}[[:space:]]+[0-9]+/) {print $2; exit}}')
    
    if [ -z "$bused" ] || [ -z "$iused" ]; then
        log_fail "Could not parse df output (bused='$bused', iused='$iused')"
        return 1
    fi
    
    # Validate they are numeric
    if ! [[ "$bused" =~ ^[0-9]+$ ]]; then
        log_fail "Block usage value is not numeric: $bused"
        return 1
    fi
    
    if ! [[ "$iused" =~ ^[0-9]+$ ]]; then
        log_fail "Inode usage value is not numeric: $iused"
        return 1
    fi
    
    # We should have used blocks for: metadata, directories, and file data
    # Expected: at least 20+ blocks used (conservative estimate)
    if [ "$bused" -gt 15 ]; then
        log_pass "Block usage reflects file operations ($bused blocks used)"
    else
        log_warn "Block usage seems low: $bused blocks (expected > 15)"
    fi
    
    # We should have at least 5 inodes in use (root + 3 dirs + files)
    if [ "$iused" -ge 5 ]; then
        log_pass "Inode usage reflects directory and file creation ($iused inodes used)"
    else
        log_warn "Inode usage seems low: $iused inodes (expected >= 5)"
    fi
    
    return 0
}

# Step 8: Remove files and directories
test_rm() {
    log_test "Step 8: Removing files and directories..."
    
    # Remove file from dir1
    if ! ./rm.pangya -f "$TEST_IMAGE" "/dir1/file1.txt" 2>&1 > /dev/null; then
        log_fail "Failed to remove /dir1/file1.txt"
        return 1
    fi
    
    # Remove file from dir2
    if ! ./rm.pangya -f "$TEST_IMAGE" "/dir2/file2.txt" 2>&1 > /dev/null; then
        log_fail "Failed to remove /dir2/file2.txt"
        return 1
    fi
    
    # Remove file from dir3
    if ! ./rm.pangya -f "$TEST_IMAGE" "/dir3/file3.txt" 2>&1 > /dev/null; then
        log_fail "Failed to remove /dir3/file3.txt"
        return 1
    fi
    
    # Remove directories
    if ! ./rm.pangya -f "$TEST_IMAGE" "/dir1" 2>&1 > /dev/null; then
        log_fail "Failed to remove /dir1"
        return 1
    fi
    
    if ! ./rm.pangya -f "$TEST_IMAGE" "/dir2" 2>&1 > /dev/null; then
        log_fail "Failed to remove /dir2"
        return 1
    fi
    
    if ! ./rm.pangya -f "$TEST_IMAGE" "/dir3" 2>&1 > /dev/null; then
        log_fail "Failed to remove /dir3"
        return 1
    fi
    
    log_pass "Files and directories removed"
    return 0
}

# Step 9: Verify filesystem is clean
test_df_final() {
    log_test "Step 9: Verifying filesystem is clean..."
    
    local df_output
    if ! df_output=$(./df.pangya -f "$TEST_IMAGE" 2>&1); then
        log_fail "df.pangya command failed"
        return 1
    fi
    
    echo "$df_output" | log_output
    
    # List root directory to verify only . and .. exist
    local ls_output
    if ! ls_output=$(./ls.pangya -f "$TEST_IMAGE" "/" 2>&1); then
        log_fail "ls.pangya / command failed"
        return 1
    fi
    
    echo "$ls_output" | log_output
    
    # After cleanup, only . and .. should remain in root
    # Count non-comment, non-header lines in ls output
    local entry_count
    entry_count=$(echo "$ls_output" | grep "^[[:space:]]*[0-9]" | wc -l)
    
    if [ "$entry_count" -eq 2 ]; then
        log_pass "Filesystem is clean (only . and .. in root)"
        return 0
    else
        log_fail "Root directory has $entry_count entries (expected 2 for . and ..)"
        return 1
    fi
}

################################################################################
# Main Execution
################################################################################

main() {
    echo ""
    echo "====== PangYa Filesystem Stress-Test Utility ======"
    echo "Test Image: $TEST_IMAGE"
    echo "Filesystem Size: ${FILESYSTEM_SIZE_KB}KB with ${INODE_COUNT} inodes"
    echo ""
    
    # Verify tools are available
    for tool in mkfs.pangya chfs.pangya df.pangya mkdir.pangya cp.pangya stat.pangya ls.pangya rm.pangya; do
        if [ ! -f "./$tool" ]; then
            log_fail "Required tool not found: $tool"
            log_info "Please build the project with 'make' first"
            exit 1
        fi
    done
    
    # Run all tests
    test_mkfs || return 1
    test_chfs_modes || return 1
    test_df_initial || return 1
    test_mkdir || return 1
    test_cp || return 1
    test_stat_and_ls || return 1
    test_df_after_write || return 1
    test_rm || return 1
    test_df_final || return 1
    
    # Summary
    echo ""
    echo "====== Test Summary ======"
    echo -e "Total Tests:  $TOTAL_TESTS"
    echo -e "Passed:       ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed:       ${RED}$FAILED_TESTS${NC}"
    echo ""
    
    if [ "$FAILED_TESTS" -eq 0 ]; then
        echo -e "${GREEN}All stress-test steps completed successfully!${NC}"
        echo -e "${GREEN}The filesystem is functional and intact after heavy I/O operations.${NC}"
        return 0
    else
        echo -e "${RED}Some tests failed. Review output above for details.${NC}"
        return 1
    fi
}

main

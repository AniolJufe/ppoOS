#include <lib/string.h>
#include "string_test.h"
#include <lib/stddef.h> // For size_t, NULL
// Assume serial_printf is available for basic test output
// This would typically come from something like <serial.h>
// If not available, these tests would need to be adapted.
// For the purpose of this subtask, we'll assume it exists.
// Example: void serial_printf(const char *fmt, ...);

// A simple assert-like macro for testing
// In a real kernel, you'd have a more robust way to report errors.
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_STR_EQUALS(expected, actual, message) \
    if (strcmp(expected, actual) == 0) { \
        tests_passed++; \
        /* serial_printf("PASS: %s\n", message); */ \
    } else { \
        tests_failed++; \
        /* serial_printf("FAIL: %s (Expected: '%s', Got: '%s')\n", message, expected, actual); */ \
    }

void test_ultoa_hex(void) {
    char buffer[32]; // Buffer needs to be large enough for ulong hex + null terminator

    tests_passed = 0;
    tests_failed = 0;

    // Test case 1: Zero value
    ultoa_hex(0, buffer);
    ASSERT_STR_EQUALS("0", buffer, "ultoa_hex(0)");

    // Test case 2: Small value
    ultoa_hex(0x1, buffer);
    ASSERT_STR_EQUALS("1", buffer, "ultoa_hex(0x1)");

    // Test case 3: Typical value
    ultoa_hex(0x123abc, buffer);
    ASSERT_STR_EQUALS("123abc", buffer, "ultoa_hex(0x123abc)");

    // Test case 4: Value with leading zeros in hex representation (but not at the start of string)
    ultoa_hex(0x0000F0F0, buffer);
    ASSERT_STR_EQUALS("f0f0", buffer, "ultoa_hex(0x0000F0F0)");
    
    // Test case 5: Larger value
    ultoa_hex(0xABCDEF0123456789, buffer);
    ASSERT_STR_EQUALS("abcdef0123456789", buffer, "ultoa_hex(0xABCDEF0123456789)");

    // Test case 6: Max ulong value (assuming 64-bit unsigned long)
    ultoa_hex(0xFFFFFFFFFFFFFFFF, buffer);
    ASSERT_STR_EQUALS("ffffffffffffffff", buffer, "ultoa_hex(0xFFFFFFFFFFFFFFFF)");
    
    // Test case 7: A value that might have issues with internal digit processing
    ultoa_hex(0x1000000000000000, buffer);
    ASSERT_STR_EQUALS("1000000000000000", buffer, "ultoa_hex(0x1000000000000000)");

    // serial_printf("ultoa_hex tests complete. Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    // For now, we can't actually print, so these variables are for potential debugger inspection.
    // A real test harness would handle reporting.
}

#!/usr/bin/env python3
"""
NimBLE Library Patcher for Nintendo Switch 5ms Connection Interval Support

This script patches the ESP-IDF NimBLE controller library (libble_app.a) to allow
connection intervals down to 5ms (4 * 1.25ms) instead of the standard 7.5ms minimum.

This is required for Nintendo Switch Pro Controller compatibility, as NS2 uses
a non-standard 5ms connection interval during reconnection.

Usage:
    python patch_nimble_lib.py [--idf-path IDF_PATH] [--target TARGET]

Targets supported:
    - esp32c6
    - esp32c61 (default)
    - esp32c2
    - esp32c3
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile


def find_objdump(idf_path: str) -> str:
    """Find the riscv32-esp-elf-objdump executable."""
    # Try common locations
    possible_paths = [
        os.path.join(idf_path, "..", ".espressif", "tools", "riscv32-esp-elf", "esp-14.2.0_20251107", "riscv32-esp-elf", "bin", "riscv32-esp-elf-objdump.exe"),
        os.path.join(idf_path, "..", ".espressif", "tools", "riscv32-esp-elf", "esp-14.2.0_20251107", "riscv32-esp-elf", "bin", "riscv32-esp-elf-objdump"),
        os.path.join(os.environ.get("IDF_TOOLS_PATH", ""), "tools", "riscv32-esp-elf", "esp-14.2.0_20251107", "riscv32-esp-elf", "bin", "riscv32-esp-elf-objdump.exe"),
        os.path.join(os.environ.get("IDF_TOOLS_PATH", ""), "tools", "riscv32-esp-elf", "esp-14.2.0_20251107", "riscv32-esp-elf", "bin", "riscv32-esp-elf-objdump"),
        "riscv32-esp-elf-objdump",
    ]

    for path in possible_paths:
        if path and os.path.isfile(path):
            return path

    # Try to find in PATH
    for cmd in ["riscv32-esp-elf-objdump", "riscv32-esp-elf-objdump.exe"]:
        if shutil.which(cmd):
            return cmd

    raise FileNotFoundError("Could not find riscv32-esp-elf-objdump. Please ensure ESP-IDF toolchain is installed.")


def find_ar(idf_path: str) -> str:
    """Find the riscv32-esp-elf-ar executable."""
    possible_paths = [
        os.path.join(idf_path, "..", ".espressif", "tools", "riscv32-esp-elf", "esp-14.2.0_20251107", "riscv32-esp-elf", "bin", "riscv32-esp-elf-ar.exe"),
        os.path.join(idf_path, "..", ".espressif", "tools", "riscv32-esp-elf", "esp-14.2.0_20251107", "riscv32-esp-elf", "bin", "riscv32-esp-elf-ar"),
        os.path.join(os.environ.get("IDF_TOOLS_PATH", ""), "tools", "riscv32-esp-elf", "esp-14.2.0_20251107", "riscv32-esp-elf", "bin", "riscv32-esp-elf-ar.exe"),
        os.path.join(os.environ.get("IDF_TOOLS_PATH", ""), "tools", "riscv32-esp-elf", "esp-14.2.0_20251107", "riscv32-esp-elf", "bin", "riscv32-esp-elf-ar"),
        "riscv32-esp-elf-ar",
    ]

    for path in possible_paths:
        if path and os.path.isfile(path):
            return path

    for cmd in ["riscv32-esp-elf-ar", "riscv32-esp-elf-ar.exe"]:
        if shutil.which(cmd):
            return cmd

    raise FileNotFoundError("Could not find riscv32-esp-elf-ar. Please ensure ESP-IDF toolchain is installed.")


def get_library_path(idf_path: str, target: str) -> str:
    """Get the path to libble_app.a for the specified target."""
    # NOTE: Only NimBLE-based controllers are supported.
    # ESP32-S3 and ESP32-C3 use a closed-source Bluedroid/RivieraWaves controller
    # (libbtdm_app.a) instead of the open-source NimBLE controller (libble_app.a).
    # This patch only works with NimBLE controllers.
    lib_paths = {
        "esp32c6": os.path.join(idf_path, "components", "bt", "controller", "lib_esp32c6", "esp32c6-bt-lib", "esp32c6", "libble_app.a"),
        "esp32c61": os.path.join(idf_path, "components", "bt", "controller", "lib_esp32c6", "esp32c6-bt-lib", "esp32c61", "libble_app.a"),
        "esp32c2": os.path.join(idf_path, "components", "bt", "controller", "lib_esp32c2", "esp32c2-bt-lib", "libble_app.a"),
        "esp32h2": os.path.join(idf_path, "components", "bt", "controller", "lib_esp32h2", "esp32h2-bt-lib", "libble_app.a"),
    }

    # Check for unsupported targets with clear error messages
    unsupported_targets = {
        "esp32s3": "uses a closed-source Bluedroid controller (libbtdm_app.a) instead of NimBLE",
        "esp32c3": "uses a closed-source Bluedroid controller (libbtdm_app.a) instead of NimBLE",
    }

    if target in unsupported_targets:
        raise ValueError(
            f"Target '{target}' is not supported: {unsupported_targets[target]}. "
            f"This patch only works with targets that use the open-source NimBLE controller. "
            f"Supported targets: {', '.join(lib_paths.keys())}"
        )

    if target not in lib_paths:
        raise ValueError(f"Unsupported target: {target}. Supported: {list(lib_paths.keys())}")

    return lib_paths[target]


def extract_object(ar_path: str, lib_path: str, obj_name: str, output_dir: str) -> str:
    """Extract an object file from the library."""
    obj_path = os.path.join(output_dir, obj_name)
    cmd = [ar_path, "x", lib_path, obj_name]
    subprocess.run(cmd, cwd=output_dir, check=True)
    return obj_path


def replace_object(ar_path: str, lib_path: str, obj_path: str):
    """Replace an object file in the library."""
    cmd = [ar_path, "r", lib_path, obj_path]
    subprocess.run(cmd, check=True)


def find_and_patch_interval_check(obj_path: str) -> int:
    """
    Find and patch the connection interval check in the object file.

    Changes:
        addi a5, a4, -6   (ffa70793) -> addi a5, a4, -4   (ffc70793)

    This changes the minimum connection interval from 6 (7.5ms) to 4 (5ms).
    """
    with open(obj_path, 'rb') as f:
        data = bytearray(f.read())

    # Pattern to find: addi a5, a4, -6
    # RISC-V encoding: ffa70793
    old_pattern = bytes([0x93, 0x07, 0xa7, 0xff])

    # New instruction: addi a5, a4, -4
    # RISC-V encoding: ffc70793
    new_pattern = bytes([0x93, 0x07, 0xc7, 0xff])

    count = 0
    i = 0
    while i < len(data) - 3:
        if data[i:i+4] == old_pattern:
            data[i:i+4] = new_pattern
            count += 1
            print(f"  Patched offset 0x{i:04x}: addi a5,a4,-6 -> addi a5,a4,-4")
        i += 1

    if count == 0:
        print("  WARNING: Pattern not found. Library may already be patched or version mismatch.")
        return 0

    with open(obj_path, 'wb') as f:
        f.write(data)

    return count


def verify_patch(objdump_path: str, lib_path: str) -> bool:
    """Verify the patch was applied correctly."""
    try:
        # Disassemble and check for the new instruction
        result = subprocess.run(
            [objdump_path, "-d", lib_path],
            capture_output=True,
            text=True
        )

        # Look for the patched instruction in r_ble_ll_conn_slave_start
        lines = result.stdout.split('\n')
        in_slave_start = False
        found_patch = False

        for line in lines:
            if '<r_ble_ll_conn_slave_start>:' in line:
                in_slave_start = True
            elif in_slave_start and '<r_' in line and 'slave_start' not in line:
                break
            elif in_slave_start and 'addi' in line and 'a4,-4' in line:
                found_patch = True
                print(f"  Verified: {line.strip()}")
                break

        return found_patch
    except Exception as e:
        print(f"  Verification warning: {e}")
        return False


def backup_library(lib_path: str) -> str:
    """Create a backup of the original library."""
    backup_path = lib_path + ".original"
    if not os.path.exists(backup_path):
        shutil.copy2(lib_path, backup_path)
        print(f"  Created backup: {backup_path}")
    return backup_path


def restore_library(lib_path: str) -> bool:
    """Restore the original library from backup."""
    backup_path = lib_path + ".original"
    if os.path.exists(backup_path):
        shutil.copy2(backup_path, lib_path)
        print(f"  Restored from backup: {backup_path}")
        return True
    else:
        print("  ERROR: No backup found!")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Patch ESP-IDF NimBLE library for Nintendo Switch 5ms connection interval support"
    )
    parser.add_argument(
        "--idf-path",
        default=os.environ.get("IDF_PATH"),
        help="Path to ESP-IDF installation (default: IDF_PATH environment variable)"
    )
    parser.add_argument(
        "--target",
        default="esp32c61",
        choices=["esp32c6", "esp32c61", "esp32c2", "esp32h2"],
        help="Target chip (default: esp32c61)"
    )
    parser.add_argument(
        "--restore",
        action="store_true",
        help="Restore the original library from backup"
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="Only verify if the patch is applied"
    )

    args = parser.parse_args()

    if not args.idf_path:
        print("ERROR: IDF_PATH not set. Use --idf-path or set environment variable.")
        sys.exit(1)

    args.idf_path = os.path.abspath(args.idf_path)

    try:
        lib_path = get_library_path(args.idf_path, args.target)
        print(f"Library: {lib_path}")

        if not os.path.exists(lib_path):
            print(f"ERROR: Library not found: {lib_path}")
            sys.exit(1)

        if args.restore:
            if restore_library(lib_path):
                print("Restore completed successfully!")
            else:
                sys.exit(1)
            return

        ar_path = find_ar(args.idf_path)
        objdump_path = find_objdump(args.idf_path)

        print(f"Tools: ar={ar_path}, objdump={objdump_path}")

        if args.verify_only:
            print("Verifying patch...")
            if verify_patch(objdump_path, lib_path):
                print("Patch is applied!")
            else:
                print("Patch NOT found!")
            return

        # Create backup
        backup_library(lib_path)

        # Work in temp directory
        with tempfile.TemporaryDirectory() as tmpdir:
            print(f"Working in: {tmpdir}")

            # Extract object file
            obj_name = "ble_ll_conn.c.o"
            obj_path = extract_object(ar_path, lib_path, obj_name, tmpdir)
            print(f"Extracted: {obj_name}")

            # Patch the object file
            count = find_and_patch_interval_check(obj_path)

            if count > 0:
                # Replace in library
                replace_object(ar_path, lib_path, obj_path)
                print(f"Replaced {obj_name} in library")

                # Verify
                print("Verifying patch...")
                if verify_patch(objdump_path, lib_path):
                    print("\n✓ Patch applied successfully!")
                    print("  Nintendo Switch 5ms connection interval is now supported.")
                else:
                    print("\n⚠ Warning: Could not verify patch!")
            else:
                print("\n✗ No patches applied. Library may already be patched.")

    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()

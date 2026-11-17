#!/usr/bin/env python3
"""Fails if a Windows executable statically imports APIs newer than Windows 7.

SnipPin must load on Windows 7 SP1: anything newer has to be resolved at
runtime through src/platform/ApiShim.cpp. Used by CI on the MinGW builds.

Usage: python3 tools/check_win7_imports.py <objdump> <exe> [<exe> ...]
"""
import re
import subprocess
import sys

# Functions introduced after Windows 7 that are easy to call by accident.
POST_WIN7 = {
    # user32 (8.1 / 10)
    "GetDpiForWindow", "GetDpiForSystem", "SetProcessDpiAwarenessContext", "SetThreadDpiAwarenessContext",
    "GetThreadDpiAwarenessContext", "GetWindowDpiAwarenessContext", "AreDpiAwarenessContextsEqual",
    "SystemParametersInfoForDpi", "GetSystemMetricsForDpi", "AdjustWindowRectExForDpi",
    "EnableNonClientDpiScaling", "LogicalToPhysicalPointForPerMonitorDPI", "GetPointerInfo",
    "GetDisplayAutoRotationPreferences",
    # shcore (8.1)
    "GetDpiForMonitor", "SetProcessDpiAwareness", "GetProcessDpiAwareness", "GetScaleFactorForMonitor",
    # kernel32 (8+)
    "GetSystemTimePreciseAsFileTime", "SetThreadDescription", "GetThreadDescription", "CreateFile2",
    "GetCurrentPackageId", "GetCurrentPackageFullName", "PrefetchVirtualMemory", "DiscardVirtualMemory",
    "SetDefaultDllDirectories", "AddDllDirectory", "RemoveDllDirectory", "GetOverlappedResultEx",
    "WaitOnAddress", "WakeByAddressSingle", "WakeByAddressAll", "CreateFileMappingFromApp",
    "GetFirmwareType", "SetProcessMitigationPolicy", "GetProcessMitigationPolicy",
    # dwmapi (8+)
    "DwmRenderGesture", "DwmTetherContact", "DwmShowContact",
}
FORBIDDEN_DLLS = {"shcore.dll", "api-ms-win-core-winrt-l1-1-0.dll", "vcruntime140.dll", "msvcp140.dll"}


def imports(objdump: str, exe: str):
    out = subprocess.run([objdump, "-p", exe], check=True, capture_output=True, text=True).stdout
    dll = None
    for line in out.splitlines():
        m = re.search(r"DLL Name: (\S+)", line)
        if m:
            dll = m.group(1)
            yield dll, None
            continue
        m = re.match(r"\s+[0-9a-fA-F]+\s+\d+\s+(\w+)", line)
        if m and dll:
            yield dll, m.group(1)


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    objdump, exes = sys.argv[1], sys.argv[2:]
    failed = False
    for exe in exes:
        count = 0
        for dll, func in imports(objdump, exe):
            if func is None:
                if dll.lower() in FORBIDDEN_DLLS:
                    print(f"{exe}: forbidden DLL dependency {dll}")
                    failed = True
                continue
            count += 1
            if func in POST_WIN7:
                print(f"{exe}: post-Windows-7 import {dll}!{func}")
                failed = True
        print(f"{exe}: {count} imports checked")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

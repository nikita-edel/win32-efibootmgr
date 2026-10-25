# win32-efibootmgr

efibootmgr for Windows. Reads and edits UEFI boot entries (`BootXXXX`, `BootOrder`, `BootNext`, `Timeout`) through `Get/SetFirmwareEnvironmentVariableExW`.

Needs Administrator, UEFI firmware, Windows 8+.

## Usage

```
efibootmgr                         list entries
efibootmgr -v                      list entries with device paths
efibootmgr -o 0001,0000,0003       set BootOrder
efibootmgr -n 0003                 boot 0003 once on next reboot
efibootmgr -t 5                    set firmware timeout
efibootmgr -b 0004 -B              delete Boot0004 (also removed from BootOrder)
efibootmgr -b 0004 -a / -A         set entry active / inactive
efibootmgr -O / -N / -T            delete BootOrder / BootNext / Timeout
```

Create an entry (GPT only, new entry goes first in BootOrder):

```
efibootmgr -c -d \\.\PhysicalDrive0 -p 1 -l \EFI\ubuntu\shimx64.efi -L ubuntu
```

Boot numbers are hex. Partition numbers are 1-based. `-h` for full help.

## Build
Need a C Compiler

```
cmake -B build
cmake --build build
```

For the mingw toolchain (to compile from e.g. linux)
```
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
cmake --build build
```

Use `-DMINGW_PREFIX=<triple>` if the toolchain isn't `x86_64-w64-mingw32`.

## Notes

- Entries not in BootOrder are only listed if numbered 0000-00FF.
- No backup is made. Write down `efibootmgr -v` output before changing things.

# MPS-DUMP

A lightweight, [hexdump](https://man7.org/linux/man-pages/man1/hexdump.1.html)-like [CLI](https://github.com/resources/articles/what-is-a-cli) program to dump the contents of an MPS binary data file. 

Leverages the [MPS Protocol](https://github.com/csooriyakumaran/scanivalve-mps-protocol.git) firmware version v4.01. 

See the [Scanivalve Hardware, Software, and User Manual](https://scanivalve.com/wp-content/uploads/2026/03/MPS4200_v401_260304.pdf)
## Usage

```powershell
mps-dump.exe [FILE]

# e.g. 
mps-dump.exe test.dat

# dump to file
mps-dump.exe test.dat > test.out
```

## Example output

A typical output line has four parts (the first three mirroring the traditional hexdump output formatting):
```text
-OFFSET-  ----------------- 4x 32-bit words ----------------   | ----- ASCII ---- |  ------------------------------ ANNOTATIONS ------------------------------------
```

1. Byte offset in hex into the file
2. 16 bytes (4 words of 32 bits each) direct hex dump from the file
3. ASCII representation of those 16 bytes
4. Parsed annotation based on the packet type

For example, the first few frames from an MPS 4216 with the following settings:
```Telnet
> SET UNITS PA
> SET SIM 0x00
> SET RATE 100
```

```powershell
mps-dump.exe .\test-4216.dat | more
```

```text
00000000  5D 00 00 00  01 00 00 00  00 00 00 00  00 00 00 00   | ]............... |   TYP:   0x0000005D;  NO.:   0000000001;  SEC:   0000000000;  +NS:    000000000;
00000010  19 00 00 42  2F 00 00 42  D7 FF FF 41  58 00 00 42   | ...B/..B...AX..B |   T01:  +3.2000e+01;  T02:  +3.2000e+01;  T03:  +3.2000e+01;  T04:  +3.2000e+01;
00000020  1F 8A 28 3F  CB 8B 9C 3F  61 73 0A BF  30 57 13 40   | ..(?...?as..0W.@ |   P01:  +6.5836e-01;  P02:  +1.2230e+00;  P03:  -5.4082e-01;  P04:  +2.3022e+00;
00000030  B2 1F FF BF  AD 7B AF 3E  BA D6 46 BF  6F 6A CA BF   | .....{.>..F.oj.. |   P05:  -1.9932e+00;  P06:  +3.4274e-01;  P07:  -7.7671e-01;  P08:  -1.5814e+00;
00000040  4A 73 6B BF  45 DE 90 BF  4F 4B E8 BE  77 ED E4 BD   | Jsk.E...OK..w... |   P09:  -9.1973e-01;  P10:  -1.1318e+00;  P11:  -4.5370e-01;  P12:  -1.1178e-01;
00000050  76 9F D0 3F  8F 7A B7 3F  B9 C7 D1 3F  E5 32 56 BF   | v..?.z.?...?.2V. |   P13:  +1.6299e+00;  P14:  +1.4334e+00;  P15:  +1.6389e+00;  P16:  -8.3671e-01;
00000060  5D 00 00 00  02 00 00 00  00 00 00 00  00 E1 F5 05   | ]............... |   TYP:   0x0000005D;  NO.:   0000000002;  SEC:   0000000000;  +NS:    100000000;
00000070  A5 FF FF 41  94 FF FF 41  AD FF FF 41  F9 FF FF 41   | ...A...A...A...A |   T01:  +3.2000e+01;  T02:  +3.2000e+01;  T03:  +3.2000e+01;  T04:  +3.2000e+01;
00000080  7D 56 98 BF  0A 2A B5 BF  5E 07 8C BF  3F 36 BE BD   | }V...*..^...?6.. |   P01:  -1.1901e+00;  P02:  -1.4153e+00;  P03:  -1.0940e+00;  P04:  -9.2877e-02;
00000090  07 7F 17 BE  88 15 87 3E  C3 F9 0C 3F  EA 8C 94 BE   | .......>...?.... |   P05:  -1.4795e-01;  P06:  +2.6384e-01;  P07:  +5.5069e-01;  P08:  -2.9014e-01;
000000a0  0A 2A B5 3F  A2 BC 40 3F  E8 5F 35 BF  7B 29 39 3F   | .*.?..@?._5.{)9? |   P09:  +1.4153e+00;  P10:  +7.5288e-01;  P11:  -7.0849e-01;  P12:  +7.2329e-01;
000000b0  FC CE 9D BE  6B 23 04 BF  7F 74 02 BF  A4 DA 2A 3E   | ....k#...t....*> |   P13:  -3.0822e-01;  P14:  -5.1617e-01;  P15:  -5.0959e-01;  P16:  +1.6685e-01;
-- More --
```


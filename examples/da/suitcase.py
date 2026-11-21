#!/usr/bin/env python3
# SPDX-License-Identifier: 0BSD
# Copyright (C) 2026 by the FinderDrop authors (LICENSE).
"""Wrap the desk accessory's DRVR in a Font/DA Mover suitcase.

build.sh makes FinderDrop.drvr, the bare DRVR resource, which no Macintosh
can install as it is.  What Font/DA Mover opens is a suitcase: a file of
type 'DFIL', creator 'DMOV', whose resource fork holds the DRVR, named
with the NUL byte in front that every desk accessory's name has.  This
writes one, as a MacBinary II file, so that it can be copied to a disk
with its resource fork and its type intact.

    suitcase.py FinderDrop.drvr "FinderDrop.bin" [file name] [DA name]

The DRVR goes in as ID 12, purgeable; Font/DA Mover gives it whatever ID
is free when it is installed.  Python's standard library only.
"""
import struct
import sys
import time


def resource_fork(rtype, rid, name, attrs, data):
    """A resource fork holding one resource."""
    body = struct.pack(">I", len(data)) + data
    names = bytes([len(name)]) + name
    type_list = struct.pack(">H", 0) + rtype + struct.pack(">HH", 0, 2 + 8)
    ref = struct.pack(">hH", rid, 0) + bytes([attrs]) + b"\0\0\0" + b"\0" * 4
    map_body = type_list + ref
    map_ = (b"\0" * 16 + b"\0" * 4 + b"\0" * 2 + struct.pack(">H", 0)
            + struct.pack(">HH", 28, 28 + len(map_body)) + map_body + names)
    data_off = 256
    map_off = data_off + len(body)
    head = struct.pack(">IIII", data_off, map_off, len(body), len(map_))
    return head + b"\0" * (256 - 16) + body + head + map_[16:]


def crc16(b):
    """The CRC-16/XMODEM MacBinary II puts in its header."""
    crc = 0
    for byte in b:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def macbinary(fname, ftype, creator, data_fork, rsrc_fork):
    h = bytearray(128)
    n = fname.encode("mac_roman")
    h[1] = len(n)
    h[2:2 + len(n)] = n
    h[65:69] = ftype
    h[69:73] = creator
    struct.pack_into(">II", h, 83, len(data_fork), len(rsrc_fork))
    now = int(time.time()) + 2082844800          # seconds since 1904
    struct.pack_into(">II", h, 91, now, now)
    h[122] = 129                                 # MacBinary II
    h[123] = 129
    struct.pack_into(">H", h, 124, crc16(bytes(h[:124])))

    def pad(b):
        return b + b"\0" * (-len(b) % 128)
    return bytes(h) + pad(data_fork) + pad(rsrc_fork)


def main():
    if len(sys.argv) not in (3, 4, 5):
        raise SystemExit(__doc__)
    drvr = open(sys.argv[1], "rb").read()
    fname = sys.argv[3] if len(sys.argv) > 3 else "FinderDrop"
    daname = sys.argv[4] if len(sys.argv) > 4 else "FinderDrop"
    fork = resource_fork(b"DRVR", 12, b"\0" + daname.encode("mac_roman"),
                         0x20, drvr)
    with open(sys.argv[2], "wb") as f:
        f.write(macbinary(fname, b"DFIL", b"DMOV", b"", fork))


if __name__ == "__main__":
    main()

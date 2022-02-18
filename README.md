# BEJ encode

Binary Encoded JSON(BEJ) which written in C.

## Introduction

Binary Encoded JSON(BEJ) is defined in [DSP0218_1.1.0](https://www.dmtf.org/sites/default/files/standards/documents/DSP0218_1.1.0.pdf) which is desined by DMTF for PLDM RDE.

BEJ is a compact binary representation of JSON that is easy for low-power embedded processors to encode, decode, and manipulate. This is important because these ASICs typically have highly limited memory and power budgets; they must be able to process data quickly and efficiently. Naturally, it must be possible to fully reconstruct a textual JSON
message from its BEJ encoding.

This code only implement the encode side for BEJ.

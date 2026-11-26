# RAID5 solution

The full specification is in `/workspace/data/123/RAID5/RAID5.pdf`.
The submitted header implements the supplied controller interface.

Stripe `s` has parity on disk `n - 1 - s % n`. Logical blocks are
assigned to the remaining disks in increasing disk order. XOR of all
physical blocks in a stripe is zero. Normal writes preserve this invariant
using `new_parity = old_parity XOR old_data XOR new_data`.

A missing data block is reconstructed by XORing every surviving block.
Writes to missing data recompute its parity from the new data and the
remaining data blocks. If parity is missing, only the data block needs
writing. Replacement reconstructs every block on the replaced disk before
normal operation resumes. Shutdown closes the caller-owned streams without
deleting them.

Normal reads use one block read; normal writes use two reads and two writes.
Rebuilding uses exactly `(n - 1) * stripes` reads and `stripes` writes.
Normal operations take O(block_size) time, degraded operations take at most
O(n * block_size), and replacement takes O(n * stripes * block_size).
Temporary buffers take O(block_size) memory; no file data is cached between
operations.

## Local validation

The provided `raid5.cpp` scenarios pass. The randomized regression test
covers 3 through 10 disks, block sizes 1, 7, and 4096, initial zero contents,
overwrites, every failed disk, degraded reads and writes, replacement of
failed disks, and replacement of healthy disks. It passes with address and
undefined-behavior sanitizers:

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g \
  -I. -I/workspace/data/123/RAID5 tests/raid5_test.cpp -o /tmp/raid5_test
/tmp/raid5_test
```

The submission client was repaired to add the `submit_code` method already
called by its command-line interface.

## Online judge result

- Submission: `879622` (first and only attempt).
- Submitted implementation commit: `dbb64d7`.
- Result: Accepted, 100/100; all 15 test points and all five subtasks passed.
- Reported total time: 1505 ms; peak memory: 4,182,016 bytes.
- The POST response timed out, but the submission was recovered from the
  submission listing and checked with the client's `status` command. No
  duplicate submission was made.

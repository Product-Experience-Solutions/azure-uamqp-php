# uAMQP receiver credit support

`azure-uamqp-c-manual-receiver-credit.patch` adds explicit receiver credit and
drain support to Azure uAMQP C. It is tested against upstream commit
`91efcbd9c4483bdd4608470f539af96c18c74a11`. The extension requires the patched
headers and library together. Rebuild the library before compiling the extension;
an older installed library cannot satisfy the new native symbols.

`setup.sh` applies this tracked patch before building uAMQP. For an existing
source checkout, run `bash scripts/apply-uamqp-patches.sh [source-directory]`.
Applying it twice is safe. An incompatible or partially applied patch causes a
clear failure before the build. Generated files under `libs-build` are not the
authoritative copy of this change.

The existing `link_set_max_link_credit` and automatic refill behavior remain
compatible for other library callers. `link_set_receiver_credit(link, credit)`
selects manual credit: before attachment it sets the initial allowance, and
after attachment it replaces remaining credit and sends FLOW. Credit is never
refilled implicitly. Setting zero cannot recall transfers already in flight;
their callbacks still run and credit cannot underflow.

`link_drain(link)` sends FLOW with drain enabled while retaining the outstanding
allowance. The caller must keep dispatching I/O and keep callbacks alive so the
sender can deliver or return the unused credit. `link_get_drain_complete` reports
completion after a matching sender FLOW with drain enabled, zero credit, and a
valid delivery count. A bounded timeout is still needed when a peer disconnects
or does not answer drain. New credit is refused while drain is pending.

`make test-credit` compiles the actual patched `link.c` with test replacements
only for the session FLOW and disposition transport. It checks finite grants,
continuous explicit replenishment, multipart deliveries, callbacks during
drain, returned credit, serial-number wraparound, and compatibility with the
default automatic behavior. `UAMQP_SOURCE_DIR` selects a nondefault checkout.

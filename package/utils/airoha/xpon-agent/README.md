# xPON setup and observation

`xpon-agent` supplies native ucode bindings and an rpcd controller for Linux
xPON. The kernel owns registration, OMCI and Ethernet OAM protocol execution.
The controller applies local settings and reads runtime objects. It does not
receive OAMPDUs, implement authentication or maintain protocol timers.

`luci-app-xpon` installs the top-level **xPON** menu:

- **Configuration**: local device selection and OMCI identity/policy.
- **OMCI**: channel status, identity profiles and the existing MIB views.
- **EPON**: MPCP links, OAM transport counters and local link enable settings.
- **Status**: common registration/optical state, endpoints and Ethernet UNIs.

## Kernel interfaces

The native modules are `xpon.so` and `xpon_omci.so`. They share the Netlink
transport helpers, but use separate kernel families and identifier namespaces:

- `xpon`, UAPI version 2: requests identify a PON datapath by ifindex.
- `omci`, UAPI version 14: requests identify an OMCI channel by device ID.

The copied UAPI headers must stay synchronized with the target kernel. The
xPON binding rejects unsupported family versions, handles complete multipart
dumps, rejects interrupted/truncated replies and leaves unavailable attributes
absent. OAM counters are decimal strings so ubus/JavaScript retain all 64 bits.

The common state API currently does not provide OAM discovery, negotiated
profile, authentication or service readiness. `CAP_OAM` means raw transport
support. It must not be interpreted as an operational in-kernel OAM agent.
Profile/credential controls can be added when that agent exposes its setup API.
REPORT configuration is available through ubus/CLI, in 16 ns time quanta.
This package does not add kernel protocol support or change DSA.

## Configuration and upgrade

`/etc/config/xpon` is the only active local configuration. The first install
copies `/etc/config/omci` if it exists, otherwise it uses the packaged defaults.
It preserves an existing xPON configuration and leaves the legacy file as a
backup. New files are created atomically with mode 0600. The configuration is
also listed in the sysupgrade keep list.

The compatibility packages `omci-agent` and `luci-app-omci` retain old image
selections and depend on the renamed packages. Upgrade those installed packages
together with the new packages. The OMCI compatibility package retains ownership
of the old `omci.so` alias, `omcictl` wrapper and rpcd placeholder; the new package
does not take over their file paths. The placeholder does not register a second
controller. `xpon.agent` also registers the compatibility `omci.agent` ubus object.

Example configuration additions:

```uci
config agent 'main'
	option netdev 'pon0'
	option device '0'
	option enabled '1'

config link 'primary'
	option id '1'
	option enabled '1'
```

`netdev` is the PON network device name, resolved at runtime. Empty means
automatically select the only registered xPON datapath; multiple devices require
an explicit selection. `device` is the separate OMCI ID. The controller checks
that the chosen OMCI channel belongs to the selected datapath before applying
OMCI settings. OMCI settings are skipped in EPON mode. On an older OMCI-only
kernel, the original OMCI setup remains usable with an empty `netdev`.

Link IDs are local registration instances, not wire LLIDs assigned by the OLT.
Only explicitly configured link entries are changed. Removing an entry leaves
its current kernel setting unchanged; set `enabled` to zero to disable it.
All entries are validated first. On failure, previously changed administrative
settings are restored, and a rollback failure is included in the error reply.
Restoring intent does not restore an interrupted MPCP session synchronously.

rpcd applies UCI on startup, `config.change`, or detection of a direct UCI commit.
It avoids rewriting equal OMCI values and equal link administration settings.
A changed/missing backend ifindex invalidates the applied signature; failures
are retried with the existing 30 second backoff. A kernel replacement that reuses
the same ifindex without an observed disappearance cannot be detected reliably
without a future kernel generation attribute. An explicit `apply` also recovers
that case. Kernel events and protocol execution do not depend on rpcd polling.

## ubus and CLI

Common `xpon.agent` methods are `family`, `devices`, `status`, `capabilities`,
`links`, `endpoints`, `unis`, `link_set`, `report_get`, `report_set` and `apply`.
The existing OMCI methods are available with an `omci_` prefix. The legacy
`omci.agent` object delegates to the same implementation. Runtime mutations do
not commit UCI; LuCI edits `/etc/config/xpon` for persistent settings.

```sh
xponctl devices
xponctl --netdev pon0 status
xponctl --netdev pon0 links
xponctl --netdev pon0 endpoints
xponctl --netdev pon0 unis
xponctl apply
xponctl --netdev pon0 link-set 2 1
xponctl --netdev pon0 report-get 1 0 0
xponctl --netdev pon0 report-set 1 0 0 1 1000
xponctl --device 0 omci status
xponctl --json --device 0 omci config list
```

Common CLI replies are JSON. Existing OMCI human-readable output is retained;
`--json` selects JSON for OMCI commands. A negative/default OMCI device argument
uses the configured device. Explicit zero still selects channel zero.

LuCI ACLs separate observations from mutations. The native binding performs
requests directly; rpcd does not execute shell commands or helper processes.

## Host verification

With a host ucode executable:

```sh
python3 tests/run.py --ucode /path/to/ucode
```

The controller tests use bounded fake providers to cover idempotence, device
selection, mode dispatch, backend replacement and rollback. CLI routing and
migration tests also run without optical hardware.

Build `tests/native.c` with the host ucode headers and `-lucode`, then run it.
It exercises the production scalar decoder and multipart receiver, including
high mode bits, 64-bit counters, malformed attributes, interrupted dumps and
completion errors. Build both modules with the package's warning flags.
Hardware registration and OLT interoperability require target testing.

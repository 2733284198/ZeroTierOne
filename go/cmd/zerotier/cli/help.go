/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package cli

import (
	"fmt"
	"zerotier/pkg/zerotier"
)

func Help() {
	fmt.Printf(`ZeroTier Network Hypervisor Service Version %d.%d.%d
(c)2013-2020 ZeroTier, Inc.
Licensed under the ZeroTier BSL (see LICENSE.txt)

Usage: zerotier [-options] <command> [command args]

Global Options:
  -j                                     Output raw JSON where applicable
  -p <path>                              Use alternate base path
  -t <path>                              Load secret auth token from a file
  -T <token>                             Set secret auth token on command line

Commands:
  help                                   Show this help
  version                                Print version
  service                                Start as service
  status                                 Show node status and configuration
  join <network> [fingerprint]           Join a virtual network
  leave <network>                        Leave a virtual network
  networks                               List VL2 virtual networks
  network <network> [command] [option] - Network management commands
    show                                 Show network details (default)
    set [option] [value]               - Get or set network options
      manageips <boolean>                Is IP management allowed?
      manageroutes <boolean>             Is route management allowed?
      globalips <boolean>                Allow assignment of global IPs?
      globalroutes <boolean>             Can global IP space routes be set?
      defaultroute <boolean>             Can default route be overridden?
  peers                                  List VL1 peers
  peer <address> [command] [option]    - Peer management commands
    show                                 Show peer details (default)
    try <endpoint> [...]                 Try peer at explicit endpoint
    locator <locator>                    Explicitly update peer locator
  roots                                  List root peers
  root [command]                       - Root management commands
    add <identity> [endpoint]            Designate a peer as a root
    remove <address>                     Un-designate a peer as a root
    subscribe <url>                      Subscribe to a root set
  set [option] [value]                 - Get or set a core config option
    port <port>                          Primary P2P port
    secondaryport <port/0>               Secondary P2P port (0 to disable)
    blacklist cidr <IP/bits> <boolean>   Toggle physical path blacklisting
    blacklist if <prefix> <boolean>      Toggle interface prefix blacklisting
    portmap <boolean>                    Toggle use of uPnP or NAT-PMP
  controller <command> [option]        - Local controller management commands
    networks                             List networks run by local controller
    new                                  Create a new network
    set <network> [setting] [value]      Show or modify network settings
    members <network>                    List members of a network
    member <network> [setting] [value]   Show or modify member level settings
    auth <address>                       Authorize a peer
    deauth <address>                     Deauthorize a peer
  identity <command> [args]            - Identity management commands
    new [c25519|p384]                    Create identity (default: c25519)
    getpublic <identity>                 Extract only public part of identity
    fingerprint <identity>               Get an identity's fingerprint
    validate <identity>                  Locally validate an identity
    sign <identity> <file>               Sign a file with an identity's key
    verify <identity> <file> <sig>       Verify a signature

The 'service' command does not exit until the service receives a signal.

An <address> may be specified as a 10-digit short ZeroTier address, a
fingerprint containing both an address and a SHA384 hash, or an identity.
The latter two options are equivalent in terms of specificity and may be
used if stronger security guarantees are desired than those provided by
the basic ZeroTier addressing system. Fields of type <identity> must be
full identities and may be specified either verbatim or as a path to a file.

An <endpoint> is a place where a peer may be reached. Currently these are
just 'IP/port' format addresses but other types may be added in the future.
`,zerotier.CoreVersionMajor, zerotier.CoreVersionMinor, zerotier.CoreVersionRevision)
}

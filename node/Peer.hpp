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

#ifndef ZT_PEER_HPP
#define ZT_PEER_HPP

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Path.hpp"
#include "Address.hpp"
#include "Utils.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "SharedPtr.hpp"
#include "Mutex.hpp"
#include "Endpoint.hpp"
#include "Locator.hpp"
#include "Protocol.hpp"
#include "AES.hpp"
#include "SymmetricKey.hpp"
#include "Containers.hpp"

// version, identity, locator, bootstrap, version info, length of any additional fields
#define ZT_PEER_MARSHAL_SIZE_MAX (1 + ZT_SYMMETRICKEY_MARSHAL_SIZE_MAX + ZT_IDENTITY_MARSHAL_SIZE_MAX + ZT_LOCATOR_MARSHAL_SIZE_MAX + 1 + (ZT_MAX_PEER_NETWORK_PATHS * ZT_ENDPOINT_MARSHAL_SIZE_MAX) + (2*4) + 2)

namespace ZeroTier {

class Topology;

/**
 * Peer on P2P Network (virtual layer 1)
 */
class Peer
{
	friend class SharedPtr<Peer>;
	friend class Topology;

public:
	/**
	 * Create an uninitialized peer
	 *
	 * The peer will need to be initialized with init() or unmarshal() before
	 * it can be used.
	 *
	 * @param renv Runtime environment
	 */
	explicit Peer(const RuntimeEnvironment *renv);

	~Peer();

	/**
	 * Initialize peer with an identity
	 *
	 * @param peerIdentity The peer's identity
	 * @return True if initialization was succcesful
	 */
	bool init(const Identity &peerIdentity);

	/**
	 * @return This peer's ZT address (short for identity().address())
	 */
	ZT_INLINE Address address() const noexcept { return m_id.address(); }

	/**
	 * @return This peer's identity
	 */
	ZT_INLINE const Identity &identity() const noexcept { return m_id; }

	/**
	 * @return Copy of current locator
	 */
	ZT_INLINE Locator locator() const noexcept
	{
		RWMutex::RLock l(m_lock);
		return m_locator;
	}

	/**
	 * Set this peer's probe token
	 *
	 * This doesn't update the mapping in Topology. The caller must do
	 * this, which is the HELLO handler in VL1.
	 *
	 * @param t New probe token
	 * @return Old probe token
	 */
	ZT_INLINE uint32_t setProbeToken(const uint32_t t) const noexcept
	{
		RWMutex::Lock l(m_lock);
		const uint32_t pt = m_probe;
		m_probe = t;
		return pt;
	}

	/**
	 * @return This peer's probe token or 0 if unknown
	 */
	ZT_INLINE uint32_t probeToken() const noexcept
	{
		RWMutex::RLock l(m_lock);
		return m_probe;
	}

	/**
	 * Log receipt of an authenticated packet
	 *
	 * This is called by the decode pipe when a packet is proven to be authentic
	 * and appears to be valid.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param path Path over which packet was received
	 * @param hops ZeroTier (not IP) hops
	 * @param packetId Packet ID
	 * @param verb Packet verb
	 * @param inReVerb In-reply verb for OK or ERROR verbs
	 */
	void received(
		void *tPtr,
		const SharedPtr<Path> &path,
		unsigned int hops,
		uint64_t packetId,
		unsigned int payloadLength,
		Protocol::Verb verb,
		Protocol::Verb inReVerb);

	/**
	 * Log sent data
	 *
	 * @param now Current time
	 * @param bytes Number of bytes written
	 */
	ZT_INLINE void sent(const int64_t now,const unsigned int bytes) noexcept
	{
		m_lastSend = now;
		m_outMeter.log(now, bytes);
	}

	/**
	 * Called when traffic destined for a different peer is sent to this one
	 *
	 * @param now Current time
	 * @param bytes Number of bytes relayed
	 */
	ZT_INLINE void relayed(const int64_t now,const unsigned int bytes) noexcept
	{
		m_relayedMeter.log(now, bytes);
	}

	/**
	 * Get the current best direct path or NULL if none
	 *
	 * @return Current best path or NULL if there is no direct path
	 */
	ZT_INLINE SharedPtr<Path> path(const int64_t now) noexcept
	{
		if ((now - m_lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
			RWMutex::Lock l(m_lock);
			m_prioritizePaths(now);
			if (m_alivePathCount > 0)
				return m_paths[0];
		} else {
			RWMutex::RLock l(m_lock);
			if (m_alivePathCount > 0)
				return m_paths[0];
		}
		return SharedPtr<Path>();
	}

	/**
	 * Send data to this peer over a specific path only
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param data Data to send
	 * @param len Length in bytes
	 * @param via Path over which to send data (may or may not be an already-learned path for this peer)
	 */
	void send(void *tPtr,int64_t now,const void *data,unsigned int len,const SharedPtr<Path> &via) noexcept;

	/**
	 * Send data to this peer over the best available path
	 *
	 * If there is a working direct path it will be used. Otherwise the data will be
	 * sent via a root server.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param data Data to send
	 * @param len Length in bytes
	 */
	void send(void *tPtr,int64_t now,const void *data,unsigned int len) noexcept;

	/**
	 * Send a HELLO to this peer at a specified physical address.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local source socket
	 * @param atAddress Destination address
	 * @param now Current time
	 * @return Number of bytes sent
	 */
	unsigned int hello(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Send a NOP message to e.g. probe a new link
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local source socket
	 * @param atAddress Destination address
	 * @param now Current time
	 * @return Number of bytes sent
	 */
	unsigned int probe(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Ping this peer if needed and/or perform other periodic tasks.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param isRoot True if this peer is a root
	 */
	void pulse(void *tPtr,int64_t now,bool isRoot);

	/**
	 * Add a potential candidate direct path to the P2P "try" queue.
	 *
	 * @param now Current time
	 * @param ep Endpoint to attempt to contact
	 * @param bfg1024 Use BFG1024 brute force symmetric NAT busting algorithm if applicable
	 */
	void tryDirectPath(int64_t now,const Endpoint &ep,bool breakSymmetricBFG1024);

	/**
	 * Reset paths within a given IP scope and address family
	 *
	 * Resetting a path involves sending an ECHO to it and then deactivating
	 * it until or unless it responds. This is done when we detect a change
	 * to our external IP or another system change that might invalidate
	 * many or all current paths.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param scope IP scope
	 * @param inetAddressFamily Family e.g. AF_INET
	 * @param now Current time
	 */
	void resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now);

	/**
	 * @return All currently memorized bootstrap endpoints
	 */
	ZT_INLINE FCV<Endpoint,ZT_MAX_PEER_NETWORK_PATHS> bootstrap() const noexcept
	{
		RWMutex::RLock l(m_lock);
		FCV<Endpoint,ZT_MAX_PEER_NETWORK_PATHS> r;
		for(SortedMap<Endpoint::Type,Endpoint>::const_iterator i(m_bootstrap.begin());i != m_bootstrap.end();++i) // NOLINT(hicpp-use-auto,modernize-use-auto,modernize-loop-convert)
			r.push_back(i->second);
		return r;
	}

	/**
	 * Set bootstrap endpoint
	 *
	 * @param ep Bootstrap endpoint
	 */
	ZT_INLINE void setBootstrap(const Endpoint &ep) noexcept
	{
		RWMutex::Lock l(m_lock);
		m_bootstrap[ep.type()] = ep;
	}

	/**
	 * @return Time of last receive of anything, whether direct or relayed
	 */
	ZT_INLINE int64_t lastReceive() const noexcept { return m_lastReceive; }

	/**
	 * @return Average latency of all direct paths or -1 if no direct paths or unknown
	 */
	ZT_INLINE int latency() const noexcept
	{
		int ltot = 0;
		int lcnt = 0;
		RWMutex::RLock l(m_lock);
		for(unsigned int i=0;i < m_alivePathCount;++i) {
			int lat = m_paths[i]->latency();
			if (lat > 0) {
				ltot += lat;
				++lcnt;
			}
		}
		return (ltot > 0) ? (lcnt / ltot) : -1;
	}

	/**
	 * @return Preferred cipher suite for normal encrypted P2P communication
	 */
	ZT_INLINE uint8_t cipher() const noexcept
	{
		return ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012;
	}

	/**
	 * Set the currently known remote version of this peer's client
	 *
	 * @param vproto Protocol version
	 * @param vmaj Major version
	 * @param vmin Minor version
	 * @param vrev Revision
	 */
	ZT_INLINE void setRemoteVersion(unsigned int vproto,unsigned int vmaj,unsigned int vmin,unsigned int vrev) noexcept
	{
		m_vProto = (uint16_t)vproto;
		m_vMajor = (uint16_t)vmaj;
		m_vMinor = (uint16_t)vmin;
		m_vRevision = (uint16_t)vrev;
	}

	ZT_INLINE unsigned int remoteVersionProtocol() const noexcept { return m_vProto; }
	ZT_INLINE unsigned int remoteVersionMajor() const noexcept { return m_vMajor; }
	ZT_INLINE unsigned int remoteVersionMinor() const noexcept { return m_vMinor; }
	ZT_INLINE unsigned int remoteVersionRevision() const noexcept { return m_vRevision; }
	ZT_INLINE bool remoteVersionKnown() const noexcept { return ((m_vMajor > 0) || (m_vMinor > 0) || (m_vRevision > 0)); }

	/**
	 * @return True if there is at least one alive direct path
	 */
	bool directlyConnected(int64_t now);

	/**
	 * Get all paths
	 *
	 * @param paths Vector of paths with the first path being the current preferred path
	 */
	void getAllPaths(std::vector< SharedPtr<Path> > &paths);

	/**
	 * Save the latest version of this peer to the data store
	 */
	void save(void *tPtr) const;

	// NOTE: peer marshal/unmarshal only saves/restores the identity, locator, most
	// recent bootstrap address, and version information.
	static constexpr int marshalSizeMax() noexcept { return ZT_PEER_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const noexcept;
	int unmarshal(const uint8_t *restrict data,int len) noexcept;

	/**
	 * Rate limit gate for inbound WHOIS requests
	 */
	ZT_INLINE bool rateGateInboundWhoisRequest(const int64_t now) noexcept
	{
		if ((now - m_lastWhoisRequestReceived) >= ZT_PEER_WHOIS_RATE_LIMIT) {
			m_lastWhoisRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit gate for inbound ECHO requests
	 */
	ZT_INLINE bool rateGateEchoRequest(const int64_t now) noexcept
	{
		if ((now - m_lastEchoRequestReceived) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			m_lastEchoRequestReceived = now;
			return true;
		}
		return false;
	}

private:
	void m_prioritizePaths(int64_t now);

	const RuntimeEnvironment *RR;

	// Read/write mutex for non-atomic non-const fields.
	RWMutex m_lock;

	// The permanent identity key resulting from agreement between our identity and this peer's identity.
	SymmetricKey< AES,0,0 > m_identityKey;

	// Most recently successful (for decrypt) ephemeral key and one previous key.
	SymmetricKey< AES,ZT_SYMMETRIC_KEY_TTL,ZT_SYMMETRIC_KEY_TTL_MESSAGES > m_ephemeralKeys[2];

	Identity m_id;
	Locator m_locator;

	// the last time something was sent or received from this peer (direct or indirect).
	std::atomic<int64_t> m_lastReceive;
	std::atomic<int64_t> m_lastSend;

	// The last time we sent a full HELLO to this peer.
	int64_t m_lastSentHello; // only checked while locked

	// The last time a WHOIS request was received from this peer (anti-DOS / anti-flood).
	std::atomic<int64_t> m_lastWhoisRequestReceived;

	// The last time an ECHO request was received from this peer (anti-DOS / anti-flood).
	std::atomic<int64_t> m_lastEchoRequestReceived;

	// The last time we sorted paths in order of preference. (This happens pretty often.)
	std::atomic<int64_t> m_lastPrioritizedPaths;

	// Meters measuring actual bandwidth in, out, and relayed via this peer (mostly if this is a root).
	Meter<> m_inMeter;
	Meter<> m_outMeter;
	Meter<> m_relayedMeter;

	// Direct paths sorted in descending order of preference.
	SharedPtr<Path> m_paths[ZT_MAX_PEER_NETWORK_PATHS];

	// For SharedPtr<>
	std::atomic<int> __refCount;

	// Number of paths current alive (number of non-NULL entries in _paths).
	unsigned int m_alivePathCount;

	// Remembered addresses by endpoint type (std::map is smaller for only a few keys).
	SortedMap<Endpoint::Type,Endpoint> m_bootstrap;

	// Addresses recieved via PUSH_DIRECT_PATHS etc. that we are scheduled to try.
	struct p_TryQueueItem
	{
		ZT_INLINE p_TryQueueItem() : target(), ts(0), breakSymmetricBFG1024(false) {}
		ZT_INLINE p_TryQueueItem(const int64_t now, const Endpoint &t, const bool bfg) : target(t), ts(now), breakSymmetricBFG1024(bfg) {}
		Endpoint target;
		int64_t ts;
		bool breakSymmetricBFG1024;
	};
	List<p_TryQueueItem> m_tryQueue;
	List<p_TryQueueItem>::iterator m_tryQueuePtr; // loops over _tryQueue like a circular buffer

	// 32-bit probe token or 0 if unknown.
	uint32_t m_probe;

	uint16_t m_vProto;
	uint16_t m_vMajor;
	uint16_t m_vMinor;
	uint16_t m_vRevision;
};

} // namespace ZeroTier

#endif

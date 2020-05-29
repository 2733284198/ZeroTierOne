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

#ifndef ZT_TOPOLOGY_HPP
#define ZT_TOPOLOGY_HPP

#include "Constants.hpp"
#include "Address.hpp"
#include "Identity.hpp"
#include "Peer.hpp"
#include "Path.hpp"
#include "Mutex.hpp"
#include "InetAddress.hpp"
#include "SharedPtr.hpp"
#include "ScopedPtr.hpp"
#include "Fingerprint.hpp"
#include "Containers.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	Topology(const RuntimeEnvironment *renv,void *tPtr);

	/**
	 * Add peer to database
	 *
	 * This will not replace existing peers. In that case the existing peer
	 * record is returned.
	 *
	 * @param peer Peer to add
	 * @return New or existing peer (should replace 'peer')
	 */
	SharedPtr<Peer> add(void *tPtr,const SharedPtr<Peer> &peer);

	/**
	 * Get a peer from its address
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param zta ZeroTier address of peer
	 * @param loadFromCached If false do not load from cache if not in memory (default: true)
	 * @return Peer or NULL if not found
	 */
	ZT_INLINE SharedPtr<Peer> peer(void *tPtr,const Address &zta,const bool loadFromCached = true)
	{
		{
			RWMutex::RLock l(m_peers_l);
			const SharedPtr<Peer> *const ap = m_peers.get(zta);
			if (likely(ap != nullptr))
				return *ap;
		}
		{
			SharedPtr<Peer> p;
			if (loadFromCached) {
				m_loadCached(tPtr, zta, p);
				if (p) {
					RWMutex::Lock l(m_peers_l);
					SharedPtr<Peer> &hp = m_peers[zta];
					if (hp)
						return hp;
					hp = p;
				}
			}
			return p;
		}
	}

	/**
	 * Get a Path object for a given local and remote physical address, creating if needed
	 *
	 * @param l Local socket
	 * @param r Remote address
	 * @return Pointer to canonicalized Path object or NULL on error
	 */
	ZT_INLINE SharedPtr<Path> path(const int64_t l,const InetAddress &r)
	{
		const uint64_t k = s_getPathKey(l, r);
		{
			RWMutex::RLock lck(m_paths_l);
			SharedPtr<Path> *const p = m_paths.get(k);
			if (likely(p != nullptr))
				return *p;
		}
		{
			SharedPtr<Path> p(new Path(l,r));
			RWMutex::Lock lck(m_paths_l);
			SharedPtr<Path> &p2 = m_paths[k];
			if (p2)
				return p2;
			p2 = p;
			return p;
		}
	}

	/**
	 * @return Current best root server
	 */
	ZT_INLINE SharedPtr<Peer> root() const
	{
		RWMutex::RLock l(m_peers_l);
		if (unlikely(m_rootPeers.empty()))
			return SharedPtr<Peer>();
		return m_rootPeers.front();
	}

	/**
	 * @param id Identity to check
	 * @return True if this identity corresponds to a root
	 */
	ZT_INLINE bool isRoot(const Identity &id) const
	{
		RWMutex::RLock l(m_peers_l);
		return (m_roots.find(id) != m_roots.end());
	}

	/**
	 * Apply a function or function object to all peers
	 *
	 * This locks the peer map during execution, so calls to get() etc. during
	 * eachPeer() will deadlock.
	 *
	 * @param f Function to apply
	 * @tparam F Function or function object type
	 */
	template<typename F>
	ZT_INLINE void eachPeer(F f) const
	{
		RWMutex::RLock l(m_peers_l);
		for(Map< Address,SharedPtr<Peer> >::const_iterator i(m_peers.begin());i != m_peers.end();++i)
			f(i->second);
	}

	/**
	 * Apply a function or function object to all peers
	 *
	 * This locks the peer map during execution, so calls to get() etc. during
	 * eachPeer() will deadlock.
	 *
	 * @param f Function to apply
	 * @tparam F Function or function object type
	 */
	template<typename F>
	ZT_INLINE void eachPeerWithRoot(F f) const
	{
		RWMutex::RLock l(m_peers_l);

		Vector<uintptr_t> rootPeerPtrs;
		rootPeerPtrs.reserve(m_rootPeers.size());
		for(Vector< SharedPtr<Peer> >::const_iterator rp(m_rootPeers.begin());rp != m_rootPeers.end();++rp)
			rootPeerPtrs.push_back((uintptr_t)rp->ptr());
		std::sort(rootPeerPtrs.begin(),rootPeerPtrs.end());

		for(Map< Address,SharedPtr<Peer> >::const_iterator i(m_peers.begin());i != m_peers.end();++i)
			f(i->second,std::binary_search(rootPeerPtrs.begin(),rootPeerPtrs.end(),(uintptr_t)i->second.ptr()));
	}

	/**
	 * @param allPeers vector to fill with all current peers
	 */
	ZT_INLINE void getAllPeers(Vector< SharedPtr<Peer> > &allPeers) const
	{
		RWMutex::RLock l(m_peers_l);
		allPeers.clear();
		allPeers.reserve(m_peers.size());
		for(Map< Address,SharedPtr<Peer> >::const_iterator i(m_peers.begin());i != m_peers.end();++i)
			allPeers.push_back(i->second);
	}

	/**
	 * Add or update a root server and its locator
	 *
	 * This also validates the identity and checks the locator signature,
	 * returning false if either of these is not valid.
	 *
	 * @param tPtr Thread pointer
	 * @param id Root identity
	 * @param loc Root locator
	 * @return True if identity and locator are valid and root was added / updated
	 */
	bool addRoot(void *tPtr,const Identity &id,const SharedPtr<const Locator> &loc);

	/**
	 * Remove a root server's identity from the root server set
	 *
	 * @param tPtr Thread pointer
	 * @param address Root address
	 * @return True if root found and removed, false if not found
	 */
	bool removeRoot(void *tPtr, Address address);

	/**
	 * Sort roots in ascending order of apparent latency
	 *
	 * @param now Current time
	 */
	void rankRoots();

	/**
	 * Do periodic tasks such as database cleanup
	 */
	void doPeriodicTasks(void *tPtr,int64_t now);

	/**
	 * Save all currently known peers to data store
	 */
	void saveAll(void *tPtr);

private:
	void m_loadCached(void *tPtr, const Address &zta, SharedPtr<Peer> &peer);
	void m_writeRootList(void *tPtr);
	void m_updateRootPeers(void *tPtr);

	// This gets an integer key from an InetAddress for looking up paths.
	static ZT_INLINE uint64_t s_getPathKey(const int64_t l,const InetAddress &r) noexcept
	{
		// SECURITY: these will be used as keys in a Map<> which uses its own hasher that
		// mixes in a per-invocation secret to work against hash collision attacks. See the
		// map hasher in Containers.hpp. Otherwise the point here is really really fast
		// path lookup by address. The number of paths is never likely to be high enough
		// for a collision to be something we worry about. That would require a minimum of
		// millions and millions of paths on a single node.
		if (r.family() == AF_INET) {
			return ((uint64_t)(r.as.sa_in.sin_addr.s_addr) << 32U) ^ ((uint64_t)r.as.sa_in.sin_port << 16U) ^ (uint64_t)l;
		} else if (r.family() == AF_INET6) {
			return Utils::loadAsIsEndian<uint64_t>(r.as.sa_in6.sin6_addr.s6_addr) + Utils::loadAsIsEndian<uint64_t>(r.as.sa_in6.sin6_addr.s6_addr + 8) + (uint64_t)r.as.sa_in6.sin6_port + (uint64_t)l;
		} else {
			// This should never really be used but it's here just in case.
			return (uint64_t)Utils::fnv1a32(reinterpret_cast<const void *>(&r),sizeof(InetAddress)) + (uint64_t)l;
		}
	}

	const RuntimeEnvironment *const RR;
	RWMutex m_paths_l; // locks m_paths
	RWMutex m_peers_l; // locks m_peers, m_roots, and m_rootPeers
	Map< uint64_t,SharedPtr<Path> > m_paths;
	Map< Address,SharedPtr<Peer> > m_peers;
	Map< Identity,SharedPtr<const Locator> > m_roots;
	Vector< SharedPtr<Peer> > m_rootPeers;
};

} // namespace ZeroTier

#endif

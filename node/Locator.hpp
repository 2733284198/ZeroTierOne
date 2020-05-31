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

#ifndef ZT_LOCATOR_HPP
#define ZT_LOCATOR_HPP

#include "Constants.hpp"
#include "Endpoint.hpp"
#include "Identity.hpp"
#include "TriviallyCopyable.hpp"
#include "SharedPtr.hpp"
#include "FCV.hpp"
#include "Containers.hpp"

#define ZT_LOCATOR_MAX_ENDPOINTS 8
#define ZT_LOCATOR_MARSHAL_SIZE_MAX (8 + ZT_FINGERPRINT_MARSHAL_SIZE + 2 + (ZT_LOCATOR_MAX_ENDPOINTS * ZT_ENDPOINT_MARSHAL_SIZE_MAX) + 2 + 2 + ZT_SIGNATURE_BUFFER_SIZE)
#define ZT_LOCATOR_STRING_SIZE_MAX 4096

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 *
 * A locator contains long-lived endpoints for a node such as IP/port pairs,
 * URLs, or other nodes, and is signed by the node it describes.
 */
class Locator
{
	friend class SharedPtr<Locator>;
	friend class SharedPtr<const Locator>;

public:
	ZT_INLINE Locator() noexcept :
		m_ts(0)
	{}

	explicit Locator(const char *const str) noexcept;

	ZT_INLINE Locator(const Locator &loc) noexcept :
		m_ts(loc.m_ts),
		m_signer(loc.m_signer),
		m_endpoints(loc.m_endpoints),
		m_signature(loc.m_signature),
		__refCount(0)
	{}

	/**
	 * @return Timestamp (a.k.a. revision number) set by Location signer
	 */
	ZT_INLINE int64_t timestamp() const noexcept
	{ return m_ts; }

	/**
	 * @return Fingerprint of identity that signed this locator
	 */
	ZT_INLINE const Fingerprint &signer() const noexcept
	{ return m_signer; }

	/**
	 * @return Endpoints specified in locator
	 */
	ZT_INLINE const Vector<Endpoint> &endpoints() const noexcept
	{ return m_endpoints; }

	/**
	 * @return Signature data
	 */
	ZT_INLINE const FCV<uint8_t, ZT_SIGNATURE_BUFFER_SIZE> &signature() const noexcept
	{ return m_signature; }

	/**
	 * Add an endpoint to this locator
	 *
	 * This doesn't check for the presence of the endpoint, so take
	 * care not to add duplicates.
	 *
	 * @param ep Endpoint to add
	 * @return True if endpoint was added (or already present), false if locator is full
	 */
	bool add(const Endpoint &ep);

	/**
	 * Sign this locator
	 *
	 * This sets timestamp, sorts endpoints so that the same set of endpoints
	 * will always produce the same locator, and signs.
	 *
	 * @param id Identity that includes private key
	 * @return True if signature successful
	 */
	bool sign(int64_t ts, const Identity &id) noexcept;

	/**
	 * Verify this Locator's validity and signature
	 *
	 * @param id Identity corresponding to hash
	 * @return True if valid and signature checks out
	 */
	bool verify(const Identity &id) const noexcept;

	/**
	 * Convert this locator to a string
	 *
	 * @param s String buffer
	 * @return Pointer to buffer
	 */
	char *toString(char s[ZT_LOCATOR_STRING_SIZE_MAX]) const noexcept;
	ZT_INLINE String toString() const { char tmp[ZT_LOCATOR_STRING_SIZE_MAX]; return String(toString(tmp)); }

	/**
	 * Decode a string format locator
	 *
	 * @param s Locator from toString()
	 * @return True if format was valid
	 */
	bool fromString(const char *s) noexcept;

	explicit ZT_INLINE operator bool() const noexcept
	{ return m_ts > 0; }

	static constexpr int marshalSizeMax() noexcept { return ZT_LOCATOR_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX], bool excludeSignature = false) const noexcept;
	int unmarshal(const uint8_t *data, int len) noexcept;

private:
	int64_t m_ts;
	Fingerprint m_signer;
	Vector<Endpoint> m_endpoints;
	FCV<uint8_t, ZT_SIGNATURE_BUFFER_SIZE> m_signature;
	std::atomic<int> __refCount;
};

} // namespace ZeroTier

#endif

#include "DepthGen_ModelIntegrity.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace depthgen {
namespace {

constexpr std::array<uint32_t, 64> kRoundConstants = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

constexpr uint32_t RotateRight(uint32_t value, uint32_t count) noexcept {
	return (value >> count) | (value << (32U - count));
}

class Sha256 final {
public:
	void Update(const uint8_t* data, size_t count) {
		for (size_t index = 0; index < count; ++index) {
			block_[block_length_++] = data[index];
			if (block_length_ == block_.size()) {
				Transform();
				bit_length_ += 512U;
				block_length_ = 0;
			}
		}
	}

	std::array<uint8_t, 32> Finalise() {
		const uint64_t total_bits = bit_length_ + static_cast<uint64_t>(block_length_) * 8U;
		block_[block_length_++] = 0x80U;
		if (block_length_ > 56U) {
			while (block_length_ < block_.size()) block_[block_length_++] = 0U;
			Transform();
			block_length_ = 0;
		}
		while (block_length_ < 56U) block_[block_length_++] = 0U;
		for (int index = 7; index >= 0; --index) {
			block_[block_length_++] = static_cast<uint8_t>(total_bits >> (index * 8));
		}
		Transform();

		std::array<uint8_t, 32> digest{};
		for (size_t index = 0; index < state_.size(); ++index) {
			digest[index * 4U] = static_cast<uint8_t>(state_[index] >> 24U);
			digest[index * 4U + 1U] = static_cast<uint8_t>(state_[index] >> 16U);
			digest[index * 4U + 2U] = static_cast<uint8_t>(state_[index] >> 8U);
			digest[index * 4U + 3U] = static_cast<uint8_t>(state_[index]);
		}
		return digest;
	}

private:
	void Transform() {
		std::array<uint32_t, 64> words{};
		for (size_t index = 0; index < 16; ++index) {
			const size_t offset = index * 4U;
			words[index] = (static_cast<uint32_t>(block_[offset]) << 24U) |
				(static_cast<uint32_t>(block_[offset + 1U]) << 16U) |
				(static_cast<uint32_t>(block_[offset + 2U]) << 8U) |
				static_cast<uint32_t>(block_[offset + 3U]);
		}
		for (size_t index = 16; index < words.size(); ++index) {
			const uint32_t s0 = RotateRight(words[index - 15U], 7U) ^ RotateRight(words[index - 15U], 18U) ^
				(words[index - 15U] >> 3U);
			const uint32_t s1 = RotateRight(words[index - 2U], 17U) ^ RotateRight(words[index - 2U], 19U) ^
				(words[index - 2U] >> 10U);
			words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
		}

		uint32_t a = state_[0];
		uint32_t b = state_[1];
		uint32_t c = state_[2];
		uint32_t d = state_[3];
		uint32_t e = state_[4];
		uint32_t f = state_[5];
		uint32_t g = state_[6];
		uint32_t h = state_[7];
		for (size_t index = 0; index < words.size(); ++index) {
			const uint32_t s1 = RotateRight(e, 6U) ^ RotateRight(e, 11U) ^ RotateRight(e, 25U);
			const uint32_t choose = (e & f) ^ ((~e) & g);
			const uint32_t temporary1 = h + s1 + choose + kRoundConstants[index] + words[index];
			const uint32_t s0 = RotateRight(a, 2U) ^ RotateRight(a, 13U) ^ RotateRight(a, 22U);
			const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
			const uint32_t temporary2 = s0 + majority;
			h = g;
			g = f;
			f = e;
			e = d + temporary1;
			d = c;
			c = b;
			b = a;
			a = temporary1 + temporary2;
		}
		state_[0] += a;
		state_[1] += b;
		state_[2] += c;
		state_[3] += d;
		state_[4] += e;
		state_[5] += f;
		state_[6] += g;
		state_[7] += h;
	}

	std::array<uint32_t, 8> state_ = {
		0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
		0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
	std::array<uint8_t, 64> block_{};
	size_t block_length_ = 0;
	uint64_t bit_length_ = 0;
};

std::string DigestText(const std::array<uint8_t, 32>& bytes) {
	std::ostringstream text;
	text << std::hex << std::setfill('0');
	for (const uint8_t byte : bytes) text << std::setw(2) << static_cast<unsigned int>(byte);
	return text.str();
}

bool VerifyDigest(const std::string& actual, const char* expected, std::string* error) {
	if (!expected || actual != expected) {
		if (error) {
			*error = "DepthGen model SHA-256 mismatch. Expected " +
				std::string(expected ? expected : "(none)") + ", got " + actual + ".";
		}
		return false;
	}
	return true;
}

} // namespace

bool ComputeSha256File(const std::filesystem::path& path, std::string* digest, std::string* error) {
	if (!digest) {
		if (error) *error = "DepthGen SHA-256 output was not supplied.";
		return false;
	}
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		if (error) *error = "DepthGen could not read the model for SHA-256 verification.";
		return false;
	}
	Sha256 hasher;
	std::array<char, 65536> buffer{};
	while (stream) {
		stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		const std::streamsize count = stream.gcount();
		if (count > 0) hasher.Update(reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<size_t>(count));
	}
	if (!stream.eof()) {
		if (error) *error = "DepthGen could not finish reading the model for SHA-256 verification.";
		return false;
	}
	*digest = DigestText(hasher.Finalise());
	return true;
}

bool ComputeSha256Bytes(const void* data, size_t size, std::string* digest, std::string* error) {
	if (!digest) {
		if (error) *error = "DepthGen SHA-256 output was not supplied.";
		return false;
	}
	if (!data || size == 0) {
		if (error) *error = "DepthGen embedded model is empty.";
		return false;
	}
	Sha256 hasher;
	hasher.Update(static_cast<const uint8_t*>(data), size);
	*digest = DigestText(hasher.Finalise());
	return true;
}

bool VerifyModelSha256(const std::filesystem::path& path, const char* expected, std::string* error) {
	std::string actual;
	if (!ComputeSha256File(path, &actual, error)) return false;
	return VerifyDigest(actual, expected, error);
}

bool VerifyModelSha256(const void* data, size_t size, const char* expected, std::string* error) {
	std::string actual;
	if (!ComputeSha256Bytes(data, size, &actual, error)) return false;
	return VerifyDigest(actual, expected, error);
}

} // namespace depthgen

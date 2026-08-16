#include "DepthGen_Temporal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace depthgen {
namespace {

float Clamp01(float value) noexcept {
	return std::max(0.0f, std::min(1.0f, value));
}

float Mix(float current, float previous, float weight) noexcept {
	return current * (1.0f - weight) + previous * weight;
}

float PercentileFromSorted(const std::vector<float>& sorted, float percentile) {
	const size_t count = sorted.size();
	if (count == 0) {
		return 0.0f;
	}
	// Position in double for the same reason as PercentileFromUnsorted: past
	// 2^24 elements, float cannot represent (count - 1) exactly, and `lower`
	// is clamped in case the cast rounds up to count.
	const double position = static_cast<double>(Clamp01(percentile / 100.0f)) *
		static_cast<double>(count - 1);
	size_t lower = static_cast<size_t>(std::floor(position));
	if (lower >= count) {
		lower = count - 1;
	}
	const size_t upper = lower + 1 < count ? lower + 1 : count - 1;
	const float fraction = static_cast<float>(position - static_cast<double>(lower));
	return sorted[lower] + (sorted[upper] - sorted[lower]) * fraction;
}

void EnforceIncreasing(float* quantiles, int count) noexcept {
	if (!quantiles || count < 2) {
		return;
	}
	for (int i = 1; i < count; ++i) {
		quantiles[i] = std::max(quantiles[i], quantiles[i - 1] + 1.0e-4f);
	}
}

float MapPiecewise(float value, const float* source, const float* destination, int count) noexcept {
	if (!source || !destination || count < 2) {
		return Clamp01(value);
	}
	if (value <= source[0]) {
		const float span = source[1] - source[0];
		if (span <= 1.0e-6f) {
			return Clamp01(destination[0]);
		}
		const float t = (value - source[0]) / span;
		return Clamp01(destination[0] + (destination[1] - destination[0]) * t);
	}
	if (value >= source[count - 1]) {
		const float span = source[count - 1] - source[count - 2];
		if (span <= 1.0e-6f) {
			return Clamp01(destination[count - 1]);
		}
		const float t = (value - source[count - 2]) / span;
		return Clamp01(destination[count - 2] + (destination[count - 1] - destination[count - 2]) * t);
	}
	for (int i = 0; i < count - 1; ++i) {
		if (value <= source[i + 1]) {
			const float span = source[i + 1] - source[i];
			if (span <= 1.0e-6f) {
				return Clamp01(destination[i]);
			}
			const float t = (value - source[i]) / span;
			return Clamp01(destination[i] + (destination[i + 1] - destination[i]) * t);
		}
	}
	return Clamp01(destination[count - 1]);
}

bool CollectOpaqueSamples(const std::vector<float>& depth, const std::vector<float>& alpha,
	float alpha_threshold, std::vector<float>* samples) {
	if (!samples || depth.size() < 16) {
		return false;
	}
	samples->clear();
	samples->reserve(depth.size());
	const bool use_alpha = alpha.size() == depth.size();
	for (size_t i = 0; i < depth.size(); ++i) {
		if (use_alpha && alpha[i] <= alpha_threshold) {
			continue;
		}
		if (!std::isfinite(depth[i])) {
			continue;
		}
		samples->push_back(depth[i]);
	}
	return samples->size() >= 16;
}

bool QuantileSceneCut(const float* current, const float* previous) noexcept {
	if (!current || !previous) {
		return true;
	}
	const float current_span = current[kTemporalQuantileCount - 1] - current[0];
	const float previous_span = previous[kTemporalQuantileCount - 1] - previous[0];
	if (current_span <= 1.0e-6f || previous_span <= 1.0e-6f) {
		return true;
	}
	const float scale = previous_span / current_span;
	if (scale < 0.4f || scale > 2.5f) {
		return true;
	}
	constexpr int kMedian = 3;
	const float median_jump = std::abs(current[kMedian] - previous[kMedian]);
	return median_jump > 0.35f * std::max(current_span, previous_span);
}

bool MappingSceneCut(const DepthLevels& current, const DepthLevels& previous) {
	if (!current.valid || !previous.valid) {
		return true;
	}
	const float current_span = current.high - current.low;
	const float previous_span = previous.high - previous.low;
	if (current_span <= 1.0e-6f || previous_span <= 1.0e-6f) {
		return true;
	}
	const float scale = previous_span / current_span;
	return scale < 0.4f || scale > 2.5f;
}

struct CacheTable {
	std::mutex mutex;
	std::uint64_t next_id = 1;
	std::unordered_map<std::uint64_t, std::shared_ptr<TemporalHistory>> histories;
};

CacheTable& Caches() {
	static CacheTable table;
	return table;
}

} // namespace

DepthLevels SmoothMappingRange(const DepthLevels& current, const DepthLevels& previous,
	float stability01) {
	const float amount = Clamp01(stability01);
	if (amount <= 1.0e-6f || MappingSceneCut(current, previous)) {
		return current;
	}
	DepthLevels mixed;
	mixed.low = Mix(current.low, previous.low, amount);
	mixed.high = Mix(current.high, previous.high, amount);
	mixed.valid = mixed.high > mixed.low;
	return mixed.valid ? mixed : current;
}

bool MeasureUnitQuantiles(
	const std::vector<float>& depth,
	const std::vector<float>& alpha,
	float alpha_threshold,
	float* quantiles) {
	if (!quantiles) {
		return false;
	}
	std::vector<float> samples;
	if (!CollectOpaqueSamples(depth, alpha, alpha_threshold, &samples)) {
		return false;
	}
	std::sort(samples.begin(), samples.end());
	for (int i = 0; i < kTemporalQuantileCount; ++i) {
		quantiles[i] = PercentileFromSorted(samples, kTemporalPercentiles[i]);
	}
	EnforceIncreasing(quantiles, kTemporalQuantileCount);
	return quantiles[kTemporalQuantileCount - 1] > quantiles[0] + 1.0e-6f;
}

void AlignUnitQuantiles(
	std::vector<float>* current,
	const std::vector<float>& alpha,
	float alpha_threshold,
	const float* previous_quantiles,
	float stability01) {
	const float amount = Clamp01(stability01);
	if (!current || !previous_quantiles || amount <= 1.0e-6f) {
		return;
	}
	float current_quantiles[kTemporalQuantileCount] = {};
	if (!MeasureUnitQuantiles(*current, alpha, alpha_threshold, current_quantiles)) {
		return;
	}
	if (QuantileSceneCut(current_quantiles, previous_quantiles)) {
		return;
	}
	float target[kTemporalQuantileCount] = {};
	for (int i = 0; i < kTemporalQuantileCount; ++i) {
		target[i] = Mix(current_quantiles[i], previous_quantiles[i], amount);
	}
	EnforceIncreasing(target, kTemporalQuantileCount);
	for (float& value : *current) {
		value = MapPiecewise(value, current_quantiles, target, kTemporalQuantileCount);
	}
}

TemporalRange MeasureUnitRange(
	const std::vector<float>& depth,
	const std::vector<float>& alpha,
	float alpha_threshold,
	const DepthLevels& mapping) {
	TemporalRange range;
	range.mapping = mapping;
	range.valid = mapping.valid &&
		MeasureUnitQuantiles(depth, alpha, alpha_threshold, range.quantiles);
	return range;
}

bool TemporalHistory::CopyPrevious(std::int32_t time, std::int32_t time_step, const TemporalLayout& layout,
	TemporalRange* previous) const {
	if (!previous || time_step == 0) {
		return false;
	}
	const std::int32_t step = time_step < 0 ? -time_step : time_step;
	const std::int32_t previous_time = time - time_step;
	const std::int32_t max_delta = step * 2;
	std::lock_guard<std::mutex> lock(mutex_);
	const Entry* exact = nullptr;
	const Entry* nearest = nullptr;
	std::int32_t nearest_delta = max_delta + 1;
	for (const Entry& entry : entries_) {
		if (entry.layout != layout || !entry.range.valid) {
			continue;
		}
		if (entry.time == previous_time) {
			exact = &entry;
			break;
		}
		if (entry.time >= time) {
			continue;
		}
		const std::int32_t delta = time - entry.time;
		if (delta > 0 && delta <= max_delta && delta < nearest_delta) {
			nearest = &entry;
			nearest_delta = delta;
		}
	}
	const Entry* chosen = exact ? exact : nearest;
	if (!chosen) {
		return false;
	}
	*previous = chosen->range;
	return true;
}

void TemporalHistory::Store(std::int32_t time, const TemporalLayout& layout, TemporalRange range) {
	if (!range.valid) {
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto it = entries_.begin(); it != entries_.end(); ++it) {
		if (it->time == time) {
			it->layout = layout;
			it->range = range;
			return;
		}
	}
	if (entries_.size() >= kMaxFrames) {
		auto furthest = entries_.begin();
		std::int32_t furthest_delta = std::abs(furthest->time - time);
		for (auto it = entries_.begin(); it != entries_.end(); ++it) {
			const std::int32_t delta = std::abs(it->time - time);
			if (delta > furthest_delta) {
				furthest = it;
				furthest_delta = delta;
			}
		}
		entries_.erase(furthest);
	}
	entries_.push_back(Entry{time, layout, range});
}

void TemporalHistory::Clear() {
	std::lock_guard<std::mutex> lock(mutex_);
	entries_.clear();
}

std::uint64_t TemporalCacheCreate() {
	CacheTable& table = Caches();
	std::lock_guard<std::mutex> lock(table.mutex);
	const std::uint64_t id = table.next_id++;
	table.histories[id] = std::make_shared<TemporalHistory>();
	return id;
}

std::shared_ptr<TemporalHistory> TemporalCacheGet(std::uint64_t id) {
	if (id == 0) {
		return nullptr;
	}
	CacheTable& table = Caches();
	std::lock_guard<std::mutex> lock(table.mutex);
	auto found = table.histories.find(id);
	if (found == table.histories.end()) {
		auto history = std::make_shared<TemporalHistory>();
		table.histories.emplace(id, history);
		return history;
	}
	return found->second;
}

void TemporalCacheRelease(std::uint64_t id) {
	if (id == 0) {
		return;
	}
	CacheTable& table = Caches();
	std::lock_guard<std::mutex> lock(table.mutex);
	table.histories.erase(id);
}

} // namespace depthgen

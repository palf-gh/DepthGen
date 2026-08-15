#include "DepthGen_Image.h"
#include "DepthGen_Inference.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct BenchmarkOptions {
	int source_width = 1920;
	int source_height = 1080;
	int short_edge = 518;
	int warm_up_runs = 3;
	int measured_runs = 31;
	depthgen::InferencePreference preference = depthgen::InferencePreference::Accelerated;
};

int ParsePositiveInteger(const char* value, const char* flag) {
	try {
		const int parsed = std::stoi(value ? value : "");
		if (parsed > 0) return parsed;
	} catch (const std::exception&) {
	}
	throw std::runtime_error(std::string(flag) + " must be a positive integer.");
}

void PrintUsage() {
	std::cout << "Usage: depthgen_benchmark [--quality fast|balanced|high] [--provider accelerated|cpu] "
		"[--source-width N] [--source-height N] [--warm-up N] [--runs N]\n";
}

BenchmarkOptions ParseOptions(int argc, char* argv[]) {
	BenchmarkOptions options;
	for (int index = 1; index < argc; ++index) {
		const std::string argument = argv[index];
		if (argument == "--help") {
			PrintUsage();
			std::exit(0);
		}
		if (index + 1 >= argc) {
			throw std::runtime_error("Missing value for " + argument + ".");
		}
		const char* value = argv[++index];
		if (argument == "--quality") {
			const std::string quality = value;
			if (quality == "fast") options.short_edge = 392;
			else if (quality == "balanced") options.short_edge = 518;
			else if (quality == "high") options.short_edge = 700;
			else throw std::runtime_error("--quality must be fast, balanced, or high.");
		} else if (argument == "--provider") {
			const std::string provider = value;
			if (provider == "accelerated") options.preference = depthgen::InferencePreference::Accelerated;
			else if (provider == "cpu") options.preference = depthgen::InferencePreference::Cpu;
			else throw std::runtime_error("--provider must be accelerated or cpu.");
		} else if (argument == "--source-width") {
			options.source_width = ParsePositiveInteger(value, "--source-width");
		} else if (argument == "--source-height") {
			options.source_height = ParsePositiveInteger(value, "--source-height");
		} else if (argument == "--warm-up") {
			options.warm_up_runs = ParsePositiveInteger(value, "--warm-up");
		} else if (argument == "--runs") {
			options.measured_runs = ParsePositiveInteger(value, "--runs");
		} else {
			throw std::runtime_error("Unknown option " + argument + ".");
		}
	}
	return options;
}

double Percentile(const std::vector<double>& sorted, double percentile) {
	const double position = (static_cast<double>(sorted.size()) - 1.0) * percentile;
	const size_t low = static_cast<size_t>(std::floor(position));
	const size_t high = static_cast<size_t>(std::ceil(position));
	const double blend = position - static_cast<double>(low);
	return sorted[low] + (sorted[high] - sorted[low]) * blend;
}

std::vector<float> MakeInput(int width, int height) {
	std::vector<float> rgb(static_cast<size_t>(width) * static_cast<size_t>(height) * 3U);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const size_t offset = (static_cast<size_t>(y) * width + x) * 3U;
			const float horizontal = static_cast<float>(x) / static_cast<float>(std::max(width - 1, 1));
			const float vertical = static_cast<float>(y) / static_cast<float>(std::max(height - 1, 1));
			rgb[offset] = horizontal;
			rgb[offset + 1U] = vertical;
			rgb[offset + 2U] = 0.5f + 0.5f * std::sin((horizontal + vertical) * 6.28318530718f);
		}
	}
	depthgen::ImageNetNormaliseInterleavedRgb(&rgb);
	return rgb;
}

void RunInference(const std::vector<float>& input, int width, int height,
	depthgen::InferencePreference preference, depthgen::InferenceProvider* provider) {
	depthgen::InferenceResult result;
	std::string error;
	if (!depthgen::InferDepthAnythingSmall(input, width, height, &result, provider, &error, preference)) {
		throw std::runtime_error(error);
	}
	if (result.width <= 0 || result.height <= 0 || result.depth.empty()) {
		throw std::runtime_error("DepthGen inference returned no depth values.");
	}
}

} // namespace

int main(int argc, char* argv[]) {
	try {
		const BenchmarkOptions options = ParseOptions(argc, argv);
		int inference_width = 0;
		int inference_height = 0;
		depthgen::ComputeInferenceSize(options.source_width, options.source_height, options.short_edge,
			&inference_width, &inference_height);
		const std::vector<float> input = MakeInput(inference_width, inference_height);
		depthgen::InferenceProvider provider = depthgen::InferenceProvider::Unavailable;
		for (int run = 0; run < options.warm_up_runs; ++run) {
			RunInference(input, inference_width, inference_height, options.preference, &provider);
		}
		std::vector<double> samples_ms;
		samples_ms.reserve(static_cast<size_t>(options.measured_runs));
		for (int run = 0; run < options.measured_runs; ++run) {
			const auto start = std::chrono::steady_clock::now();
			RunInference(input, inference_width, inference_height, options.preference, &provider);
			const auto stop = std::chrono::steady_clock::now();
			samples_ms.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
		}
		std::sort(samples_ms.begin(), samples_ms.end());
		std::cout << std::fixed << std::setprecision(3);
		std::cout << "provider,source_width,source_height,short_edge,inference_width,inference_height,warm_up_runs,measured_runs,p50_ms,p95_ms\n";
		std::cout << depthgen::InferenceProviderName(provider) << ','
			<< options.source_width << ',' << options.source_height << ',' << options.short_edge << ','
			<< inference_width << ',' << inference_height << ',' << options.warm_up_runs << ','
			<< options.measured_runs << ',' << Percentile(samples_ms, 0.50) << ','
			<< Percentile(samples_ms, 0.95) << '\n';
		return 0;
	} catch (const std::exception& exception) {
		std::cerr << "DepthGen benchmark failed: " << exception.what() << '\n';
		return 1;
	}
}

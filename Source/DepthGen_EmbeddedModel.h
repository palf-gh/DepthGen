#pragma once

#include "DepthGen_Inference.h"

#include <cstddef>
#include <string>

namespace depthgen {

struct EmbeddedModelView {
	const void* data = nullptr;
	size_t size = 0;
};

bool GetEmbeddedModel(DepthModel model, EmbeddedModelView* view, std::string* error);

} // namespace depthgen

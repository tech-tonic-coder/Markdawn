#pragma once

#include <optional>
#include <string>

namespace markdawn::core {

// Bumping this is a breaking wire-format change: an old running instance
// receiving a message from a newer protocol version (or vice versa) should
// reject it rather than guess, so parseOpenFile() enforces an exact match.
inline constexpr int kOpenFileProtocolVersion = 1;

// The single-instance handoff message (§5 Phase 1): a second launch sends
// this to the already-running instance instead of opening its own window.
struct OpenFileMessage {
    int version = kOpenFileProtocolVersion;
    std::string path;
};

// Serializes to a single line of JSON with no embedded newline, so the
// caller can frame it on the wire with a trailing '\n'.
std::string serializeOpenFile(const OpenFileMessage& message);

// Returns std::nullopt if the payload isn't valid JSON, is missing the
// "path" field, or reports a protocol version this build doesn't handle.
std::optional<OpenFileMessage> parseOpenFile(const std::string& payload);

} // namespace markdawn::core

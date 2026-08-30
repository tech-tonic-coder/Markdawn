#include "markdawncore/ipc/ipc_message.h"

#include <nlohmann/json.hpp>

namespace markdawn::core {

std::string serializeOpenFile(const OpenFileMessage& message) {
    const nlohmann::json j{{"version", message.version}, {"path", message.path}};
    return j.dump();
}

std::optional<OpenFileMessage> parseOpenFile(const std::string& payload) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }

    if (!j.is_object() || !j.contains("version") || !j.contains("path")) {
        return std::nullopt;
    }

    OpenFileMessage message;
    try {
        message.version = j.at("version").get<int>();
        message.path = j.at("path").get<std::string>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }

    if (message.version != kOpenFileProtocolVersion) {
        return std::nullopt;
    }
    return message;
}

} // namespace markdawn::core

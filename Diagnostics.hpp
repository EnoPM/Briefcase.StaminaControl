#pragma once
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace suspicion {
inline constexpr const char *schema = R"({"type":"object","properties":{
 "multiplier":{"type":"number","minimum":0.0,"maximum":10.0,"default":1.0,"description":"Suspicion gain multiplier. 0 disables running drain and discrete stamina losses; 1 preserves vanilla. Restart required."},
 "diagnostics":{"type":"boolean","default":true,"description":"Enable bounded configuration and native before/after logs. Gameplay settings remain active when false."},
 "maximumSamples":{"type":"integer","minimum":1,"maximum":1000,"default":120,"description":"Maximum before/after drain samples per server run, plus 16 initialization samples."}
}})";
struct Config {
    double multiplier = 1;
    bool diagnostics = true;
    uint32_t maximum = 120;
    static Config parse(const std::string &text) {
        auto j = nlohmann::json::parse(text);
        if (!j.at("multiplier").is_number() || !j.at("diagnostics").is_boolean() ||
            !j.at("maximumSamples").is_number_integer())
            throw std::runtime_error("Invalid diagnostic settings");
        const auto m = j["multiplier"].get<double>();
        const auto n = j["maximumSamples"].get<int64_t>();
        if (!std::isfinite(m) || m < 0 || m > 10 || n < 1 || n > 1000)
            throw std::runtime_error("Suspicion setting out of range");
        return {m, j["diagnostics"].get<bool>(), uint32_t(n)};
    }
};
inline float effective_multiplier(double original, double factor) {
    if (!std::isfinite(original) || !std::isfinite(factor) || factor < 0 || factor > 10)
        throw std::runtime_error("Invalid stamina multiplier");
    const auto value = factor == 1 ? original : (original > 0 ? original : 1.) * factor;
    if (std::abs(value) > std::numeric_limits<float>::max())
        throw std::runtime_error("Stamina multiplier overflow");
    return static_cast<float>(value);
}
inline float scaled_delta(double delta, double factor) {
    if (!std::isfinite(delta) || !std::isfinite(factor) || factor < 0 || factor > 10)
        throw std::runtime_error("Invalid stamina delta");
    const auto value = delta > 0 ? delta * factor : delta;
    if (std::abs(value) > std::numeric_limits<float>::max())
        throw std::runtime_error("Stamina delta overflow");
    return static_cast<float>(value);
}
class Budget {
    uint32_t used_ = 0, maximum_;

  public:
    explicit Budget(uint32_t maximum) : maximum_(maximum) {}
    bool take() {
        if (used_ >= maximum_)
            return false;
        ++used_;
        return true;
    }
    uint32_t used() const { return used_; }
};
} // namespace suspicion

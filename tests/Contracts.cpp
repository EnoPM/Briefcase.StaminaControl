#include "../Diagnostics.hpp"
#include "ConfigDefaults.hpp"
#include <iostream>
static int count;
void check(bool b) {
    ++count;
    if (!b)
        throw std::runtime_error("Suspicion contract " + std::to_string(count));
}
template <class F> void reject(F f) {
    bool failed = false;
    try {
        f();
    } catch (...) {
        failed = true;
    }
    check(failed);
}
int main() {
    try {
        auto schema = nlohmann::json::parse(suspicion::schema);
        auto defaults = config_defaults(schema);
        auto c = suspicion::Config::parse(defaults.dump());
        check(c.multiplier == 1 && c.diagnostics && c.maximum == 120);
        for (double m : {0., 0.1, 0.5, 1., 10.}) {
            auto j = defaults;
            j["multiplier"] = m;
            check(suspicion::Config::parse(j.dump()).multiplier == m);
        }
        for (auto v :
             {nlohmann::json(-0.1), nlohmann::json(10.1), nlohmann::json(true), nlohmann::json("0.5")}) {
            auto j = defaults;
            j["multiplier"] = v;

            reject([&] { suspicion::Config::parse(j.dump()); });
        }
        for (int max : {1, 120, 1000}) {
            suspicion::Budget b(max);
            for (int i = 0; i < max; ++i)
                check(b.take());
            for (int i = 0; i < 5; ++i)
                check(!b.take());
            check(b.used() == max);
        }
        for (auto v : {nlohmann::json(0), nlohmann::json(1001), nlohmann::json(1.5)}) {
            auto j = defaults;
            j["maximumSamples"] = v;

            reject([&] { suspicion::Config::parse(j.dump()); });
        }
        for (double factor : {0., 0.1, 0.5, 1., 10.}) {
            const auto expected = factor == 1 ? 0.f : float(factor);
            check(suspicion::effective_multiplier(0, factor) == expected);
            check(suspicion::effective_multiplier(2, factor) == float(2 * factor));
            check(suspicion::scaled_delta(20, factor) == float(20 * factor));
            check(suspicion::scaled_delta(-20, factor) == -20);
            check(suspicion::scaled_delta(0, factor) == 0);
        }
        for (double bad :
             {-1., 11., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            reject([&] { suspicion::effective_multiplier(0, bad); });
            reject([&] { suspicion::scaled_delta(20, bad); });
        }
        reject([&] { suspicion::effective_multiplier(std::numeric_limits<double>::infinity(), 1); });
        reject([&] { suspicion::effective_multiplier(std::numeric_limits<double>::max(), 10); });
        reject([&] { suspicion::scaled_delta(std::numeric_limits<double>::max(), 10); });
        std::cout << "PASS " << count << " suspicion configuration/scaling checks\n";
                         return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

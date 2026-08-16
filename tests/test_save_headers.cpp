#include <catch2/catch_test_macros.hpp>

#include "run/save_headers.hpp"

using namespace erstats;

TEST_CASE("synthetic BND4 headers parse per slot") {
    std::vector<SlotHeader> input = {
        SlotHeader{true, 0, "Wolf", 12, 100},
        SlotHeader{true, 2, "Maidenless", 150, 40000},
    };
    const auto bytes = make_synthetic_save(input);
    REQUIRE(starts_with_bnd4(bytes));

    const auto parsed = parse_save_headers(bytes);
    REQUIRE(parsed);
    REQUIRE(parsed->size() == 10);
    REQUIRE((*parsed)[0].active);
    REQUIRE((*parsed)[0].name == "Wolf");
    REQUIRE((*parsed)[0].level == 12);
    REQUIRE((*parsed)[0].playtime_seconds == 100);
    REQUIRE_FALSE((*parsed)[1].active);
    REQUIRE((*parsed)[2].name == "Maidenless");
    REQUIRE((*parsed)[2].level == 150);
}

TEST_CASE("maybe_decrypt_sl2 leaves plaintext BND4 alone") {
    auto bytes = make_synthetic_save({SlotHeader{true, 0, "A", 1, 1}});
    const auto again = maybe_decrypt_sl2(bytes);
    REQUIRE(starts_with_bnd4(again));
}

TEST_CASE("maybe_decrypt_sl2 rejects garbage") {
    std::vector<uint8_t> junk(64, 0xAB);
    REQUIRE(maybe_decrypt_sl2(junk).empty());
}

TEST_CASE("parse_save_headers rejects short buffers") {
    std::vector<uint8_t> tiny = {'B', 'N', 'D', '4', 0, 1, 2, 3};
    REQUIRE_FALSE(parse_save_headers(tiny));
}

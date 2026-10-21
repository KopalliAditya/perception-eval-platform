#include "taxonomy.hpp"

#include "io_util.hpp"

namespace pev {

std::string to_string(ObjectType t) {
    switch (t) {
        case ObjectType::Vehicle: return "TYPE_VEHICLE";
        case ObjectType::Pedestrian: return "TYPE_PEDESTRIAN";
        case ObjectType::Cyclist: return "TYPE_CYCLIST";
        case ObjectType::Sign: return "TYPE_SIGN";
    }
    return "TYPE_VEHICLE";  // unreachable
}

ObjectType object_type_from_string(const std::string& s) {
    if (s == "TYPE_VEHICLE") return ObjectType::Vehicle;
    if (s == "TYPE_PEDESTRIAN") return ObjectType::Pedestrian;
    if (s == "TYPE_CYCLIST") return ObjectType::Cyclist;
    if (s == "TYPE_SIGN") return ObjectType::Sign;
    throw IoError("unknown object type: '" + s + "'");
}

std::string to_string(Difficulty d) {
    switch (d) {
        case Difficulty::Level1: return "LEVEL_1";
        case Difficulty::Level2: return "LEVEL_2";
    }
    return "LEVEL_1";  // unreachable
}

Difficulty difficulty_from_string(const std::string& s) {
    if (s == "LEVEL_1") return Difficulty::Level1;
    if (s == "LEVEL_2") return Difficulty::Level2;
    throw IoError("unknown difficulty level: '" + s + "'");
}

Difficulty difficulty_from_points(int num_points) {
    return num_points >= kLevel1MinPoints ? Difficulty::Level1
                                          : Difficulty::Level2;
}

RangeBin range_bin_for(double range_m) {
    if (range_m < kRangeNearMax) return RangeBin::Near;
    if (range_m < kRangeMidMax) return RangeBin::Mid;
    return RangeBin::Far;
}

std::string to_string(RangeBin b) {
    switch (b) {
        case RangeBin::Near: return "[0,30)m";
        case RangeBin::Mid: return "[30,50)m";
        case RangeBin::Far: return "[50,inf)m";
    }
    return "[0,30)m";  // unreachable
}

}  // namespace pev

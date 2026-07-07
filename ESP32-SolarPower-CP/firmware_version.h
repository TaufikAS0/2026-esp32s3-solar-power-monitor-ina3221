#pragma once

namespace FirmwareInfo {

// Single source of truth for firmware identity.
static constexpr char kVersion[] = "v0.13.1";
static constexpr char kReleaseLabel[] = "web-load-lighter-ina-avg1";
static constexpr char kBuildDate[] = __DATE__;
static constexpr char kBuildTime[] = __TIME__;
static constexpr char kBuildStamp[] = __DATE__ " " __TIME__;

}  // namespace FirmwareInfo

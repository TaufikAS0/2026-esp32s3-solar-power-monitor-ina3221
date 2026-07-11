#pragma once

namespace FirmwareInfo {

// Single source of truth for firmware identity.
static constexpr char kVersion[] = "v0.16.22";
static constexpr char kReleaseLabel[] = "bypass-manual-override-fix";
static constexpr char kBuildDate[] = __DATE__;
static constexpr char kBuildTime[] = __TIME__;
static constexpr char kBuildStamp[] = __DATE__ " " __TIME__;

}  // namespace FirmwareInfo
